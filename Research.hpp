#ifndef PLURIBUS_RESEARCH_H
#define PLURIBUS_RESEARCH_H

#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <mutex>
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <random>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include "Single.hpp"
#include "Task.hpp"
#include "TaskQueue.hpp"
#include "Evaluate.hpp"

using namespace std;

//下注类型
enum E_NN_ACT {
    NN_ACT_UNKNOWN     = 0x0001,  //未知
    NN_ACT_FOLD        = 0x0010,  //弃牌
    NN_ACT_PASS        = 0x0020,  //过牌
    NN_ACT_FOLLOW      = 0x0040,  //跟注
    NN_ACT_RAISE       = 0x0080,  //加注
    NN_ACT_ALLIN       = 0x0100,  //全下
    NN_ACT_SMALL_BLIND = 0x0200,  //小盲
    NN_ACT_BIG_BLIND   = 0x0400,  //大盲
};
//
class Pluribus;
//
class Research  {
public:
    //
    Research(const Research &obj) = delete;
    //
    Research();
    //
    ~Research();
    //
    bool init();
    //
    bool final();
    //
    bool kill();
    //
    bool wait();
    //
    size_t iterations();
    //
    size_t processed();
    //
    void search(int iterations = 100);
    //
    bool depthLimitedSearch(State *state);
    //
    void setResearchPlayer(int player);
    //
    InfoNode *findAgentNode(const int stage, const std::string &infoSet);
    //
    Pluribus *getPluribus();

private:
    //
    void consume();
    //
    void clearTaskQueue();
    //
    void waitThreadsIdle();
    //
    void waitTaskQueueIdle();
    //
    void parallelDiscount(double factor);
    //
    void parallelStrategy();

public:
    //
    void parallelLoad();
    //
    void parallelSave();
    //
    bool isParallelLoaded();
    //
    TaskQueue &getTaskQueue();

private:
    //
    Pluribus *pluribus;
    //
    std::atomic<bool> consumeRunning;
    //
    std::unordered_map<std::thread::id, std::thread *> consumers;
    //
    mutable std::mutex mtxConsumers;
    //
    std::atomic<uint32_t> maxIterations;
    //
    std::atomic<uint32_t> numIterations;
    //
    std::atomic<uint32_t> numProcessed;
    //
    std::atomic<uint32_t> numTaskThread;
    //
    std::unordered_map<std::string, std::atomic<bool>> thredadStatus;
    //
    std::mutex mtxReseach;

private:
    //
    TaskQueue tqResearch;
};

#endif // !PLURIBUS_RESEARCH_H
