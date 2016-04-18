#ifndef MSGTYPE_H
#define MSGTYPE_H

// login
const int MSG_LOGIN = 0;
const int MSG_LOGIN_FAIL = 1;
const int MSG_LOGIN_SUCC = 2;

// register
const int MSG_REGIS = 3;
const int MSG_REGIS_SUCC = 4;
const int MSG_REGIS_FAIL = 5;

// send message to friend
const int MSG_SENDTOFRIEND = 6;
const int MSG_SENDTOFRIEND_FAIL = 7;
const int MSG_SENDTOFRIEND_SUCC = 8;

// send message to group
const int MSG_SENDTOGROUP = 9;
const int MSG_SENDTOGROUP_SUCC = 10;
const int MSG_SENDTOGROUP_FAIL = 11;

// add friend
const int MSG_ADDFRIEND = 12;
const int MSG_ADDFRIEND_WAIT = 13;
const int MSG_ADDFRIEND_SUCC = 14;
const int MSG_ADDFRIEND_FAIL = 15;
const int MSG_ADDFRIEND_NONAME = 16;

// initial client's friend list
const int MSG_INITFRIEND_ONLINE = 17;
const int MSG_INITFRIEND_OFFLINE = 18;
const int MSG_INITGROUP = 31;

// build group
const int MSG_BUILDGROUP = 19;
const int MSG_BUILDGROUP_SUCC = 20;
const int MSG_BUILDGROUP_FAIL = 21;

// invite member
const int MSG_GROUPINVIMEM = 22;
const int MSG_GROUPINVIMEM_WAIT = 23;
const int MSG_GROUPINVIMEM_SUCC = 24;
const int MSG_GROUPINVIMEM_FAIL = 25;
const int MSG_GROUPINVIMEM_NONAME = 26;

// request enter group
const int MSG_REQENTERGROUP = 27;
const int MSG_REQENTERGROUP_WAIT = 28;
const int MSG_REQENTERGROUP_SUCC = 29;
const int MSG_REQENTERGROUP_FAIL = 30;
const int MSG_REQENTERGROUP_NONAME = 32;

const short MSG_DOWNLOAD_FILE = 33;
const short MSG_UPLOAD_FILE = 34;
const short MSG_RELOAD_FILE = 35;

const short MSG_ECHO = 4567;


// const int MSG_LOGOUT = 500;

// // Error type ...
// const int E_LOGIN = 1000;

// // buffuer size ...
// const int BUFFSIZE = 256;
// const int MSGSIZE = 512;
// const int NAMESIZE = 64;
// const int TIMESIZE = 64;

// const int GROUPMEMSIZE = 8;


#endif // MSGTYPE_H