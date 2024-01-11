#ifndef OFFLINEMSGMODEL_H
#define OFFLINEMSGMODEL_H
#include<string>
#include<vector>

namespace ccy
{

class OfflineMsgModel{
public:
    void insert(int userid, std::string msg);
    void remove(int userid);
    std::vector<std::string> query(int userid);

};

}
#endif