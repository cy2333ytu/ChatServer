#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include"group.h"
#include<string>
#include<vector>

namespace ccy
{
    class GROUPMODEL_H
    {
        public:
            bool createGroup(Group &group);
            void addGroup(int userid, int groupid, std::string role);
            std::vector<Group> queryGroups(int userid);
            std::vector<int> queryGroupUsers(int userid, int groupid);
    };

}

#endif