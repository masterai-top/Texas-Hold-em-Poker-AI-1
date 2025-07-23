#include "SessionHandle.h"

static int64_t unixTimestamp() {
    time_t raw_time;
    time(&raw_time);
    struct tm *pttm = gmtime(&raw_time); //获取CST/GMT时间
    // TESTF_D("timestamp：%ld", mktime(pttm));
    return (int64_t)mktime(pttm);
}
static std::string getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    static const int MAX_BUFFER_SIZE = 128;
    char timestamp_str[MAX_BUFFER_SIZE];
    time_t sec = static_cast<time_t>(tv.tv_sec);
    int ms = static_cast<int>(tv.tv_usec) / 1000;

    struct tm tm_time;
    localtime_r(&sec, &tm_time);
    static const char *formater = "%02d:%02d:%02d.%03d";
    int wsize = snprintf(timestamp_str, MAX_BUFFER_SIZE, formater,
                        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, ms);

    timestamp_str[min(wsize, MAX_BUFFER_SIZE - 1)] = '\0';
    return std::string(timestamp_str);
}

#include "Research.hpp"
SessionHandle::SessionHandle(struct bufferevent *bev, Research *r):
    mBufferEvent(bev), mResearch(r) {
    mReadBuffer.clear();
    mWriteBuffer.clear();
    //
    if (nullptr != mBufferEvent) {
        fd = bufferevent_getfd(mBufferEvent);
    }
    //
    mLastAccessTime = 0;
    //
    std::random_device mRd;
    mActionEng = std::mt19937(mRd());
    //
    mDesc.insert(make_pair("FOLD",   "Fold"));
    mDesc.insert(make_pair("CHECK",  "Check"));
    mDesc.insert(make_pair("CALL",   "Call"));
    mDesc.insert(make_pair("ALLIN",  "AllIn"));
    mDesc.insert(make_pair("RAISE1", "0.33x"));
    mDesc.insert(make_pair("RAISE2", "0.50x"));
    mDesc.insert(make_pair("RAISE3", "0.75x"));
    mDesc.insert(make_pair("RAISE4", "1.00x"));
    mDesc.insert(make_pair("RAISE5", "1.50x"));
    mDesc.insert(make_pair("RAISE6", "2.00x"));
}

SessionHandle::~SessionHandle() {
    mReadBuffer.clear();
    mWriteBuffer.clear();
    mBufferEvent = nullptr;
    mResearch = nullptr;
    fd = -1;
}

std::string SessionHandle::toString() {
    std::string str = "SessionHandle";
    return str;
}

SessionBuffer *SessionHandle::getReadBuffer() {
    return &mReadBuffer;
}

SessionBuffer *SessionHandle::getWriteBuffer() {
    return &mWriteBuffer;
}

void SessionHandle::dispatch() {
    size_t headLen = sizeof(tagMsgHeader);
    while (true) {
        if (getReadBuffer()->bufSize() < headLen) {
            break;
        }
        //
        struct MsgHeader header;
        header.UnpackFromArray(getReadBuffer()->buffer(), headLen);
        TEST_D(header.ToString());
        //
        if (header.m_nLen < headLen) {
            TESTF_E("Data length exception: header.m_nLen=%d", header.m_nLen);
            shutdown();
            break;
        }
        //
        int maxPackSize = 32768;
        if (header.m_nLen >= maxPackSize) {
            TESTF_E("Packet body too large: body.m_nLen=%d", header.m_nLen);
            shutdown();
            break;
        }
        //
        if ((header.m_nCmd < alg::ALG_CMD_NODE_REGISER_REQ) || (header.m_nCmd > alg::ALG_CMD_ROBOT_DECIDE_RSP)) {
            TESTF_E("Packet command word error: body.m_nCmd=%d", header.m_nCmd);
            shutdown();
            break;
        }
        //
        if (getReadBuffer()->bufSize() < header.m_nLen) {
            TEST_D("Packet incomplete: bufSize=" << getReadBuffer()->bufSize() << ", packLen=" << header.m_nLen);
            break;
        }
        //
        process(&header, getReadBuffer()->buffer(headLen), header.m_nLen - headLen);
        //
        getReadBuffer()->recycle(header.m_nLen);
    }
}

bool SessionHandle::process(struct MsgHeader *header, char *data, size_t size) {
    std::string strHeader = header->ToString();
    TESTF_D("(%d) process %s", fd, strHeader.c_str());
    //
    switch (header->m_nCmd) {
    case alg::ALG_CMD_NODE_REGISER_REQ:
        handleRegister(header->m_nUid, data, size);
        break;
    case alg::ALG_CMD_KEEPALIVE_REQ:
        handleKeeplive(header->m_nUid, data, size);
        break;
    case alg::ALG_CMD_ROBOT_DECIDE_REQ:
        handleRobotDecide2(header->m_nUid, data, size);
        break;
    default:
        TESTF_E("(%d) unknown command: %d", fd, header->m_nCmd);
        break;
    }
    //
    updateAccessTime();
    return true;
}

size_t SessionHandle::send(const char *data, const size_t size) {
    if (!data || size <= 0) {
        TESTF_E("(%d) invalid data: data=%p, size=%ld", fd, data, size);
        return 0;
    }
    //
    if (!mBufferEvent) {
        TESTF_E("(%d) invalid data: mBufferEvent=%p", fd, mBufferEvent);
        return 0;
    }
    //
    size_t result = bufferevent_write(mBufferEvent, data, size);
    if (result == 0) {
        TESTF_D("(%d) send done(succ): sent=%ld, result=%ld", fd, size, result);
        return size;
    }
    //
    TESTF_E("(%d) send done(fail): sent=%ld, result=%ld", fd, size, result);
    return 0;
}

size_t SessionHandle::sendData() {
    if (0 == getWriteBuffer()->bufSize()) {
        TEST_D("No data to send!");
        return 0;
    }
    //
    size_t sent = send(getWriteBuffer()->buffer(), getWriteBuffer()->bufSize());
    //
    getWriteBuffer()->recycle(sent, 1);
    return 0;
}

void SessionHandle::shutdown() {
    TESTF_D("(%d) shutdown socket.", fd);
    mReadBuffer.clear();
    mWriteBuffer.clear();
    bufferevent_free(mBufferEvent);
    delete this;
}

void SessionHandle::updateAccessTime() {
    mLastAccessTime = unixTimestamp();
}

bool SessionHandle::isDisabled() {
    if (0 == mLastAccessTime) {
        return false;
    }
    //
    static int64_t interval = 60 * 1000;
    int64_t diff = unixTimestamp() - mLastAccessTime;
    if (diff >= interval) {
        return true;
    }
    //
    return false;
}

int SessionHandle::packSend(const int64_t uid, const int cmd, const char *data, const size_t size) {
    MsgHeader headr(size, cmd, uid, 0);
    headr.ToNetOrder();
    getWriteBuffer()->append((const char *)(&headr), sizeof(headr));
    getWriteBuffer()->append(data, size);
    TESTF_D("(%d) pack send data: cmd=%d, uid=%ld, sent=%ld", fd, cmd, uid, size);
    return 0;
}

int SessionHandle::handleRegister(const int64_t uid, const char *data, const size_t size) {
    alg::AlgNodeRegisterReq req;
    req.ParseFromArray(data, size);
    //
    std::string strRsp;
    alg::AlgNodeRegisterRsp rsp;
    rsp.set_result(100);
    rsp.SerializeToString(&strRsp);
    return packSend(uid, alg::ALG_CMD_NODE_REGISER_RSP, strRsp.c_str(), strRsp.size());
}

int SessionHandle::handleKeeplive(const int64_t uid, const char *data, const size_t size) {
    alg::AlgKeepAliveReq req;
    req.ParseFromArray(data, size);
    //
    std::string strRsp;
    alg::AlgKeepAliveRsp rsp;
    rsp.set_timestamp(time(NULL));
    rsp.SerializeToString(&strRsp);
    return packSend(uid, alg::ALG_CMD_KEEPALIVE_RSP, strRsp.c_str(), strRsp.size());
}

int SessionHandle::getLegalActions2(int pot_size, int call_size, int stage_beted, int flop_raises, int remain_chips, int stage_index, int last_raise, vector<std::string> &actions) {
    int mRealRaise = Configure::GetInstance().mRealRaise;
    int bet_033 = (int)floor((pot_size + call_size) * 0.33);
    int bet_050 = (int)floor((pot_size + call_size) * 0.50);
    int bet_075 = (int)floor((pot_size + call_size) * 0.75);
    int bet_100 = (int)floor((pot_size + call_size) * 1.00);
    int bet_150 = (int)floor((pot_size + call_size) * 1.50);
    int bet_200 = (int)floor((pot_size + call_size) * 2.00);

    if(mRealRaise == 0)
    {
        bet_033 = (int)floor((pot_size ) * 0.33);
        bet_050 = (int)floor((pot_size ) * 0.50);
        bet_075 = (int)floor((pot_size ) * 0.75);
        bet_100 = (int)floor((pot_size ) * 1.00);
        bet_150 = (int)floor((pot_size ) * 1.50);
        bet_200 = (int)floor((pot_size ) * 2.00);
    }
    if ((stage_beted + call_size == 2) && (stage_index == 1)) {
        bet_033 = 4 - stage_beted;
        bet_050 = 6 - stage_beted;
        bet_075 = 8 - stage_beted;
        bet_100 = 10 - stage_beted;
    }
    //
    int idx = 0;
    //
    if (call_size != 0) {
        actions.push_back("FOLD");
        idx |= 1 << 1;
        actions.push_back("CALL");
        idx |= 1 << 3;
    } else {
        actions.push_back("CHECK");
        idx |= 1 << 2 ;
    }
    //
    if ((flop_raises < 3) || (stage_index == 1)) {
        int mRaiseRule = Configure::GetInstance().mRaiseRule;
        if (0 == mRaiseRule) {
            //if ((remain_chips >= bet_033 + call_size) && (call_size == 0 || (call_size != 0 && (stage_beted + bet_033 + call_size) >= (stage_beted + call_size) * 2))) {
            //    actions.push_back("RAISE1");
            //    idx |= 1 << 4 ;
            //}
            //
            if ((remain_chips >= bet_050 + call_size) && (call_size == 0 || (call_size != 0 && (stage_beted + bet_050 + call_size) >= (stage_beted + call_size) * 2))) {
                actions.push_back("RAISE2");
                idx |= 1 << 5 ;
            }
            //
            if ((remain_chips >= bet_075 + call_size) && (call_size == 0 || (call_size != 0 && (stage_beted + bet_075 + call_size) >= (stage_beted + call_size) * 2))) {
                actions.push_back("RAISE3");
                idx |= 1 << 6;
            }
            //
            if ((remain_chips >= bet_100 + call_size) && (call_size == 0 || (call_size != 0 && (stage_beted + bet_100 + call_size) >= (stage_beted + call_size) * 2))) {
                actions.push_back("RAISE4");
                idx |= 1 << 7;
            }
        } else if (1 == mRaiseRule) {
            if ((remain_chips >= bet_033 + call_size) && (bet_033 > last_raise)) {
                actions.push_back("RAISE1");
                idx |= 1 << 4 ;
            }
            //
            if ((remain_chips >= bet_050 + call_size) && (bet_050 > last_raise)) {
                actions.push_back("RAISE2");
                idx |= 1 << 5 ;
            }
            //
            if ((remain_chips >= bet_075 + call_size) && (bet_075 > last_raise)) {
                actions.push_back("RAISE3");
                idx |= 1 << 6;
            }
            //
            if ((remain_chips >= bet_100 + call_size) && (bet_100 > last_raise)) {
                actions.push_back("RAISE4");
                idx |= 1 << 7;
            }
        } else if (2 == mRaiseRule) {
            if (mRealRaise == 1){
           
                if (remain_chips >= bet_033 + call_size) {
                    actions.push_back("RAISE1");
                    idx |= 1 << 4 ;
                }
                //
                if (remain_chips >= bet_050 + call_size) {
                    actions.push_back("RAISE2");
                    idx |= 1 << 5 ;
                }
                //
                if (remain_chips >= bet_075 + call_size) {
                    actions.push_back("RAISE3");
                    idx |= 1 << 6;
                }
                //
                if (remain_chips >= bet_100 + call_size) {
                    actions.push_back("RAISE4");
                    idx |= 1 << 7;
                }
            }
            else{
                if (remain_chips >= bet_033 ) {
                    actions.push_back("RAISE1");
                    idx |= 1 << 4 ;
                }
                //
                if (remain_chips >= bet_050 ) {
                    actions.push_back("RAISE2");
                    idx |= 1 << 5 ;
                }
                //
                if (remain_chips >= bet_075 ) {
                    actions.push_back("RAISE3");
                    idx |= 1 << 6;
                }
                //
                if (remain_chips >= bet_100 ) {
                    actions.push_back("RAISE4");
                    idx |= 1 << 7;
                }

            }
        }
        //
        actions.push_back("ALLIN");
        idx |= 1 << 10;
    }
    //
    return idx;
}

int SessionHandle::handleRobotDecide2(const int64_t uid, const char *data, const size_t size, bool debug) {
    alg::AlgRobotActionReq req;
    req.ParseFromArray(data, size);
    TEST_I("@Req: " << req.DebugString());
    //
    auto t1 = std::chrono::high_resolution_clock::now();
    //
    int sbSize = req.small_blind();
    int bbSize = req.big_blind();
    int numPlayers = Configure::GetInstance().mNumPlayers;
    //
    int mRealRaise = Configure::GetInstance().mRealRaise;

    int64_t gid = req.run_game_id();
    if ((1 != sbSize) || (2 != bbSize)) {
        TEST_E("Blind information error");
        return -1;
    }
    //
    std::ostringstream oss;
    oss << "ProtobufCards=";
    //
    int turnIndex = -1;
    std::vector<string> holeCards;
    std::vector<string> holeCards1;
    std::vector<int> discardPlayers;
    int numSeats = req.seat_info_size();
    //
    int bbPos = 1;
    int seatOffset = numPlayers - numSeats;
    TEST_I("@@@@ seatOffset=" << seatOffset << ", bbPos=" << bbPos << ", numPlayers=" << numPlayers);
    //
    //exit(0);
    for (int i = 0; i < numSeats; i++) {
        auto si = req.seat_info(i);
        //
        if (si.robot_guid() == req.uid()) {
            turnIndex = i;
            for (auto j = 0; j < si.hole_card_size(); j++ ) {
                auto c = si.hole_card(j);
                auto s = Card::cardToStr(c);
                holeCards.push_back(s);
                oss << s;
            }
        }
        //
        if (si.is_fold()) {
            int newPos = i;
            if (newPos > bbPos) {
                newPos = newPos + seatOffset;
            }
            //
            discardPlayers.emplace_back(newPos);
        }
    }
    oss << " ";
    //
    if ((-1 == turnIndex) || (2 != holeCards.size())) {
        TEST_F("Invalid: turn=" << turnIndex << ", holeCards.size=" << holeCards.size());
        return -1;
    }
    //
    std::ostringstream oss1;
    oss1 << "discardPlayersPosition:";
    for (auto i = 0; i < discardPlayers.size(); i++) {
        oss1 << " " << std::to_string(discardPlayers[i]);
    }
    TEST_D(oss1.str());
    //
    bool isSucceed = true;
    auto state = new State(numPlayers);
    state->numSeats = numSeats;
    state->boardCards.clear();
    //
    auto pEngine = state->getEngine();
    if (nullptr == pEngine) {
        TEST_E("pEngine is nullptr!");
        return -1;
    }
    //
    int numBoradCards = req.board_card_size();
    for (int i = 0; i < numBoradCards; i++) {
        auto c = req.board_card(i);
        auto s = Card::cardToStr(c);
        state->boardCards.push_back(s);
        oss << s;

    }
    //
    if (turnIndex > bbPos) {
        turnIndex = turnIndex + seatOffset;
    }
    //
    for (int i = 0; i < seatOffset ; i++) {
        state->Command("FOLD", false);
    }
    //
    //
    int history_size = req.history_size();
    if(history_size < 10)//for turn test
        return NULL;
    //if(history_size < 13) //for flop test
    //	return 0;


    if(history_size == 10) // for turn test
    //if(history_size == 13) // for river test 
    {
        TEST_I("Start discards");
        //pEngine->distributeCards2();
	testboardCards.clear();
        state->boardCards.clear();
        holeCards.clear();
        holeCards.push_back("As");
        holeCards.push_back("Kh");
        holeCards1.push_back("Kc");
        holeCards1.push_back("Ac");

        testboardCards.push_back("2d");
        testboardCards.push_back("2c");
        testboardCards.push_back("2s");
        testboardCards.push_back("4d");
        testboardCards.push_back("4c");
        TEST_I("Cards" <<holeCards[0] << holeCards[1] <<holeCards1[0] << holeCards1[1]<<state->boardCards[0] << state->boardCards[1]<<state->boardCards[2] << state->boardCards[3] << "turnIndex" << turnIndex);
        std::unordered_map<int, vector<string>> hands;
        hands[0] = holeCards;
        hands[1] = holeCards1;
        testhands = hands;

    }
    //distributeCards5(const std::unordered_map<int, vector<string>> &hands, const std::vector<string> &boards) {
    //WARNING!!!!!!!!!!!!!!!!!!!!
    TEST_D("before: isShuffle=" << pEngine->getShuffle());
    pEngine->setShuffle(0);
    TEST_D("behind: isShuffle=" << pEngine->getShuffle());
    //

    for (int i = 0; i < numPlayers; i++) {
        state->handRanges[i].clear();
        Evaluate::GetInstance().cloneHandProbs(state->handRanges[i]);
    }



    //
    int potSize = 0;
    int preflopRaises = 0;
    int flopRaises = 0;
    int lastRaise = 0;
    std::vector<int> vRemains(numPlayers, 0);
    std::vector<int> vRoundBet(4, 0);
    std::vector<vector<int>> vRoundCall(4, vector(numPlayers, 0));
    std::vector<vector<int>> vBetting(4, vector(numPlayers, 0));
    //
    for (int i = 0 ; i < numSeats; i++) {
        auto info = req.seat_info(i);
        int newPos = i;
        if (i > bbPos)
            newPos = i + seatOffset;
        //
        vRemains[newPos] = info.init_chips();
    }
    //

    //history_size = 0;
    for (int i = 0; i < history_size; i++) {
        TEST_D("<--------- step " << i << "--------->");
        //
        auto his = req.history(i);
        TEST_D("@HistoryLine: pos=" << his.pos()
               << ", act="   << his.act()
               << ", bet="   << his.bet()
               << ", pot="   << his.pot()
               << ", uid="   << his.guid()
               << ", chips=" << his.chips());
        auto round = his.round();
        //
        int newPos = his.pos();
        if (his.pos() > bbPos) {
            newPos = his.pos() + seatOffset;
        }
	int his_bet = his.bet();
	int his_pot = his.pot();
        if (i == 14)
            his_bet = 32;
            his_pot = his_pot; 

        // Game statistics
        if (true) {
            //
	    potSize += his_bet;
            // Player's remaining chips
            vRemains[newPos] = his.chips();
            // Player's call size
            vRoundCall[round][newPos] = vRoundBet[round] - vBetting[round][newPos];
            // The size of each player's bet

	    vBetting[round][newPos] += his_bet;
            // Record the cumulative maximum bet per round
            int totalBeted = vBetting[round][newPos];
            if (vRoundBet[round] < totalBeted) {
                vRoundBet[round] = totalBeted;
            }
        }
        //
        if (i > bbPos) {
            if (state->isTerminal()) {
                TEST_E("@Fuck you(0)!!!!");
                isSucceed = false;
                break;
            }
            //
            string action = "";
            if ((0 == round) && (0 == preflopRaises)) {
                int mov = his.act();
                if (NN_ACT_FOLD == mov) {
                    action = "FOLD";
                } else if (NN_ACT_PASS == mov) {
                    action = "CHECK";
                } else if (NN_ACT_FOLLOW == mov) {
                    action = "CALL";
                } else if (NN_ACT_ALLIN == mov) {
                    action = "ALLIN";
                    preflopRaises++;
                } else {
                    int betting[3] = { 6, 8, 10};
                    int beted = vBetting[round][newPos];
                    int tag = 0;
                    int dif = abs(beted - betting[0]);
                    for (int idx = 1; idx < 3; idx++) {
                        int bet = betting[idx];
                        if (bet == beted) {
                            tag = idx;
                            dif = 0;
                            break;
                        }
                        //
                        int tmp = abs(beted - betting[idx]);
                        if (dif >= tmp) {
                            tag = idx;
                            dif = tmp;
                        }
                    }
                    if (tag == 0)
                        action = "RAISE2";
                    else if (tag == 1)
                        action = "RAISE3";
                    else if (tag == 2)
                        action = "RAISE4";
                    else if (tag == 3)
                        action = "RAISE5";
                    else if (tag == 4)
                        action = "RAISE6";
                    //
                    if (-1 == tag) {
                        isSucceed = false;
                        TEST_E("tag invalid!!: beted=" << beted);
                        break;
                    }
                    //
                    preflopRaises++;
                }
            } else {
                int mov = his.act();
                if (NN_ACT_FOLD == mov) {
                    action = "FOLD";
                } else if (NN_ACT_PASS == mov) {
                    action = "CHECK";
                } else if (NN_ACT_FOLLOW == mov) {
                    action = "CALL";
                } else if (NN_ACT_RAISE == mov) {
                    int callSize = vRoundCall[round][newPos];
                    float factor = (his_bet - callSize) * 100 / (his_pot + callSize) / 100.0;
                    if(mRealRaise == 0)
		    {
                        factor = (his_bet) * 100 / (his_pot) / 100.0;
		    }
                    // {0.33, 0.50, 0.75, 1.00, 1.50, 2.00}
                    float betting[4] = {0.00, 0.50, 0.75, 1.00};
                    lastRaise = his_bet;
                    //
                    int tag = 0;
                    float dif = fabsf(factor - betting[tag]);
                    for (int idx = 1; idx < 4; idx++) {
                        float ratio = betting[idx];
                        if (ratio == factor) {
                            tag = idx;
                            dif = 0.0;
                            break;
                        }
                        //
                        float tmp = fabsf(factor - ratio);
                        if (dif >= tmp) {
                            tag = idx;
                            dif = tmp;
                        }
                    }
                    //
                    if (0 == round)
                        preflopRaises++;
                    else
                        flopRaises++;
                    //
                    if (0 == tag)
                        action = "CALL";
                    else if (1 == tag)
                        action = "RAISE2";
                    else if (2 == tag)
                        action = "RAISE3";
                    else if (3 == tag)
                        action = "RAISE4";
                    else if (4 == tag)
                        action = "RAISE5";
                    else if (5 == tag)
                        action = "RAISE6";
                    //
                    TEST_I("factor=" << factor << ", act=" << action << ", dif=" << dif << ", callSize=" << callSize);
                } else if (NN_ACT_ALLIN == mov) {
                    action = "ALLIN";
                    //
                    if (0 == round)
                        preflopRaises++;
                    else
                        flopRaises++;
                }
            }
            //
            bool isDiscard = false;
            auto found = std::find(discardPlayers.begin(), discardPlayers.end(), newPos);
            if (found != discardPlayers.end()) {
                isDiscard = true;
            }
            //
            int succHandNum = 0;
            int succNodeNum = 0;
            // Evaluate the range of hands
            //if (!isDiscard && (action != "FOLD") and history_size > 10 and i > 9 ) { // flop test
            if (!isDiscard && (action != "FOLD") and history_size > 13 and i > 12 ) {  //turn test
                //prob player do a action Ａ
                double probA = 0;
                //
                auto defaultRatio = DEFAULT_PROBS;
                auto mapHands = &state->handRanges[newPos];
                TEST_T("************** begin" << state->getTurnIndex());
                std::vector<string> vRemoves;
                for (auto iter = mapHands->begin(); iter != mapHands->end(); iter++) {
                    std::vector<string> vHands;
                    StringUtils::split((*iter).first, "-", vHands);
                    if (2 != vHands.size()) {
                        TEST_E("invalid data: vHands=" << (*iter).first);
                        isSucceed = false;
                        break;
                    }
                    //
                    if (!state->boardCards.empty()) {
                        auto found1 = std::find(state->boardCards.begin(), state->boardCards.end(), vHands[0]);
                        auto found2 = std::find(state->boardCards.begin(), state->boardCards.end(), vHands[1]);
                        if ((found1 != state->boardCards.end()) || (found2 != state->boardCards.end())) {
                            vRemoves.push_back((*iter).first);
                            continue;
                        }
                    }
                    //
                    succHandNum++;
                    //
                    double prob = 0.0;
                    std::vector<double> probs;
                    std::vector<string> actions;
		    if(round == 0)
                        state->boardCards =std::vector<string>();
		    else if(round == 1 )
                        state->boardCards = std::vector<string>(testboardCards.begin(), testboardCards.begin() + 3 );
		    else if(round == 2 )
                        state->boardCards = std::vector<string>(testboardCards.begin(), testboardCards.begin() + 4 );
		    else if(round == 3 )
                        state->boardCards = std::vector<string>(testboardCards.begin(), testboardCards.begin() + 5 );

                    int res = Evaluate::GetInstance().eval(state->infoSet(), vHands, state->boardCards,mResearch+round, actions, probs);
                    if (0 == res) {
                        //
                        succNodeNum++;
                        //
                        auto found3 = std::find(actions.begin(), actions.end(), action);
                        if (found3 != actions.end()) {
                            auto i = std::distance(actions.begin(), found3);
                            probA += probs[i] * (*mapHands)[(*iter).first] ;
                            prob = probs[i];
                        }
                        //
                        //if (prob >= defaultRatio) {
                        (*mapHands)[(*iter).first] = prob *  (*mapHands)[(*iter).first];
                        //} else {
                        //    vRemoves.push_back((*iter).first);
                        //}
                    }
		    else
		    {
                        TEST_I("researchindex" << i << "round" << round << "res" << res << state->infoSet());
                        //(mResearch+round)->depthLimitedSearch(state);
			continue;

		    }
                }
                //
                for (auto iter = mapHands->begin(); iter != mapHands->end(); iter++) {
                    if(probA != 0)
                         (*mapHands)[(*iter).first] = (*mapHands)[(*iter).first] / probA ;
                }
                for (auto iter = mapHands->begin(); iter != mapHands->end(); iter++) {
                    if((*mapHands)[(*iter).first] < defaultRatio)
                        vRemoves.push_back((*iter).first);
                }


                //
                for (auto iter = vRemoves.begin(); iter != vRemoves.end(); iter++) {
                    auto found = mapHands->find(*iter);
                    if (found != mapHands->end()) {
                        mapHands->erase(found);
                    }
                }
                //
                TEST_T("vHands.size=" << mapHands->size() << " vRemoves.size=" << vRemoves.size());
                vRemoves.clear();
                // std::ostringstream osHands;
                // for (auto iter = mapHands->begin(); iter != mapHands->end(); iter++) {
                //     osHands << " hands=" << iter->first << "=" << iter->second;
                // }
                // TEST_D("@Probs: Player" << info.pos() << osHands.str());
                TEST_T("************** over" << state->getTurnIndex());
            }
            //
            TEST_D("@Decision: Player" << newPos << " action=" << action
                   << " succHandNum=" << succHandNum
                   << " succNodeNum=" << succNodeNum
                   << " isDiscard=" << isDiscard);
            state->Command(action, false);
        }
    }
    TEST_I("deckoffset"<<to_string(pEngine->deckOffset));
    state->boardCards = testboardCards;
    if (!pEngine->distributeCards5(testhands, testboardCards)) {
        TEST_F("distributeCards3() failed");
        return -1;
    }
    holeCards = testhands[0];
    holeCards1 = testhands[1]; 
    TEST_I("after5Cards" << string(pEngine->getFlopCard()[0]->strCluster()) << string(pEngine->getFlopCard()[1]->strCluster())<<  string(pEngine->getFlopCard()[2]->strCluster()) <<  string(pEngine->getTurnCard()->strCluster())  <<  string(pEngine->getRiverCard()->strCluster())<<"deckoffset"<< to_string( pEngine->deckOffset) );
    
    TEST_I("handRange");
    //for (auto iter = state->handRanges[turnIndex].begin(); iter !=  state->handRanges[turnIndex].end(); iter++) {
    TEST_I("Players0");
    for (auto iter = state->handRanges[0].begin(); iter !=  state->handRanges[0].end(); iter++) {
        TEST_I((*iter).first << (*iter).second );
    }

    TEST_I("Players1");
    for (auto iter = state->handRanges[1].begin(); iter !=  state->handRanges[1].end(); iter++) {
        TEST_I((*iter).first << (*iter).second );
    }

    //if(history_size == 14)
    //    exit(-1);
    //WARNING!!!!!!!!!!!!!!!!!!!!
    pEngine->setShuffle(1);
    pEngine->printHand();
    pEngine->printBoard();
    TEST_D("@isSucceed=" << isSucceed << ", isShuffle=" << pEngine->getShuffle() << " " << oss.str());
    //
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / pow(10, 6);
    TEST_I("@Game state restoration: duration=" << duration2);
    //
    auto infoSet = state->infoSet();
    TEST_I("@isSucceed=" << isSucceed << ", infoSet=" << infoSet);
    int historyStage = -1;
    int boradCardsNum = state->boardCards.size();
    if (holeCards.size() == 2) {
        if (0 == boradCardsNum)
            historyStage = 1;
        else if (3 == boradCardsNum)
            historyStage = 2;
        else if (4 == boradCardsNum)
            historyStage = 3;
        else if (5 == boradCardsNum)
            historyStage = 4;
    } else {
        TEST_E("invalid holde-cards data!!");
    }
    //
    int dumpPbData = Configure::GetInstance().mResearchTestDump;
    if ((state->getRound() != historyStage) || (1 == dumpPbData) || !isSucceed) {
        TEST_E("Game state resolution failed: " << infoSet);
        if (!debug) {
            std::ofstream ofs(infoSet + ".game", ios::binary);
            if (ofs.is_open()) {
                ofs.write(data, size);
                ofs.close();
            }
        }
        // Debug use
        if (!isSucceed) {
            // Closing process
            // exit(-1);
        }
    }
    //
    int round = -1;
    if (2 == holeCards.size()) {
        if (0 == numBoradCards)
            round = 0;
        else if (3 == numBoradCards)
            round = 1;
        else if (4 == numBoradCards)
            round = 2;
        else if (5 == numBoradCards)
            round = 3;
    }
    //
    if (-1 == round) {
        TEST_E("Invalid round: holeCards.size=" << holeCards.size()
               << ", boardCards.size=" << boradCardsNum);
        return -1;
    }
    int newPos = turnIndex;
    TEST_D("newPos=" << newPos << ", curRound=" << round);
    //
    int maxRoundBet = vRoundBet[round];
    int curRoundBet = vBetting[round][newPos];
    int curRemains  = vRemains[newPos];
    int callSize    = maxRoundBet - curRoundBet;
    TEST_D("@PlayerState: maxRoundBet=" << maxRoundBet
           << ", curRoundBet=" << curRoundBet
           << ", curRemains="  << curRemains
           << ", callSize="    << callSize
           << ", potSize="     << potSize);
    //Compatible game
    std::vector<string> validActions;
    if (getLegalActions2(potSize, callSize, curRoundBet, flopRaises, curRemains, newPos, lastRaise, validActions) < 0) {
        TEST_E("getLegalActions() failed");
    }
    //
    int aiAct = 16;
    int aiBet = 0;
    std::string msg = "@infoSet=" + infoSet;
    TEST_I("msgtime=" << getCurrentTime() <<" " << holeCards[0] << holeCards[1] );
    if (true) {
        TEST_D("@@@@@@@@@Calc: uid=" << req.uid() << ", gameId=" << req.run_game_id());
        int regretNum = 0;
        int strategyNum = 0;
        //
        TEST_D("@Succeed: uid=" << req.uid() << ", gameId=" << req.run_game_id());
        //
        std::string proMsg = "";
        std::string rawMsg = "";
        std::string numMsg = "";
        //
        std::unordered_map<std::string, double> strategySum;
        std::unordered_map<std::string, double> strategyAvg;
        std::unordered_map<std::string, double> regretSum;
        //
        bool isResearch = true;
        auto stage = atoi(infoSet.substr(0, 1).c_str());
        /*
        auto node = BluePrint::GetInstance().tree()->findAgentNode(stage, infoSet);
        if (node != nullptr) {
            if (true) {
                strategySum = node->getStrategySum();
                strategyAvg = node->getAverageStrategy();
                regretSum   = node->getRegretSum();
                regretNum   = node->getRegretNumber();
                strategyNum = node->getStrategyNumber();
                //
                msg += "\n@Stat: strategyNum=" + std::to_string(strategyNum) + "  regretNum=" + std::to_string(regretNum);
            }
            //
            if (regretNum < 200) {
                msg += "\n 训练次数为" + std::to_string(regretNum) + ", 采取初始策略";
                isResearch = true;
            } else if (regretNum < 1000) {
                msg += "\n 训练次数为" + std::to_string(regretNum) + ", 节点策略不稳定";
                isResearch = true;
            }
            //
            for (auto iter = strategySum.begin(); iter != strategySum.end(); iter++) {
                TEST_I("@strategySum: action=" << (*iter).first << "\tprobs=" << (*iter).second);
            }
            for (auto iter = strategyAvg.begin(); iter != strategyAvg.end(); iter++) {
                TEST_I("@strategyAvg: action=" << (*iter).first << "\tprobs=" << (*iter).second);
            }

            //
            for (auto iter = regretSum.begin(); iter != regretSum.end(); iter++) {
                TEST_I("@regretSum: action=" << (*iter).first << "\tprobs=" << (*iter).second);
            }
        } else {
            msg = "初始策略(" + infoSet + ")";
            isResearch = true;
        }*/
        //
        // real-time search
	TEST_I("start research " );
	/*
        if (history_size == 10)
            isResearch = false;
        else if (history_size == 12)
	    isResearch = true;
        */	
        
        State::testnumber = 0 ;
        if (isResearch) {
            auto researchStage = Configure::GetInstance().mResearchStage;
            if (isSucceed && Configure::GetInstance().mResearchOpening && (stage >= researchStage)) {
                //
                msg = "实时搜索(" + infoSet + ")";
                //
		auto node = (mResearch+round)->findAgentNode(stage, infoSet);
		
		if (node == nullptr) {
	            TEST_I(" realNode info node empty" << round);
                    (mResearch+round) -> depthLimitedSearch(state);
                    node = (mResearch+round) -> findAgentNode(stage, infoSet);
                    //(mResearch+round) ->getPluribus()->save(false);
 
                }

                /* 
		vector<vector<string>> showlist;
		vector<string> temp;
		temp.push_back("As");
		temp.push_back("Ad");
		showlist.push_back(temp);
		temp.clear();
		temp.push_back("As");
		temp.push_back("Kd");
		showlist.push_back(temp);
		temp.clear();
		temp.push_back("Ks");
		temp.push_back("Kh");
		showlist.push_back(temp);
		temp.clear();
                */
		vector<vector<string>> showlist;
		vector<string> temp;
		temp.push_back("As");
		temp.push_back("Ah");
		showlist.push_back(temp);
	        
                temp.clear();
		temp.push_back("As");
		temp.push_back("Kh");
		showlist.push_back(temp);


		temp.clear();
		temp.push_back("Ks");
		temp.push_back("Kh");
		showlist.push_back(temp);

                std::unordered_map<int, vector<string>> showhand = testhands;
                //showhand
	        /*	
                infoSet = "4-1-1-4-0-KhKs-1-31046-0-0-1-202";
                node = (mResearch+round)->findAgentNode(stage, infoSet);
                TEST_I("resultcard" << "KsKh" << infoSet);
                TEST_I("" << endl);

                if (node == nullptr) {
                    TEST_E("Research failed: infoSet=" << infoSet);
                } else {
                    regretSum.clear();
                    //
                    if (true) {
                        strategySum = node->getStrategySum();
                        strategyAvg = node->getAverageStrategy();
                        regretSum   = node->getRegretSum();
                        regretNum   = node->getRegretNumber();
                        strategyNum = node->getStrategyNumber();
                        //
                        msg += "\n@Stat: strategyNum=" + std::to_string(strategyNum) + "  regretNum=" + std::to_string(regretNum);
                        TEST_I("@Statnum=" << strategyNum <<"@Regretnum" << regretNum);

                    }
                    //
                    for (auto iter = strategySum.begin(); iter != strategySum.end(); iter++) {
                        TEST_I("@strategySum: action=" << (*iter).first << "\tprobs=" << (*iter).second);
                    }

                    TEST_I("" << endl);
                    for (auto iter = strategyAvg.begin(); iter != strategyAvg.end(); iter++) {
                        TEST_I("@strategyAvg: action=" << (*iter).first << "\tprobs=" << (*iter).second);
                    }


                    //
                    for (auto iter = regretSum.begin(); iter != regretSum.end(); iter++) {
                        TEST_I("@regretSum: action=" << (*iter).first << "\tprobs=" << (*iter).second);
                    }
                }
                */

                for(int v = 0; v < showlist.size(); v++ ){

                    //std::unordered_map<int, vector<string>> hands;
                    testhands[0] = showlist[v];

                    if (!pEngine->distributeCards5(testhands, testboardCards)) {
                        TEST_F("distributeCards3() failed");
                        return -1;
                    }
                    infoSet = state->infoSet(); 
                    auto node = (mResearch+round)->findAgentNode(stage, infoSet);
                    TEST_I("resultcard" << showlist[v][0] << showlist[v][1] << infoSet);
                    TEST_I("" << endl);

                    if (node == nullptr) {
                        TEST_E("Research failed: infoSet=" << infoSet);
                    } else {
                        regretSum.clear();
                        //
                        if (true) {
                            strategySum = node->getStrategySum();
                            strategyAvg = node->getAverageStrategy();
                            regretSum   = node->getRegretSum();
                            regretNum   = node->getRegretNumber();
                            strategyNum = node->getStrategyNumber();
                            //
                            msg += "\n@Stat: strategyNum=" + std::to_string(strategyNum) + "  regretNum=" + std::to_string(regretNum);
                            TEST_I("@Statnum=" << strategyNum <<"@Regretnum" << regretNum);

                        }
                        //
                        for (auto iter = strategySum.begin(); iter != strategySum.end(); iter++) {
                            TEST_I("@strategySum: action=" << (*iter).first << "\tprobs=" << (*iter).second);
                        }

                        TEST_I("" << endl);
                        for (auto iter = strategyAvg.begin(); iter != strategyAvg.end(); iter++) {
                            TEST_I("@strategyAvg: action=" << (*iter).first << "\tprobs=" << (*iter).second);
                        }


                        //
                        for (auto iter = regretSum.begin(); iter != regretSum.end(); iter++) {
                            TEST_I("@regretSum: action=" << (*iter).first << "\tprobs=" << (*iter).second);
                        }
                    }
                }

	    }else {
                TEST_D("real-time search close: isSucceed=" << isSucceed);
                isResearch = false;
            }
        }
        //
        std::vector<double> probabilities;
        probabilities.clear();
        //
        std::vector<std::string> actions;
        actions.clear();
        //
        if (!Configure::GetInstance().mResearchOpening || !isResearch) {
            // Initial strategy
            if (regretNum < 200) {
                strategySum.clear();
                TEST_I("@startinitRegret");
                strategySum = state->initialRegret();
                for (auto iter = strategySum.begin(); iter != strategySum.end(); iter++) {
                    TEST_I("@initialRegret: action=" << (*iter).first << "\tprobs=" << (*iter).second);
                }
            } else {
                double weightSum = 0;
                for (auto iter = strategySum.begin(); iter != strategySum.end(); iter++) {
                    weightSum += (*iter).second;
                }
                //
                auto found = strategySum.find("ALLIN");
                if (found != strategySum.end()) {
                    double weight = strategySum["ALLIN"] / weightSum;
                    if (weight < 0.1) {
                        strategySum["ALLIN"] = 0.0;
                    }
                }
            }
        }
        //
        if (strategySum.empty()) {
            int numValidActions = validActions.size();
            for (auto i = 0; i < numValidActions; i++) {
                actions.push_back(validActions[i]);
                probabilities.push_back(1.0 / (float)numValidActions);
            }
        } else {
            for (auto map : strategySum) {
                auto found = std::find(validActions.begin(), validActions.end(), map.first);
                if (found == validActions.end()) {
                    TEST_E("@@@@Action mismatch, action=" << map.first);
                    TEST_E("@@@@validActions.size=" << validActions.size() << " strategySum.size=" << strategySum.size());
                    continue;
                }
                //
                actions.push_back(map.first);
                //
                if (map.second > 0.0) {
                    probabilities.push_back(map.second);
                } else {
                    probabilities.push_back(0.0);
                }
            }
        }
        //
        auto sumProbs = std::accumulate(probabilities.begin(), probabilities.end(), 0.0);
        if (0.0 >= sumProbs) {
            TEST_E("node error");
        }
        //
        rawMsg = "@Node: ";
        proMsg = "@Prob: ";
        for (int v = 0; v < probabilities.size(); v++) {
            std::stringstream ss0;
            ss0 << mDesc[actions[v]] + "=";
            ss0 << std::setiosflags(std::ios::fixed) << std::setprecision(2) << probabilities[v];
            ss0 << "  ";
            rawMsg += ss0.str();
            //
            probabilities[v] = probabilities[v] / sumProbs;
            //
            std::stringstream ss1;
            ss1 << mDesc[actions[v]] + "=";
            ss1 << std::setiosflags(std::ios::fixed) << std::setprecision(2) << probabilities[v];
            ss1 << "  ";
            proMsg += ss1.str();
        }
        //
        if (!probabilities.empty()) {
            std::discrete_distribution<int> random_choice(probabilities.begin(), probabilities.end());
            auto action = actions[random_choice(mActionEng)];
            TEST_D("@@@@@@@@@@ random_choice:\t" << mDesc[action] << " " << callSize);
            //
            int firstBet = 0;
            if ((0 == preflopRaises) && (1 == historyStage)) {
                firstBet = 1;
            }
            //
            if (action == "FOLD") {
                aiAct = 16;
                aiBet = 0;
            } else if (action == "CHECK") {
                aiAct = 32;
                aiBet = 0;
            } else if (action == "CALL") {
                if (curRemains  == callSize) {
                    aiAct = 256;
                    aiBet = callSize;
                } else {
                    aiAct = 64;
                    aiBet = callSize;
                }
            } else if (action == "ALLIN") {
                aiAct = 256;
                aiBet = curRemains;
            } else if (action == "RAISE1") {//0.33x
                aiAct = 128;
                if (firstBet)
                    aiBet = 2 * bbSize - (maxRoundBet - callSize);
                else{
	            if(mRealRaise == 1 )
                        aiBet = callSize + (int)floor((potSize + callSize) * 0.33);
		    else
                        aiBet = (int)floor((potSize ) * 0.33);
		}
                //
                if (aiBet < bbSize)
                    aiBet = bbSize;
            } else if (action == "RAISE2") {//0.50x
                aiAct = 128;
                if (firstBet)
                    aiBet = 3 * bbSize - (maxRoundBet - callSize);
                else{
	            if(mRealRaise == 1 )
                        aiBet = callSize + (int)floor((potSize + callSize) * 0.5);
		    else
                        aiBet = (int)floor((potSize ) * 0.5);
		}
                //
                if (aiBet < bbSize)
                    aiBet = bbSize;
            } else if (action == "RAISE3") {//0.75x
                aiAct = 128;
                if (firstBet)
                    aiBet = 4 * bbSize - (maxRoundBet - callSize);
                else{
	            if(mRealRaise == 1 )
                        aiBet = callSize + (int)floor((potSize + callSize) * 0.75);
		    else
                        aiBet = (int)floor((potSize ) * 0.75);

		}
                //
                if (aiBet < bbSize)
                    aiBet = bbSize;
            } else if (action == "RAISE4") {//1.00x
                aiAct = 128;
                if (firstBet)
                    aiBet = 5 * bbSize - (maxRoundBet - callSize);
                else{
	            if(mRealRaise == 1 )
                        aiBet = callSize + (int)floor((potSize + callSize) * 1);
		    else
                        aiBet = (int)floor((potSize ) * 1);

		}
                //
                if (aiBet < bbSize)
                    aiBet = bbSize;
            } else if (action == "RAISE5") {//1.50x
                aiAct = 128;
                if (firstBet)
                    aiBet = 5 * bbSize - (maxRoundBet - callSize);
                else{
	            if(mRealRaise == 1 )
                        aiBet = callSize + (int)floor((potSize + callSize) * 1.5);
		    else
                        aiBet = (int)floor((potSize ) * 1.5);


		}
                //
                if (aiBet < bbSize)
                    aiBet = bbSize;
            } else if (action == "RAISE6") {//2.00x
                aiAct = 128;
                if (firstBet)
                    aiBet = 10 * bbSize - (maxRoundBet - callSize);
                else{
                    //aiBet = callSize + (int)floor((potSize + callSize) * 2.00);
		    if(mRealRaise == 1 )
                        aiBet = callSize + (int)floor((potSize + callSize) * 2);
		    else
                        aiBet = (int)floor((potSize ) * 2);

	
		}
                //
                if (aiBet < bbSize)
                    aiBet = bbSize;
            }
            //
            numMsg += "@Choice: betAct=" + mDesc[action] + " betSize=" + std::to_string(aiBet);
            //
            msg += "\n" + proMsg;
            msg += "\n" + rawMsg;
            msg += "\n" + numMsg;
        } else {
            std::stringstream ss0;
            ss0 << "@ValidActions:";
            for (auto iter = validActions.begin(); iter != validActions.end(); iter++) {
                ss0 << " " << mDesc[*iter];
            }
            TEST_E("Decision making failure!!");
            msg += "\n" + infoSet + "@Decision making failure!!";
            msg += "\n" + ss0.str();
        }
    }
    //
    if (nullptr != state) {
        delete state;
        state = nullptr;
    }
    //
    auto t3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t1).count() / pow(10, 6);
    TEST_I("@Decision result: duration=" << duration3);
    //

    std::string strRsp;
    alg::AlgRobotActionRsp rsp;
    rsp.set_result(0);
    rsp.set_uid(req.uid());
    rsp.set_run_game_id(req.run_game_id());
    rsp.set_odds(1);
    rsp.set_bet_size(aiBet);
    rsp.set_action(aiAct);
    rsp.set_info_msg(msg);
    rsp.SerializeToString(&strRsp);
    TEST_I("@Rsp: " << rsp.DebugString());
    //if(history_size == 13)
    //    exit(0);
    return packSend(uid, alg::ALG_CMD_ROBOT_DECIDE_RSP, strRsp.c_str(), strRsp.size());
}
