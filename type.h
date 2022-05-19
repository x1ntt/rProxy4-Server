#ifndef _TYPE_H
#define _TYPE_H

#include <cstdint>

typedef unsigned short  us16;
typedef unsigned int    us32;

typedef us16    PackSize;
typedef us16    Sid;
typedef us16    Cid;
typedef us16    CmdId;

const us16 RECV_MAX = 4096;
const us32 MAX_BUFFER_SIZE = 1 * 1024 * 1024;
const Sid MAX_SID = UINT16_MAX;
const Sid MIN_SID = 1;


#endif