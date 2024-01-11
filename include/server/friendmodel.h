#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include "user.h"
#include <vector>

namespace ccy
{

    class FriendModel
    {
    public:
        void insert(int userid, int friendid);
        std::vector<User> query(int suerid);
    };

}

#endif