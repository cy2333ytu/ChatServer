#include "../include/server/chatservice.h"
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
        _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, std::placeholders::_1,
                                                    std::placeholders::_2, std::placeholders::_3)});

        _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3)});

        _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, std::placeholders::_1,
                                                       std::placeholders::_2, std::placeholders::_3)});
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

                conn->send(resposne.dump());
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
        _offlineMsgModel.insert(toid, js.dump());
    }
    void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
    {
        int userid = js["id"].get<int>();
        int friendid = js["friendid"].get<int>();

        _friendModel.insert(userid, friendid);
    }
}