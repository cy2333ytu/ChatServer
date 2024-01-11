#include"chatserver.h"
#include<iostream>

int main(){
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ccy::ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    
    return 0;
}