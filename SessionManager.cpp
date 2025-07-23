#include "SessionManager.h"

SessionManager::SessionManager() {

}

SessionManager::~SessionManager() {

}

//
SessionManager *SessionManager::GetInstance() {
    static SessionManager _instance;
    return &_instance;
}

//
void SessionManager::CheckSession() {
    std::vector<int64_t> v;
    if (true) {
        std::unique_lock<std::shared_mutex>  l(mLock);
        for (auto iter = mSessionMap.begin(); iter != mSessionMap.end(); iter++) {
            auto handler = (*iter).second;
            if (nullptr != handler) {
                // if (handler->isDisabled()) {
                //     v.push_back((*iter).first);
                // }
            }
        }
    }
    //
    for (auto iter = v.begin(); iter != v.end(); iter++) {
        delSession(*iter);
    }
    v.clear();
}

//
bool SessionManager::addSession(const int64_t sessinId, SessionHandle *handler) {
    if ((sessinId <= 0) || (nullptr == handler)) {
        return false;
    }
    //
    std::unique_lock<std::shared_mutex>  l(mLock);
    auto iter = mSessionMap.find(sessinId);
    if (iter != mSessionMap.end()) {
        return false;
    }
    //
    mSessionMap.insert(std::make_pair(sessinId, handler));
    return true;
}

//
bool SessionManager::delSession(const int64_t sessinId) {
    if (sessinId <= 0) {
        return false;
    }
    //
    std::unique_lock<std::shared_mutex>  l(mLock);
    auto iter = mSessionMap.find(sessinId);
    if (iter != mSessionMap.end()) {
        mSessionMap.erase(iter);
        return true;
    }
    //
    return false;
}

//
SessionHandle *SessionManager::findSession(const int64_t sessinId) {
    if (sessinId <= 0) {
        return nullptr;
    }
    //
    std::unique_lock<std::shared_mutex> l(mLock);
    auto iter = mSessionMap.find(sessinId);
    if (iter != mSessionMap.end()) {
        return (*iter).second;
    }
    //
    return nullptr;
}