#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <event.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#ifndef _WIN32
#include <netinet/in.h>
#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif
#include <sys/socket.h>
#endif
//
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/event-config.h>
//
#ifdef EVENT__HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
//
#include "Log.hpp"
#include "Deploy.hpp"
#include "SessionManager.h"
#include "SessionHandle.h"
#include "RedisCluster.hpp"
#include "ShareMem.hpp"
#include "Pluribus.hpp"
#include "Research.hpp"
//
#include "Game.hpp"
#include "Configure.hpp"
//
#include "simulator.h"
//
#include "State.hpp"
//
Research *gResearch = nullptr;
/*
void initResearch(){
for (int i =0; i <  4; i++ ){
        
   }
}*/
//
static void listener_callback(struct evconnlistener *, evutil_socket_t, struct sockaddr *, int, void *);
static void conn_write_callback(struct bufferevent *, void *);
static void conn_read_callback(struct bufferevent *, void *);
static void conn_event_callback(struct bufferevent *, short, void *);
static void signal_callback(evutil_socket_t, short, void *);
//
static struct timeval lastTime;
static int eventIsPersistent = 0;
//
static void timer_callback(evutil_socket_t fd, short event, void *arg) {
    struct timeval newTime;
    auto timeout = (struct event *)arg;
    evutil_gettimeofday(&newTime, NULL);
    //
    // struct timeval difference;
    // evutil_timersub(&newtime, &lasttime, &difference);
    // double elapsed = difference.tv_sec + (difference.tv_usec / 1.0e6);
    // TESTF_T("timer_callback called at %d: %.3f seconds elapsed.", (int)newtime.tv_sec, elapsed);
    //
    lastTime = newTime;
    //
    SessionManager::GetInstance()->CheckSession();
    //
    if (!eventIsPersistent) {
        struct timeval tv;
        evutil_timerclear(&tv);
        tv.tv_sec = 15;
        event_add(timeout, &tv);
    }
}

//
static int deploy_main(int port) {
    auto base = event_base_new();
    if (!base) {
        TESTF_E("Could not initialize libevent");
        return 1;
    }
    //
    struct sockaddr_in sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    auto listener = evconnlistener_new_bind(
                        base, listener_callback, (void *)base,
                        LEV_OPT_REUSEABLE | LEV_OPT_LEAVE_SOCKETS_BLOCKING | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE,
                        -1, (struct sockaddr *)&sin, sizeof(sin));
    //
    if (!listener) {
        TESTF_E("Could not create a listener!");
        return 1;
    }
    TESTF_I("server listen(%d)...", port);
    //
    auto signal_event = evsignal_new(base, SIGINT, signal_callback, (void *)base);
    if (!signal_event || event_add(signal_event, NULL) < 0) {
        TESTF_E( "Could not create/add a signal event!\n");
        return 1;
    }
    //
    if (true) {
        //Initialize one event
        struct event timeout;
        event_assign(&timeout, base, -1, EV_PERSIST, timer_callback, (void *) &timeout);
        //
        struct timeval tv;
        evutil_timerclear(&tv);
        tv.tv_sec = 2;
        event_add(&timeout, &tv);
        evutil_gettimeofday(&lastTime, NULL);
        //
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);
    }
    //
    event_base_dispatch(base);
    //
    evconnlistener_free(listener);
    event_free(signal_event);
    event_base_free(base);
    //
    TESTF_I("Server exited");
    return 0;
}

//
static void listener_callback(
    struct evconnlistener *listener,
    evutil_socket_t fd,
    struct sockaddr *sa,
    int socklen,
    void *user_data) {
    auto base = (struct event_base *)user_data;
    auto bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE);
    if (!bev) {
        TESTF_E("Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }
    //
    auto handle = new SessionHandle(bev, gResearch);
    bufferevent_setcb(bev, conn_read_callback, conn_write_callback, conn_event_callback, (void *)handle);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
}

//
static void conn_read_callback(
    struct bufferevent *bev,
    void *user_data) {
    auto fd = bufferevent_getfd(bev);
    char data[8192] = {0};
    size_t size = sizeof(data);
    //
    while (true) {
        auto input = bufferevent_get_input(bev);
        if (evbuffer_get_length(input) == 0) {
            break;
        }
        //
        memset(data, 0, size);
        size_t ret = bufferevent_read(bev, data, size);
        if (ret > 0) {
            auto handle = (SessionHandle *)user_data;
            if (handle) {
                TESTF_T("(%d) conn_read_callback: recv_num=%ld", fd, ret);
                handle->getReadBuffer()->append(data, ret);
                handle->dispatch();
                handle->sendData();
            }
        }
    }
}

//
static void conn_write_callback(
    struct bufferevent *bev,
    void *user_data) {
    auto fd = bufferevent_getfd(bev);
    auto *handle = (SessionHandle *)user_data;
    if (handle) {
        std::string str = handle->toString();
        TESTF_T("(%d) conn_write_callback: %s", fd, str.c_str());
    }
    //
    auto output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
        // bufferevent_disable(bev, EV_WRITE);
    }
}

//
static void conn_event_callback(
    struct bufferevent *bev,
    short events,
    void *user_data) {
    auto fd = bufferevent_getfd(bev);
    if (events & BEV_EVENT_EOF) {
        TESTF_T("(%d) Connection closed.", fd);
    } else if (events & BEV_EVENT_ERROR) {
        TESTF_T("(%d) Got an error on the connection: %s", fd, strerror(errno));
    } else {
        TESTF_T("(%d) Unknow event.", fd);
    }
    //
    auto handle = (SessionHandle *)user_data;
    if (handle) {
        delete handle;
        handle = nullptr;
    }
    //
    bufferevent_free(bev);
}

//
static void signal_callback(
    evutil_socket_t sig,
    short events,
    void *user_data) {
    auto base = (struct event_base *)user_data;
    struct timeval delay = { 3, 0 };
    TESTF_T("Caught an interrupt signal; exiting cleanly in three seconds.");
    event_base_loopexit(base, &delay);
}

//
Deploy::Deploy() {
    pair_init();
}
//
Deploy::~Deploy() {
    if (nullptr != gResearch) {
        delete[] gResearch;
        gResearch = nullptr;
    }
}

bool Deploy::init() {
    TESTF_I("@deploy init(begin).");
    //
    Evaluate::GetInstance().init();
    //
    BluePrint::GetInstance().init();
    //
    if (nullptr == gResearch) {
        gResearch = new Research[4];
        for(int b = 0 ; b < 4 ; b++)
            gResearch[b].init();
    }
    //
    State::initDefault();
    //
    TESTF_I("@deploy init(over).");
    return true;
}

bool Deploy::loop() {
    TESTF_I("@deploy loop begin.");
    //
    if (nullptr == gResearch) {
        TEST_E("gResearch is nullptr");
        return false;
    }
    //
    deploy_main(Configure::GetInstance().mResearchPort);
    TESTF_I("@deploy loop over.");
    return true;
}

bool Deploy::final() {
    TESTF_I("@deploy final(begin).");
    //
    if (nullptr != gResearch) {
        // gResearch->save(false);
    }
    //
    Evaluate::GetInstance().final();
    //
    BluePrint::GetInstance().final();
    //
    TESTF_I("@deploy final(over).");
    return true;
}

bool Deploy::test1() {
    std::string infoSet = Configure::GetInstance().mResearchTestInfoset;
    std::ifstream ifs(infoSet + ".game", std::ios::binary);
    if (ifs.is_open()) {
        ifs.seekg(0, std::ios::end);
        //
        int length = ifs.tellg();
        TEST_D("fileSize=" << length);
        //
        ifs.seekg(0, ifs.beg);
        //
        char *buffer  = new char[length];
        if (buffer) {
            try {
                ifs.read(buffer, length);
                TEST_D("readSzie=" << length);
            } catch (std::ios_base::failure f) {
                TEST_E(f.what() << " Characters extracted: " << ifs.gcount());
            } catch (...) {
                TEST_E("Some other error");
            }
            //
            auto sh = new SessionHandle(nullptr, gResearch);
            sh->handleRobotDecide2(9999, buffer, length, true);
            //
            delete[] buffer;
        }
        //
        ifs.close();
    }
    //
    return true;
}
bool Deploy::test6() {
    vector<string> infolist = {"3-0-0-5-1-196-0-10000-0-0-0-1252","3-0-0-5-1-196-1-20024-0-0-0-1258","4-0-0-5-1-163-0-21000-0-0-0-1252","4-0-0-5-1-163-1-31024-0-0-0-1258"};

    auto sh = new SessionHandle(nullptr, gResearch);

    for(int c = 0 ; c < infolist.size() ; c++){

        //std::string infoSet = Configure::GetInstance().mResearchTestInfoset;
	string infoSet = infolist[c];
	printf("infoset:%s",infoSet.c_str());
        std::ifstream ifs(infoSet + ".game", std::ios::binary);
        if (ifs.is_open()) {
            ifs.seekg(0, std::ios::end);
            //
            int length = ifs.tellg();
            TEST_D("fileSize=" << length);
            //
            ifs.seekg(0, ifs.beg);
            //
            char *buffer  = new char[length];
            if (buffer) {
                try {
                    ifs.read(buffer, length);
                    TEST_D("readSzie=" << length);
                } catch (std::ios_base::failure f) {
                    TEST_E(f.what() << " Characters extracted: " << ifs.gcount());
                } catch (...) {
                    TEST_E("Some other error");
                }
                //
                sh->handleRobotDecide2(9999, buffer, length, true);
                //
                delete[] buffer;
            }
            //
            ifs.close();
        }

    }
    return true;
}
bool Deploy::test2() {
    int mNumPlayers = Configure::GetInstance().mNumPlayers;
    //
    auto state = new State(mNumPlayers);
    state->numSeats = mNumPlayers;
    state->boardCards.clear();
    //
    if (mNumPlayers != state->getEngine()->getPlayerNum()) {
        TEST_E("invalid getPlayerNum");
        return false;
    }
    //
    for (int i = 0; i < mNumPlayers; i++) {
        state->handRanges[i].clear();
        Evaluate::GetInstance().cloneHandProbs(state->handRanges[i]);
    }
    //
    for (int turnIndex = 0; turnIndex < mNumPlayers; turnIndex++) {
        int ret = Evaluate::dealCards(turnIndex, state->boardCards, state->handRanges, state->mEngine);
        if (ret < 0) {
            TEST_E("Evaluate::dealCards() failed: turnIndex=" << turnIndex << ", ret=" << ret);
            return false;
        }
    }
    //
    return true;
}

bool Deploy::test3() {
    int mNumPlayers = Configure::GetInstance().mNumPlayers;
    //
    auto state = new State(mNumPlayers);
    state->numSeats = mNumPlayers;
    state->boardCards.clear();
    //
    for (int i = 0; i < mNumPlayers; i++) {
        state->handRanges[i].clear();
        Evaluate::GetInstance().cloneHandProbs(state->handRanges[i]);
    }
    //
    gResearch->setResearchPlayer(state->getTurnIndex());
    gResearch->depthLimitedSearch(state);
    gResearch->kill();
    gResearch->wait();
    return true;
}

bool Deploy::test4() {
    int mNumPlayers = Configure::GetInstance().mNumPlayers;
    //
    auto state = new State(mNumPlayers);
    state->numSeats = mNumPlayers;
    state->boardCards.clear();
    //
    if (mNumPlayers != state->getEngine()->getPlayerNum()) {
        TEST_E("invalid getPlayerNum");
        return false;
    }
    //
    for (int i = 0; i < mNumPlayers; i++) {
        state->handRanges[i].clear();
        Evaluate::GetInstance().cloneHandProbs(state->handRanges[i]);
    }
    //
    std::vector<vector<string>> vPair;
    if (-1 == Evaluate::GetInstance().cloneHandPairs(vPair)) {
        TEST_E("vPair error!");
        return false;
    }
    //
    int trainTimes = 1326;
    for (int x = 0; x < trainTimes; x++) {
        for (int turnIndex = 0; turnIndex < mNumPlayers; turnIndex++) {
            int ret = Evaluate::dealCards3(x, state->boardCards, state->handRanges, state->mEngine);
            if (ret < 0) {
                TEST_E("Evaluate::dealCards() failed: turnIndex=" << turnIndex << ", ret=" << ret);
                return false;
            }
        }
    }
    //
    return true;
}

bool Deploy::test5() {
    clock_t start = clock();
    //
    int sampleSize = 10000;
    int unknowPlayers = 1;
    //
    Simulator sim;
    //
    auto results = sim.compute_probabilities(sampleSize, {"4c", "5c", "6c"}, {{"2c", "3c"}}, unknowPlayers);
    printf("id  win  tie \n");
    int size = results.size();
    for (int i = 0; i < size; i++) {
        printf("%d  %5.2f\n", i, results[i] * 100.0 / sampleSize);
    }
    //
    clock_t stop = clock();
    double elapsed = (double)(stop - start) / CLOCKS_PER_SEC;
    printf("\nTime elapsed: %.5fs\n", elapsed);
    return true;
}
/*
bool Deploy::test6() {
    auto infoSet = "2-3-1-2-1-83-0-10000-0-0-1-1252";
    auto agent = gResearch->getPluribus()->getAgent(2, infoSet);
    if (nullptr == agent) {
        TEST_E("invalid agent: infoSet=" << infoSet);
        return false;
    }
    //
    return true;
}*/
