/* ************************************************************************
> File Name:     test.c
> Author:        Yunzhe Su
> Created Time:  Thu 21 Dec 2023 11:24:28 AM CST
> Description:   
 ************************************************************************/
#include <json.hpp>
// #include <json/json.h>
#include<iostream>

std::string func(){
    
    nlohmann::json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    // std::cout<<js<<std::endl;
    std::string recbuf = js.dump();
    return recbuf;
}

int main(){
    std::string recv = func();
    nlohmann::json jsrecv = nlohmann::json::parse(recv);
    std::cout<<jsrecv["msg_type"]<<std::endl;
    return 0;
}