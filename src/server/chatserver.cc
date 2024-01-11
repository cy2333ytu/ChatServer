#include "chatserver.h"
#include "chatservice.h"
#include "../thirdparty/json.hpp"

#include <functional>
#include <string>

using json = nlohmann::json;
namespace ccy
{

    ChatServer::ChatServer(EventLoop *loop,
                           const InetAddress &listenAddr,
                           const string &nameArg)
        : _server(loop, listenAddr, nameArg)
    {
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, std::placeholders::_1));
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this,
                                        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _server.setThreadNum(4);
    }

    void ChatServer::start()
    {
        _server.start();
    }

    void ChatServer::onConnection(const TcpConnectionPtr &conn)
    {
        if (!conn->connected())
        {
            ChatService::instance()->clientCloseException(conn);
            conn->shutdown();
        }
    }
    void ChatServer::onMessage(const TcpConnectionPtr &conn,
                               Buffer *buffer,
                               Timestamp time)
    {
        std::string buf = buffer->retrieveAllAsString();
        json js = json::parse(buf);

        auto msgHandler = ChatService::instance()->getHandler(js["msgId"].get<int>());
        // callback msg bind eventHandler to execute event
        msgHandler(conn, js, time);
    }
}