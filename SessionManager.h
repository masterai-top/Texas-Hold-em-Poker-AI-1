#ifndef __SESSION_MANAGAER_H__
#define __SESSION_MANAGAER_H__

#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <shared_mutex>
#include <iostream>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <chrono>
#include <set>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
//
#include "Log.hpp"
#include "SessionHandle.h"

using namespace std;

class SessionManager {
public:
    //
    static SessionManager *GetInstance();
    //
    void CheckSession();
    //
    bool addSession(const int64_t sessinId, SessionHandle *handler);
    //
    bool delSession(const int64_t sessinId);
    //
    SessionHandle *findSession(const int64_t sessinId);

private:
    //
    SessionManager();
    //
    ~SessionManager();
    //
    SessionManager(const SessionManager &);
    //
    SessionManager &operator=(const SessionManager &);

private:
    //
    std::unordered_map<int64_t, SessionHandle *> mSessionMap;
    //
    std::shared_mutex mLock;
};

#endif



