#include "SessionBuffer.h"
#include "Log.hpp"

SessionBuffer::SessionBuffer(): mBuffer(nullptr) {
    //
    mCapacity = 128 * 1024;
    mBuffer = (char *)malloc(mCapacity);
    //
    clear();
}

SessionBuffer::~SessionBuffer() {
    if (mBuffer) {
        free(mBuffer);
    }
}

void SessionBuffer::clear() {
    std::unique_lock<std::shared_mutex> l(mLock);
    mHead = 0;
    mTail = 0;
}

bool SessionBuffer::append(const char *data, const size_t size) {
    std::unique_lock<std::shared_mutex> l(mLock);
    if (size > mCapacity) {
        TESTF_E("append error. mCapacity=%ld, nSize=%ld", mCapacity, size);
        return false;
    }
    //
    while (true) {
        size_t remain = mCapacity - mTail;
        if (remain >= size) {
            memcpy(&mBuffer[mTail], data, size);
            mTail = mTail + size;
            break;
        }
        //
        if (0 == remain) {
            TESTF_E("append error. mCapacity=%ld, nSize=%ld", mCapacity, size);
            return false;
        }
        //
        memmove(&mBuffer[0], &mBuffer[mHead], remain);
        mHead = 0;
        mTail = remain;
    }
    //
    TESTF_T("append succ. nSize=%ld, nLen=%ld", size, mTail - mHead);
    return true;
}

bool SessionBuffer::recycle(const size_t len, const int flags) {
    if (len == 0) {
        return false;
    }
    //
    std::unique_lock<std::shared_mutex> l(mLock);
    size_t diff = mTail - mHead;
    if (diff < len) {
        TESTF_E("recycle error. len=%ld, nDiff=%ld", len, diff);
        return false;
    }
    //
    mHead = mHead + len;
    TESTF_T("recycle succ. nFlags=%d, nSize=%ld, nLen=%ld", flags, len, mTail - mHead);
    return true;
}

void SessionBuffer::SessionBuffer::resetTailOffset(size_t offset) {
    if (true) {
        std::unique_lock<std::shared_mutex> l(mLock);
        mTail = mTail + offset;
    }
}

void SessionBuffer::resetHeadOffset(size_t offset) {
    if (true) {
        std::unique_lock<std::shared_mutex> l(mLock);
        mHead = mHead + offset;
    }
}

char *SessionBuffer::buffer(size_t offset) {
    char *ptr = nullptr;
    if (true) {
        std::unique_lock<std::shared_mutex> l(mLock);
        ptr = &mBuffer[mHead + offset];
    }
    return ptr;
}

size_t SessionBuffer::bufSize() {
    int val = 0;
    if (true) {
        std::unique_lock<std::shared_mutex> l(mLock);
        val = mTail - mHead;
    }
    return val;
}

size_t SessionBuffer::remainSize() {
    int val = 0;
    if (true) {
        val = mCapacity - mTail;
    }
    return val;
}

size_t SessionBuffer::headOffset() {
    return mHead;
}

size_t SessionBuffer::tailOffset() {
    return mTail;
}

std::string SessionBuffer::toString() {
    std::string str = "SessionBuffer";
    return str;
}
