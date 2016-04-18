#ifndef MSGSTRUCT_H
#define MSGSTRUCT_H

#include <string.h>
#include <netinet/in.h>

// a map for request friend, it record the sender name and invitee name(groupName == NULL)
// or used in group invite/request (groupName != NULL)
// typedef struct FRIENDREQ
// {
//     char sendName[NAMESIZE];
//     char recvName[NAMESIZE];
//     char groupName[NAMESIZE];
// }friendReqMap;

// group information
// typedef struct GROUP
// {
//     char groupName[NAMESIZE];
//     char builder[NAMESIZE];
//     char member[GROUPMEMSIZE][NAMESIZE];
//     size_t memMount;
// }chatGroup;

// typedef struct
// {
//     char user[NAMESIZE];
//     char passwd[NAMESIZE];
//     char groupName[NAMESIZE][NAMESIZE];
//     size_t groupMount;
//     char friendName[NAMESIZE][NAMESIZE];
//     size_t friendMount;
// }userInfo;

// typedef struct
// {
//     char user[NAMESIZE];
//     int fd;
//     struct sockaddr_in sock;
// }userOnline;

// message package
const int TEXTSIZE = 512;
typedef struct
{
    short type;                                   // message type
//    short totalPackNum;                           // total pack num for per message
    char text[TEXTSIZE];                          // main message (can be msg/groupname/ etc .look below ..)
//    char sendTime[TIMESIZE];                    // send time
//    char sendName[NAMESIZE];                    // sender's name
//    char recvName[NAMESIZE];                    // receiver : group or person name
}msgPack;
/*
 *  The special content purpose for member 'msg'
 *  {{
 *      According to 'flag':
 *      1. If the msgPack have no message text ,the 'group name' should be loaded in 'msg'.
 *      2. If there are message text shoul be sent, the 'group name' should be loaded in 'recvName'.
 *
 *  }}
*/


#endif // MSGSTRUCT_H