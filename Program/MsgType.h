#ifndef MSGTYPE_H
#define MSGTYPE_H

// login
const short MSG_LOGIN = 0;
const short MSG_LOGIN_FAIL = 1;
const short MSG_LOGIN_SUCC = 2;

// register
const short MSG_REGIS = 3;
const short MSG_REGIS_SUCC = 4;
const short MSG_REGIS_FAIL = 5;

// send message to friend
const short MSG_SENDTOFRIEND = 6;
const short MSG_SENDTOFRIEND_FAIL = 7;
const short MSG_SENDTOFRIEND_SUCC = 8;

// send message to group
const short MSG_SENDTOGROUP = 9;
const short MSG_SENDTOGROUP_SUCC = 10;
const short MSG_SENDTOGROUP_FAIL = 11;

// add friend
const short MSG_ADDFRIEND = 12;
const short MSG_ADDFRIEND_WAIT = 13;
const short MSG_ADDFRIEND_SUCC = 14;
const short MSG_ADDFRIEND_FAIL = 15;
const short MSG_ADDFRIEND_NONAME = 16;

// initial client's friend list
const short MSG_INITFRIEND_ONLINE = 17;
const short MSG_INITFRIEND_OFFLINE = 18;
const short MSG_INITGROUP = 31;

// build group
const short MSG_BUILDGROUP = 19;
const short MSG_BUILDGROUP_SUCC = 20;
const short MSG_BUILDGROUP_FAIL = 21;

// invite member
const short MSG_GROUPINVIMEM = 22;
const short MSG_GROUPINVIMEM_WAIT = 23;
const short MSG_GROUPINVIMEM_SUCC = 24;
const short MSG_GROUPINVIMEM_FAIL = 25;
const short MSG_GROUPINVIMEM_NONAME = 26;

// request enter group
const short MSG_REQENTERGROUP = 27;
const short MSG_REQENTERGROUP_WAIT = 28;
const short MSG_REQENTERGROUP_SUCC = 29;
const short MSG_REQENTERGROUP_FAIL = 30;
const short MSG_REQENTERGROUP_NONAME = 32;

const short MSG_DOWNLOAD_FILE = 33;
const short MSG_UPLOAD_FILE = 34;
//////////////////////////////
/////////reload控制信息//////////////
const short MSG_RELOAD_FILE = 35;
//////////////////////////////
const short MSG_DELETE_FILE = 36;

const short MSG_ECHO = 4567;
const short MSG_OK = 4568;
const short MSG_ERROR = 4569;




#endif // MSGTYPE_H