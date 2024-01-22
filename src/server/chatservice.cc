#include "../include/server/chatservice.h"
#include "../include/server/redis/redis.h"
#include "../include/server/friendmodel.h"
#include "../include/public.h"
#include <muduo/base/Logging.h>
#include <string>
#include <vector>

namespace ccy
{

    ChatService *ChatService::instance()
    {
        static ChatService service;
        return &service;
    }

    ChatService::ChatService()
    {
        _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
        _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
        _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
        _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
        _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

        _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
        _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
        _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

        if (_redis.connect())
        {
            _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
        }
    }
    void ChatService::reset()
    {
        _userModel.resetState();
    }

    MsgHandler ChatService::getHandler(int msgId)
    {
        auto it = _msgHandlerMap.find(msgId);
        if (it == _msgHandlerMap.end())
        {
            return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
            {
                LOG_ERROR << "msgId: " << msgId << "can't find handler!";
            };
        }
        else
            return _msgHandlerMap[msgId];
    }

    void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int id = js["id"].get<int>();
        std::string pwd = js["password"];

        User user = _userModel.query(id);
        if (user.getId() == id && user.getPwd() == pwd)
        {
            if (user.getState() == "online")
            {
                json resposne;
                resposne["msgid"] = LOGIN_MSG_ACK;
                resposne["errno"] = 2;
                resposne["errmsg"] = "the account has already logged in, please log in again";
                conn->send(resposne.dump());
            }
            else
            { // log success and record connection msg
                {
                    std::lock_guard<std::mutex> lock(_connMutex);
                    _userConnMap.insert({id, conn});
                }
                // log success and refresh user's state
                user.setState("online");
                _userModel.updateState(user);
                json resposne;
                resposne["msgid"] = LOGIN_MSG_ACK;
                resposne["errno"] = 0;
                resposne["id"] = user.getId();
                resposne["name"] = user.getName();
                // query if there is offline msg
                std::vector<string> vec = _offlineMsgModel.query(id);
                if (!vec.empty())
                {
                    resposne["offlinemsg"] = vec;
                    _offlineMsgModel.remove(id);
                }
                std::vector<User> userVec = _friendModel.query(id);
                if (!userVec.empty())
                {
                    std::vector<std::string> vec2;
                    for (User &user : userVec)
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        vec2.emplace_back(js.dump());
                    }
                    resposne["friends"] = vec2;
                }

                std::vector<Group> groupuserVec = _groupModel.queryGroups(id);
                if (!groupuserVec.empty())
                {
                    // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                    vector<string> groupV;
                    for (Group &group : groupuserVec)
                    {
                        json grpjson;
                        grpjson["id"] = group.getId();
                        grpjson["groupname"] = group.getName();
                        grpjson["groupdesc"] = group.getDesc();
                        vector<string> userV;
                        for (GroupUser &user : group.getUsers())
                        {
                            json js;
                            js["id"] = user.getId();
                            js["name"] = user.getName();
                            js["state"] = user.getState();
                            js["role"] = user.getRole();
                            userV.push_back(js.dump());
                        }
                        grpjson["users"] = userV;
                        groupV.push_back(grpjson.dump());
                    }

                    response["groups"] = groupV;
                }

                conn->send(response.dump());
            }
        }
        else
        {
            json resposne;
            resposne["msgid"] = LOGIN_MSG_ACK;
            resposne["errno"] = 1;
            resposne["errmsg"] = "Incorrect username or password";
            conn->send(resposne.dump());
        }
    }

    void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        std::string name = js["name"];
        std::string pwd = js["password"];

        User user;
        user.setName(name);
        user.setPwd(pwd);
        bool state = _userModel.insert(user);

        if (state)
        {
            json resposne;
            resposne["msgid"] = REG_MSG_ACK;
            resposne["errno"] = 0;
            resposne["id"] = user.getId();
            conn->send(resposne.dump());
        }
        else
        {
            json resposne;
            resposne["msgid"] = REG_MSG_ACK;
            resposne["errno"] = 1;
            conn->send(resposne.dump());
        }
    }
    void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int userid = js["id"].get<int>();
        {
            std::lock_guard<std::mutex> lock(_connMutex);
            auto it = _userConnMap.find(userid);
            if (it != _userConnMap.end())
            {
                _userConnMap.erase(it);
            }
        }
        _redis.unsubscribe(userid);
        User user(userid, "", "", "offline");
        _userModel.updateState(user);
    }

    void ChatService::clientCloseException(const TcpConnectionPtr &conn)
    {
        User user;
        {
            std::lock_guard<std::mutex> lock(_connMutex);
            for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
            {
                if (it->second == conn)
                {
                    user.setId(it->first);
                    _userConnMap.erase(it);
                    break;
                }
            }
        }
        _redis.unsubscribe(user.getId());

        if (user.getId() != -1)
        {

            user.setState("offline");
            _userModel.updateState(user);
        }
    }

    void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int toid = js["to"].get<int>();
        {
            std::lock_guard<std::mutex> lock(_connMutex);
            auto it = _userConnMap.find(toid);
            if (it != _userConnMap.end())
            {
                it->second->send(js.dump());
                return;
            }
        }

        User user = _userModel.query(toid);
        if (user.getState() == "online")
        {
            _redis.publish(toid, js.dump());
            return;
        }

        _offlineMsgModel.insert(toid, js.dump());
    }
    void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int userid = js["id"].get<int>();
        int friendid = js["friendid"].get<int>();

        _friendModel.insert(userid, friendid);
    }
    void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int userid = js["id"].get<int>();
        string name = js["groupname"];
        string desc = js["groupdesc"];

        // store new group info
        Group group(-1, name, desc);
        if (_groupModel.createGroup(group))
        {
            // store the info of creator of group
            _groupModel.addGroup(userid, group.getId(), "creator");
        }
    }

    void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int userid = js["id"].get<int>();
        int groupid = js["groupid"].get<int>();
        _groupModel.addGroup(userid, groupid, "normal");
    }
    void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int userid = js["id"].get<int>();
        int groupid = js["groupid"].get<int>();
        vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

        lock_guard<mutex> lock(_connMutex);
        for (int id : useridVec)
        {
            auto it = _userConnMap.find(id);
            if (it != _userConnMap.end())
            {
                // send msg to another
                it->second->send(js.dump());
            }
            else
            {
                // query if online
                User user = _userModel.query(id);
                if (user.getState() == "online")
                {
                    _redis.publish(id, js.dump());
                }
                else
                {
                    // store offlin msg
                    _offlineMsgModel.insert(id, js.dump());
                }
            }
        }
    }
    void ChatService::handleRedisSubscribeMessage(int userid, string msg)
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            it->second->send(msg);
            return;
        }

        // store offline msg
        _offlineMsgModel.insert(userid, msg);
    }
}