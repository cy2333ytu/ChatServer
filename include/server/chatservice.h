#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <functional>
#include <muduo/net/TcpConnection.h>
#include <memory>
#include <mutex>
#include "../thirdparty/json.hpp"
#include "usermodel.h"
#include "friendmodel.h"
#include "offlinemsgmodel.h"

using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

namespace ccy
{

    using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;
    class ChatService
    {
    public:
        static ChatService *instance();
        void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
        void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
        void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // add friend
        void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
        MsgHandler getHandler(int msgId);
        void clientCloseException(const TcpConnectionPtr &conn);
    private:
        ChatService();
        std::unordered_map<int, MsgHandler> _msgHandlerMap;
        std::unordered_map<int, TcpConnectionPtr> _userConnMap;

        std::mutex _connMutex;
        UserModel _userModel;
        OfflineMsgModel _offlineMsgModel;
        FriendModel _friendModel;  
    };

}
#endif