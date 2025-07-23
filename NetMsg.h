#ifndef __NET_MSG_H__
#define __NET_MSG_H__

#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/message.h>

using namespace std;

//
#define htonl64(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohl64(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

//
#pragma pack (push, 1)

typedef struct MsgHeader {
    uint32_t  m_nLen;
    uint32_t  m_nCmd;
    uint64_t  m_nUid;
    uint32_t  m_nErr;
    //
    MsgHeader() {
        memset(this, 0, sizeof(MsgHeader));
    }
    //
    MsgHeader(uint32_t len, uint32_t cmd, uint64_t uid, uint32_t err) {
        m_nLen = len + sizeof(struct MsgHeader);
        m_nCmd = cmd;
        m_nErr = err;
        m_nUid = uid;
    }
    //
    bool UnpackFromArray(const char *pData, size_t nLen) {
        if (!pData || (nLen < sizeof(MsgHeader)))
            return false;
        //
        m_nLen  = ntohl(((MsgHeader *)(pData))->m_nLen);
        m_nCmd  = ntohl(((MsgHeader *)(pData))->m_nCmd);
        m_nUid  = ntohl64(((MsgHeader *)(pData))->m_nUid);
        m_nErr  = ntohl(((MsgHeader *)(pData))->m_nErr);
        return true;
    }
    //
    void ToNetOrder() {
        m_nLen = htonl(m_nLen);
        m_nCmd = htonl(m_nCmd);
        m_nErr = htonl(m_nErr);
        m_nUid = htonl64(m_nUid);
    }
    //
    std::string ToString() {
        static std::ostringstream os;
        os.str("");
        os << "MsgHeader: ";
        os << " Len=" << m_nLen;
        os << " Cmd=" << m_nCmd;
        os << " Uid=" << m_nUid;
        os << " Err=" << m_nErr;
        return os.str();
    }
} tagMsgHeader; //end struct MsgHeader

#pragma pack(pop)

extern std::string ProtobufToString(const google::protobuf::Message &pbMsg);


#endif // __NET_MSG_H__
