#ifndef __SESSION_BUFFER_H__
#define __SESSION_BUFFER_H__

#include <iostream>
#include <string>
#include <cstring>
#include <shared_mutex>

using namespace std;

class SessionBuffer {
public:
    //
    SessionBuffer();
    //
    ~SessionBuffer();
    //
    void clear();
    //
    bool append(const char *data, const size_t size);
    //
    bool recycle(const size_t len, const int flags = 0);
    //
    void resetTailOffset(size_t offset);
    //
    void resetHeadOffset(size_t offset);
    //
    char *buffer(size_t offset = 0);
    //
    size_t bufSize();
    //
    size_t remainSize();
    //
    size_t headOffset();
    //
    size_t tailOffset();
    //
    std::string toString();

private:
    //
    std::shared_mutex mLock;
    //
    size_t mHead;
    //
    size_t mTail;
    //
    size_t mCapacity;
    //
    char *mBuffer;
};

#endif