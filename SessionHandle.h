#ifndef __SESSION_HANDLE_H__
#define __SESSION_HANDLE_H__

#include <iostream>
#include <string>
#include <fstream>
#include "SessionBuffer.h"
//
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/event-config.h>
//
#include "NetMsg.h"
#include "alg.pb.h"
#include "Log.hpp"
#include "Pluribus.hpp"
#include "Research.hpp"
#include "NumPy.hpp"
#include "Configure.hpp"
//
using namespace std;

//
enum E_NN_ACTION {
    NN_ACTION_FOLD  = 16,       // 弃牌
    NN_ACTION_CHECK = 32,       // 看牌
    NN_ACTION_CALL  = 64,       // 跟注
    NN_ACTION_RAISE = 128,      // 加注
    NN_ACTION_ALLIN = 256,      // allin
};

//
class SessionHandle {
public:
    //
    SessionHandle() = delete;
    //
    SessionHandle(SessionHandle &&) = delete;
    //
    SessionHandle(const SessionHandle &) = delete;
    //
    void operator= (const SessionHandle &) = delete;

public:
    //
    SessionHandle(struct bufferevent *bev, Research *r);
    //
    ~SessionHandle();
    //
    std::string toString();
    //
    SessionBuffer *getReadBuffer();
    //
    SessionBuffer *getWriteBuffer();
    //
    void dispatch();
    //
    int getLegalActions2(int pot_size, int call_size, int stage_beted, int flop_raises, int remain_chips, int stage_index, int last_raise, vector<std::string> &actions);
    //
    size_t sendData();

private:
    //
    bool process(struct MsgHeader *header, char *data, size_t size);
    //
    size_t send(const char *data, const size_t size);
    //
    void shutdown();
    //
    void updateAccessTime();
    //
    bool isDisabled();

public:
    //
    int packSend(const int64_t uid, const int cmd, const char *data, const size_t size);
    //
    int handleRegister(const int64_t uid, const char *data, const size_t size);
    //
    int handleKeeplive(const int64_t uid, const char *data, const size_t size);
    //
    int handleRobotDecide2(const int64_t uid, const char *data, const size_t size, bool debug = false);

    std::unordered_map<int, vector<string>> testhands;
    std::vector<string> testboardCards;

private:
    //
    int64_t mLastAccessTime;
    //
    std::shared_timed_mutex mLock;
    //
    SessionBuffer mReadBuffer;
    //
    SessionBuffer mWriteBuffer;
    //
    struct bufferevent *mBufferEvent;
    //
    evutil_socket_t fd;
    //
    std::mt19937 mActionEng;
    //
    Research * mResearch;
    //
    std::map<string, string> mDesc;
};

#endif

