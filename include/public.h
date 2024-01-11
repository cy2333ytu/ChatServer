#ifndef PUBLIC_H
#define PUBLIC_H

namespace ccy
{

    enum EnMsgType
    {
        LOGIN_MSG = 1,
        LOGIN_MSG_ACK,
        REG_MSG,
        REG_MSG_ACK, // register response msg
        ONE_CHAT_MSG, // chat msg
        ADD_FRIEND_MSG, // add friend

        CREATE_GROUP,   // create group
        ADD_GROUP_NSG,  // add a group
        GROUP_CHAT_MSG, 
    };

}

#endif