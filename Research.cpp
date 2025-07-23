#include "Research.hpp"
#include "Pluribus.hpp"
#include "GamePool.hpp"
#include "Configure.hpp"
//
#include "alg.pb.h"

//
Research::Research(): pluribus(nullptr) {
    consumeRunning = true;
    numIterations = 0;
    numProcessed = 0;
    maxIterations = 2000;
}

Research::~Research() {

}

bool Research::init() {
    TEST_I("@Research-init: begin");
    //
    int threadNum = 1;
    if (Configure::GetInstance().mResearchMultiThreading) {
        threadNum = std::thread::hardware_concurrency();
    }
    //
    numIterations = 0;
    numProcessed  = 0;
    TEST_I("Starting point of training: numIterations=" << numIterations
           << ", numProcessed=" << numProcessed);
    //
    if (nullptr == pluribus) {
        pluribus = new Pluribus();
        pluribus->load(true);
    }
    //
    bool isLoadBluePrint = false;
    if (isLoadBluePrint)
        parallelLoad();
    //
    if (true) {
        std::unique_lock<std::mutex> lock(mtxConsumers);
        for (int i = 0; i < threadNum; i++) {
            auto consumer = new std::thread(std::bind(&Research::consume, this));
            consumer->detach();
            consumers.insert({consumer->get_id(), consumer});
            numTaskThread++;
        }
    }
    //
    if (isLoadBluePrint) {
        // TEST_I("TaskSize=" << " " << getTaskQueue().Size());
        while (!isParallelLoaded()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }
    //
    TEST_I("@Research-init: over");
    return true;
}

bool Research::final() {
    if (true) {
        std::unique_lock<std::mutex> lock(mtxConsumers);
        for (auto iter = consumers.begin(); iter != consumers.end(); iter++) {
            delete (*iter).second;
        }
        //
        consumers.clear();
    }
    //
    TEST_D("Research end");
    return true;
}

bool Research::kill() {
    TEST_I("kill reserach...");
    consumeRunning = false;
    return true;
}

bool Research::wait() {
    while (numTaskThread > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    //
    return true;
}

void Research::search(int iterations) {
    if (true) {
        //
        std::unique_lock<std::mutex> lock(mtxReseach);
        //
        TEST_I("@Research: begin");
        auto tt1 = std::chrono::high_resolution_clock::now();
        //
        numIterations = 0;
        numProcessed = 0;
        maxIterations = iterations;
        //
        while (consumeRunning) {
            // Training mission completed
            if (numProcessed == maxIterations) {
                break;
            }
            // Training queue is idle
            waitTaskQueueIdle();
            //
            auto tt3 = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(tt3 - tt1).count();
            if (duration >= Configure::GetInstance().mResearchTimeout) {
                TEST_I("@Research timemout: duration=" << duration);
                clearTaskQueue();
                break;
            }
            //
            if ((numIterations > 0) && (numProcessed > 0)) {
                //
                auto cfrThreshold = Configure::GetInstance().mResearchCfrThreshold;
                if (numProcessed <= cfrThreshold) {
                    auto discountInterval = Configure::GetInstance().mResearchDiscountInterval;
                    if (0 == (numIterations % discountInterval)) {
                        waitTaskQueueIdle();
                        float factor = ((float)numProcessed / (float)discountInterval) / (((float)numProcessed / (float)discountInterval) + 1.);
                        parallelDiscount(factor);
                        waitTaskQueueIdle();
                        TEST_I("@train discount: numProcessed=" << numProcessed << ", factor=" << factor);
                    }
                }
                //
                auto strategyInterval = Configure::GetInstance().mResearchStrategyInterval;
                if (0 == (numIterations % strategyInterval)) {
                    waitTaskQueueIdle();
                    parallelStrategy();
                    waitTaskQueueIdle();
                    TEST_I("@train strategy: numProcessed=" << numProcessed);
                }
            }
            //
            int taskNum = 1;
            int taskDif = maxIterations - numIterations;
            if (taskDif < taskNum) {
                taskNum = taskDif;
            }
            //
            if (taskNum > 0) {
                for (int i = 0; i < taskNum; i++) {
                    Task t;
                    t.mType = TASK_TYPE_RESEARCH;
                    t.mTaskId = ++numIterations;
                    t.mPluribus = pluribus;
                    tqResearch.Push(t);
                }
            }
        }
        //
        auto tt2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tt2 - tt1).count();
        TEST_I("@Research: over, duration=" << duration / pow(10, 6));
    }
}

//
bool Research::depthLimitedSearch(State *state) {
    if (nullptr == state) {
        TEST_E("invalid state");
        return false;
    }
    //
    if (nullptr == pluribus) {
        TEST_E("invalid pluribus");
        return false;
    }
    //
    auto t1 = std::chrono::high_resolution_clock::now();
    //
    bool isReseach = false;
    auto infoSet = state->infoSet();
    int stage = atoi(infoSet.substr(0, 1).c_str());
    if (stage >= Configure::GetInstance().mResearchStage) {
        int researchCount = 10;
        if (stage == 1) {
            researchCount = Configure::GetInstance().mResearchPreflopTimes;
        } else if (stage == 2) {
            researchCount = Configure::GetInstance().mResearchFlopTimes;
        } else if (stage == 3) {
            researchCount = Configure::GetInstance().mResearchTurnTimes;
        } else if (stage == 4) {
            researchCount = Configure::GetInstance().mResearchRiverTimes;
        } else {
            TEST_E("invalid stage: " << stage);
            return false;
        } 

	TEST_D("stage=" << stage << ", researchCount=" << researchCount);
        //
        pluribus->mGameState = state;
        search(researchCount);
        //
        isReseach = true;
    }
    //
    if (!isReseach) {
        TEST_I("@Research: No real-time search!");
        return false;
    }
    //
    if (false) {
        TEST_D("------------------[ hands-ranges-0 ]------------------");
        int numPlayers = Configure::GetInstance().mNumPlayers;
        for (int i = 0; i < numPlayers; i++) {
            auto &mm = state->handRanges[i];
            //
            std::ostringstream os;
            os << "Player" << i;
            for (auto iter = mm.begin(); iter != mm.end(); iter++) {
                os << " " << iter->first;
            }
            //
            TEST_D(os.str());
        }
        //
        TEST_D("------------------[ hands-ranges-1 ]------------------");
    }
    //
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / pow(10, 6);
    //
    auto node = pluribus->findAgentNode(state->getRound(), infoSet);
    if (node == nullptr) {
        TEST_E("@Research: no node. infoSet=" << infoSet << ", isReseach=" << isReseach);
        return false;
    }
    //
    TEST_I("@Research: finish, isReseach=" << isReseach << ", duration=" << duration);
    return true;
}

void Research::consume() {
    TEST_I("consume begined");
    //
    bool isSleep = false;
    while (consumeRunning) {
        if (true) {
            Task task;
            if (tqResearch.Get(task)) {
                //
                task.Process();
                //
                if (task.mType == TASK_TYPE_RESEARCH) {
                    numProcessed++;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    //
    TEST_I("consume exited");
    numTaskThread--;
}

void Research::clearTaskQueue() {
    tqResearch.Clean();
}

size_t Research::iterations() {
    return numIterations;
}

size_t Research::processed() {
    return numProcessed;
}

Pluribus *Research::getPluribus() {
    return pluribus;
}

InfoNode *Research::findAgentNode(const int stage, const std::string &infoSet) {
    return pluribus->findAgentNode(stage, infoSet);
}

TaskQueue &Research::getTaskQueue() {
    return tqResearch;
}

void Research::waitThreadsIdle() {
    while (true) {
        if (numIterations > 0) {
            if (numIterations == numProcessed)
                break;
        }
        //
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Research::waitTaskQueueIdle() {
    while (consumeRunning) {
        if (tqResearch.Empty())
            break;
        //
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Research::setResearchPlayer(int player) {
    TEST_D("set-search-target!!!!!");
    auto pEngine = pluribus->mResearchState.getEngine();
    if (nullptr != pEngine) {
        pEngine->setResearchPlayer(player);
    }
}

void Research::parallelDiscount(double factor) {
    TEST_T("parallel-discount!!!!!");
    int agentSize = pluribus->getAgentSize();
    for (int i = 0; i < agentSize; i++) {
        if (true) {
            Task t;
            t.mType = TASK_TYPE_DISCOUNT;
            t.mFactor = factor;
            t.mAgent = pluribus->getAgentPointer(i);
            t.mPluribus = pluribus;
            tqResearch.Push(t);
        }
    }
}

void Research::parallelStrategy() {
    TEST_T("parallel-strategy!!!!!");
    int agentSize = pluribus->getAgentSize();
    for (int i = 0; i < agentSize; i++) {
        if (true) {
            Task t;
            t.mType = TASK_TYPE_STRATEGY;
            t.mAgent = pluribus->getAgentPointer(i);
            t.mPluribus = pluribus;
            tqResearch.Push(t);
        }
    }
}

void Research::parallelLoad() {
    TEST_T("parallel-load!!!!!");
    auto blueprint = BluePrint::GetInstance().tree();
    if (nullptr == blueprint) {
        TEST_E("invalid blueprint!");
        return;
    }
    //
    int agentSize = blueprint->getAgentSize();
    for (int i = 0; i < agentSize; i++) {
        auto agent = blueprint->getAgentPointer(i);
        if (!agent->isLoaded()) {
            Task t;
            t.mType = TASK_TYPE_LOAD_MODEL;
            t.mAgent = agent;
            t.mPluribus = blueprint;
            tqResearch.Push(t);
        }
    }
}

void Research::parallelSave() {
    TEST_D("parallel-save!!!!!");
    auto blueprint = BluePrint::GetInstance().tree();
    if (nullptr == blueprint) {
        TEST_E("invalid blueprint!");
        return;
    }
    //
    int agentSize = blueprint->getAgentSize();
    for (int i = 0; i < agentSize; i++) {
        auto agent = blueprint->getAgentPointer(i);
        if (!agent->isLoaded()) {
            Task t;
            t.mType = TASK_TYPE_SAVE_MODEL;
            t.mAgent = agent;
            t.mPluribus = blueprint;
            tqResearch.Push(t);
        }
    }
}

bool Research::isParallelLoaded() {
    auto blueprint = BluePrint::GetInstance().tree();
    if (nullptr == blueprint) {
        TEST_E("invalid blueprint!");
        return false;
    }
    //
    int loadedNum = 0;
    int agentSize = blueprint->getAgentSize();
    for (int i = 0; i < agentSize; i++) {
        auto agent = blueprint->getAgentPointer(i);
        if (agent != nullptr) {
            if (agent->isLoaded()) {
                loadedNum++;
            }
        }
    }
    //
    return (loadedNum == agentSize);
}
