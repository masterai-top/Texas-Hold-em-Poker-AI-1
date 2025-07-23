#include "Evaluate.hpp"
#include "Research.hpp"
Evaluate::Evaluate() {

}

Evaluate::~Evaluate() {

}

int Evaluate::init() {
    if (-1 == initCardsList())
        return -1;
    //
    if (-1 == initHandsList())
        return -1;
    //
    return 0;
}

int Evaluate::final() {
    vCards.clear();
    vHands.clear();
    return 0;
}

int Evaluate::eval(const std::string &infoSet,
                   const std::vector<string> &holeCards,
                   const std::vector<string> &boardCards,
                   Research *mResearch,
                   std::vector<string> &actions,
                   std::vector<double> &probabilities) {
    auto tt1 = std::chrono::high_resolution_clock::now();
    //
    if (holeCards.empty()) {
        TEST_E("Invalid param: holeCards");
        return -2;
    }
    //
    std::ostringstream cardStr;
    for (auto iter = holeCards.rbegin(); iter != holeCards.rend(); iter++) {
        cardStr << *iter;
    }
    //
    int cardCluster = -1;
    if (boardCards.empty()) {
        auto card1 = holeCards[0];
        auto card2 = holeCards[1];
        //
        auto suit1 = Card::suitToInt(card1.substr(1, 1));
        auto suit2 = Card::suitToInt(card2.substr(1, 1));
        auto face1 = Card::faceToInt(card1.substr(0, 1));
        auto face2 = Card::faceToInt(card2.substr(0, 1));
        //
        auto mid = 0;
        auto dif = 0;
        if (face1 >= face2) {
            mid = face2 - 2;
            dif = face1 - face2;
        } else {
            mid = face1 - 2;
            dif = face2 - face1;
        }
        //
        if (suit1 == suit2) {
            cardCluster = mid * 13 + mid + dif * 13;
        } else {
            cardCluster = mid * 13 + mid + dif;
        }
    } else {
        for (auto iter = boardCards.begin(); iter != boardCards.end(); iter++) {
            cardStr << *iter;
        }
        //
        cardCluster = NumPy::GetInstance().ShmPointer()->TestCard(cardStr.str());
    }
    //
    if (-1 == cardCluster) {
        TEST_T("***** cardStr=" << cardStr.str());
        return -999;
    }
    //
    std::string tagSplit = "-";
    std::vector<string> vFields;
    StringUtils::split(infoSet, tagSplit, vFields);
    if (vFields.empty()) {
        TEST_E("Invalid data: infoSet=" << infoSet);
        return -4;
    }
    //
    if (12 != vFields.size()) {
        TEST_E("Invalid size: vFields.size=" << vFields.size());
        return -5;
    }
    // replace card-cluster
    vFields[5] = std::to_string(cardCluster);
    std::string newInfoSet = StringUtils::strcat(vFields, tagSplit);
    //
    auto stage = atoi(vFields[0].c_str());
    //auto node = BluePrint::GetInstance().tree()->findAgentNode(stage, newInfoSet);
    auto node = mResearch->getPluribus()->findAgentNode(stage, newInfoSet);
    if (nullptr == node) {
        TEST_T("Invalid node: stage=" << stage << " before=" << infoSet << " behind=" << newInfoSet);
        return -6;
    }
    //
    if (true) {
        auto strategyAvg = node->getAverageStrategy();
        for (auto iter = strategyAvg.begin(); iter != strategyAvg.end(); iter++) {
            actions.emplace_back((*iter).first);
            probabilities.emplace_back((*iter).second);
        }
    }
    //
    return 0;
}

int Evaluate::initCardsList() {
    vCards.clear();
    for (int suit = 0; suit <= 3; suit++) {
        for (int face = 2; face <= 14; face++) {
            vCards.emplace_back(Card::faceToStr(face - 2) + Card::suitToStr(suit));
	    if(face == 13 or face == 14)
                testCards.emplace_back(Card::faceToStr(face - 2) + Card::suitToStr(suit));
        }
    }
    //
    TEST_D("cards: card_num=" << vCards.size());
    return 0;
}



int Evaluate::initHandsList() {
    vHands.clear();
    mProbs.clear();
    //size_t cardNum = vCards.size();
    size_t cardNum = testCards.size();
    for (int i = 0; i < cardNum; i++) {
        for (int j = i + 1; j < cardNum; j++) {

            //std::string pair = vCards[j] + "-" + vCards[i];
            std::string pair = testCards[j] + "-" + testCards[i];
            vHands.emplace_back(pair);
            //
            std::vector<string> pairs;
            pairs.emplace_back(vCards[j]);
            pairs.emplace_back(vCards[i]);
            //
            vPairs.emplace_back(pairs);
            //
            mProbs.emplace(pair, DEFAULT_PROBS);
        }
    }
    //
    TEST_D("Hands: pairs_num=" << vHands.size());
    return 0;
}

int Evaluate::cloneHandPairs(std::vector<vector<string>> &cardPairs) {
    cardPairs.clear();
    cardPairs.insert(cardPairs.begin(), vPairs.begin(), vPairs.end());
    if (cardPairs.size() != vPairs.size()) {
        return -1;
    }
    //
    return 0;
}

int Evaluate::cloneHandProbs(std::unordered_map<string, double> &handsProbs) {
    handsProbs.clear();
    for (auto iter = mProbs.begin(); iter != mProbs.end(); iter++) {
        handsProbs.emplace(iter->first, iter->second);
    }
    //
    if (handsProbs.size() != mProbs.size()) {
        return -1;
    }
    //
    return 0;
}

int Evaluate::dealCards(const int iterations,
                        const std::vector<string> &boardCards,
                        const std::unordered_map<string, double> handRanges[],
                        Game *pEngine) {
    auto tt1 = std::chrono::high_resolution_clock::now();
    TEST_T("**********************************************");
    //
    std::unordered_map<int, std::vector<string>> mHands;
    //
    std::vector<string> dealDards;
    std::vector<string> validHands;
    std::vector<double> validProbs;
    int numPlayers = pEngine->getPlayerNum();
    int turnIndex = pEngine->getTurnIndex();
    int offSeats = numPlayers - turnIndex;
    std:: ostringstream os;
    os << "deal cards: ";
    // behind
    std::vector<int> vPositions;
    for (int i = turnIndex; i < numPlayers; i++) {
        vPositions.emplace_back(i);
        os << " " << std::to_string(i);
    }
    // before
    for (int j = 0; j < turnIndex; j++) {
        vPositions.emplace_back(j);
        os << " " << std::to_string(j);
    }
    TEST_T(os.str());
    //
    for (auto iter = vPositions.begin(); iter != vPositions.end(); iter++) {
        auto i = *iter;
        auto player = pEngine->getPlayer(i);
        if (nullptr == player)
            continue;
        //
        if (player->isFold())
            continue;
        //
        auto &hands = handRanges[i];
        if (hands.empty()) {
            TEST_E("Invalid handParis, Player" << i);
            return -1;
        }
        //
        int beforeNum = hands.size();
        //
        validHands.clear();
        validProbs.clear();
        //
        for (auto iter = hands.begin(); iter != hands.end(); iter++) {
            bool isValid = true;
            //Remove existing public cards
            for (auto iter2 = boardCards.begin(); iter2 != boardCards.end(); iter2++) {
                std::size_t found = (*iter).first.find(*iter2);
                if (found != std::string::npos) {
                    isValid = false;
                }
            }
            //Remove cards that have been dealt
            if (isValid) {
                for (auto iter3 = dealDards.begin(); iter3 != dealDards.end(); iter3++) {
                    std::size_t found = (*iter).first.find(*iter3);
                    if (found != std::string::npos) {
                        isValid = false;
                    }
                }
            }
            //
            if (isValid) {
                validHands.emplace_back((*iter).first);
                validProbs.emplace_back((*iter).second);
            }
        }
        //
        std::random_device mRd;
        std::mt19937 mt = std::mt19937(mRd());
        std::discrete_distribution<> random_choice(validProbs.begin(), validProbs.end());
        int idx = random_choice(mt);
        //
        std::vector<string> cards;
        StringUtils::split(validHands[idx], "-", cards);
        if (cards.size() != 2) {
            TEST_E("dealDards failed!");
            return -1;
        }
        //
        dealDards.insert(dealDards.end(), cards.begin(), cards.end());
        mHands.emplace(i, cards);
    }
    //
    TEST_T("======== deal-dards =======");
    for (auto iter = mHands.begin(); iter != mHands.end(); iter++) {
        TEST_T("== Player" << (*iter).first << " " << (*iter).second[0] << " " << (*iter).second[1]);
    }
    //
    if (!pEngine->distributeCards5(mHands, boardCards)) {
        TEST_F("distributeCards5() failed");
        return -2;
    }
    //
    auto tt2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tt2 - tt1).count();
    TEST_T("Deal cards: duration=" << duration / pow(10, 6));
    return 0;
}

int Evaluate::dealCards2(const int iterations,
                         const int turnIndex,
                         const std::vector<vector<string>> &cardPairs,
                         const std::vector<string> &boardCards,
                         const std::unordered_map<string, double> handRanges[],
                         Game *pEngine) {
    auto tt1 = std::chrono::high_resolution_clock::now();
    TEST_T("**********************************************");
    //
    std::unordered_map<int, std::vector<string>> mHands;
    //
    std::vector<string> dealDards;
    //
    std::string strHands;
    auto holeCards = &cardPairs[iterations % 1326];
    for (auto iter = holeCards->begin(); iter != holeCards->end(); iter++) {
        dealDards.emplace_back(*iter);
        strHands += *iter;
    }
    mHands.emplace(turnIndex, dealDards);
    //
    std::vector<string> validHands;
    std::vector<double> validProbs;
    int numPlayers = pEngine->getPlayerNum();
    int offSeats = numPlayers - turnIndex;
    std:: ostringstream os;
    os << std::to_string(iterations) << "-" << turnIndex << " ";
    os << "deal cards(" << strHands << "): " << turnIndex;
    // behind
    std::vector<int> vPositions;
    for (int i = turnIndex + 1; i < numPlayers; i++) {
        vPositions.emplace_back(i);
        os << " " << std::to_string(i);
    }
    // before
    for (int j = 0; j < turnIndex; j++) {
        vPositions.emplace_back(j);
        os << " " << std::to_string(j);
    }
    TEST_T(os.str());
    //
    for (auto iter = vPositions.begin(); iter != vPositions.end(); iter++) {
        auto i = *iter;
        auto player = pEngine->getPlayer(i);
        if (nullptr == player)
            continue;
        //
        if (player->isFold())
            continue;
        //
        auto &hands = handRanges[i];
        if (hands.empty()) {
            TEST_E("Player" << i << " No hand!");
            return -1;
        }
        //
        int beforeNum = hands.size();
        validHands.clear();
        validProbs.clear();
        //
        for (auto iter = hands.begin(); iter != hands.end(); iter++) {
            bool isValid = true;
            //Remove existing public cards
            for (auto iter2 = boardCards.begin(); iter2 != boardCards.end(); iter2++) {
                std::size_t found = (*iter).first.find(*iter2);
                if (found != std::string::npos) {
                    isValid = false;
                }
            }
            //Remove cards that have been dealt
            if (isValid) {
                for (auto iter3 = dealDards.begin(); iter3 != dealDards.end(); iter3++) {
                    std::size_t found = (*iter).first.find(*iter3);
                    if (found != std::string::npos) {
                        isValid = false;
                    }
                }
            }
            //
            if (isValid) {
                validHands.emplace_back((*iter).first);
                validProbs.emplace_back((*iter).second);
            }
        }
        //
        std::random_device mRd;
        std::mt19937 mt = std::mt19937(mRd());
        std::discrete_distribution<> random_choice(validProbs.begin(), validProbs.end());
        int idx = random_choice(mt);
        //
        std::vector<string> cards;
        StringUtils::split(validHands[idx], "-", cards);
        if (cards.size() != 2) {
            TEST_E("dealDards failed!");
            return -1;
        }
        //
        dealDards.insert(dealDards.end(), cards.begin(), cards.end());
        mHands.emplace(i, cards);
    }
    //
    TEST_T("======== deal-dards =======");
    for (auto iter = mHands.begin(); iter != mHands.end(); iter++) {
        TEST_T("== Player" << (*iter).first << " " << (*iter).second[0] << " " << (*iter).second[1]);
    }
    //
    if (!pEngine->distributeCards5(mHands, boardCards)) {
        TEST_F("distributeCards5() failed");
        return -2;
    }
    //
    auto tt2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tt2 - tt1).count();
    TEST_T("Deal cards: duration=" << duration / pow(10, 6));
    return 0;
}

int Evaluate::dealCards3(const int iterations,
                         const std::vector<string> &boardCards,
                         const std::unordered_map<string, double> handRanges[],
                         Game *pEngine) {
    auto tt1 = std::chrono::high_resolution_clock::now();
    TEST_T("**********************************************");
    //
    std::unordered_map<int, std::vector<string>> mHands;
    //
    std::vector<string> dealDards;
    std::vector<string> validHands;
    std::vector<double> validProbs;
    int numPlayers = pEngine->getPlayerNum();
    int turnIndex = pEngine->getTurnIndex();
    auto curPlayer = pEngine->getPlayer(turnIndex);
    if (nullptr == curPlayer) {
        TEST_E("curPlayer is nullptr, turnIndex=" << turnIndex);
        return -1;
    }
    //
    auto card1 = curPlayer->getHand(0)->strCluster();
    auto card2 = curPlayer->getHand(1)->strCluster();
    dealDards.emplace_back(card1);
    dealDards.emplace_back(card2);
    //
    int offSeats = numPlayers - turnIndex;
    std:: ostringstream os;
    os << "Player" << turnIndex << " deal cards(" << card1 << card2 << "): ";
    // behind
    std::vector<int> vPositions;
    for (int i = turnIndex + 1; i < numPlayers; i++) {
        vPositions.emplace_back(i);
        os << " " << std::to_string(i);
    }
    // before
    for (int j = 0; j < turnIndex; j++) {
        vPositions.emplace_back(j);
        os << " " << std::to_string(j);
    }
    TEST_T(os.str());
    //
    for (auto iter = vPositions.begin(); iter != vPositions.end(); iter++) {
        auto i = *iter;
        auto player = pEngine->getPlayer(i);
        if (nullptr == player)
            continue;
        //
        if (player->isFold())
            continue;
        //
        auto &hands = handRanges[i];
        if (hands.empty()) {
            TEST_E("Player" << i << " No hand!");
            return -1;
        }
        //
        int beforeNum = hands.size();
        //
        validHands.clear();
        validProbs.clear();
        //
        for (auto iter = hands.begin(); iter != hands.end(); iter++) {
            bool isValid = true;
            //Remove existing public cards
            for (auto iter2 = boardCards.begin(); iter2 != boardCards.end(); iter2++) {
                std::size_t found = (*iter).first.find(*iter2);
                if (found != std::string::npos) {
                    isValid = false;
                }
            }
            //Remove cards that have been dealt
            if (isValid) {
                for (auto iter3 = dealDards.begin(); iter3 != dealDards.end(); iter3++) {
                    std::size_t found = (*iter).first.find(*iter3);
                    if (found != std::string::npos) {
                        isValid = false;
                    }
                }
            }
            //
            if (isValid) {
                validHands.emplace_back((*iter).first);
                validProbs.emplace_back((*iter).second);
            }
        }
        //
        std::random_device mRd;
        std::mt19937 mt = std::mt19937(mRd());
        std::discrete_distribution<> random_choice(validProbs.begin(), validProbs.end());
        int idx = random_choice(mt);
        //
        std::vector<string> cards;
        StringUtils::split(validHands[idx], "-", cards);
        if (cards.size() != 2) {
            TEST_E("dealDards failed!");
            return -1;
        }
        //
        dealDards.insert(dealDards.end(), cards.begin(), cards.end());
        mHands.emplace(i, cards);
    }
    //
    TEST_T("======== deal-dards =======");
    for (auto iter = mHands.begin(); iter != mHands.end(); iter++) {
        TEST_T("== Player" << (*iter).first << " " << (*iter).second[0] << " " << (*iter).second[1]);
    }
    //
    if (!pEngine->distributeCards5(mHands, boardCards)) {
        TEST_F("distributeCards5() failed");
        return -2;
    }
    //
    auto tt2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tt2 - tt1).count();
    TEST_T("Deal cards: duration=" << duration / pow(10, 6));
    return 0;
}

int Evaluate::dealCards4(const int iterations,
                         const std::vector<string> &holeCards,
                         const std::vector<string> &boardCards,
                         const std::unordered_map<string, double> handRanges[],
                         Game *pEngine) {
    auto tt1 = std::chrono::high_resolution_clock::now();
    TEST_T("**********************************************");
    //
    std::unordered_map<int, std::vector<string>> mHands;
    //
    std::vector<string> dealDards;
    std::vector<string> validHands;
    std::vector<double> validProbs;
    int numPlayers = pEngine->getPlayerNum();
    int turnIndex = pEngine->getTurnIndex();
    auto curPlayer = pEngine->getPlayer(turnIndex);
    if (nullptr == curPlayer) {
        TEST_E("curPlayer is nullptr, turnIndex=" << turnIndex);
        return -1;
    }
    //
    auto card1 = curPlayer->getHand(0)->strCluster();
    auto card2 = curPlayer->getHand(1)->strCluster();
    dealDards.emplace_back(card1);
    dealDards.emplace_back(card2);
    //
    int offSeats = numPlayers - turnIndex;
    std:: ostringstream os;
    os << "Player" << turnIndex << " deal cards(" << card1 << card2 << "): ";
    // behind
    std::vector<int> vPositions;
    for (int i = turnIndex + 1; i < numPlayers; i++) {
        vPositions.emplace_back(i);
        os << " " << std::to_string(i);
    }
    // before
    for (int j = 0; j < turnIndex; j++) {
        vPositions.emplace_back(j);
        os << " " << std::to_string(j);
    }
    TEST_T(os.str());
    //
    for (auto iter = vPositions.begin(); iter != vPositions.end(); iter++) {
        auto i = *iter;
        auto player = pEngine->getPlayer(i);
        if (nullptr == player)
            continue;
        //
        if (player->isFold())
            continue;
        //
        auto &hands = handRanges[i];
        if (hands.empty()) {
            TEST_E("Player" << i << " No hand!");
            return -1;
        }
        //
        int beforeNum = hands.size();
        //
        validHands.clear();
        validProbs.clear();
        //
        for (auto iter = hands.begin(); iter != hands.end(); iter++) {
            bool isValid = true;
            //Remove existing public cards
            for (auto iter2 = boardCards.begin(); iter2 != boardCards.end(); iter2++) {
                std::size_t found = (*iter).first.find(*iter2);
                if (found != std::string::npos) {
                    isValid = false;
                }
            }
            //Remove cards that have been dealt
            if (isValid) {
                for (auto iter3 = dealDards.begin(); iter3 != dealDards.end(); iter3++) {
                    std::size_t found = (*iter).first.find(*iter3);
                    if (found != std::string::npos) {
                        isValid = false;
                    }
                }
            }
            //
            if (isValid) {
                validHands.emplace_back((*iter).first);
                validProbs.emplace_back((*iter).second);
            }
        }
        //
        std::random_device mRd;
        std::mt19937 mt = std::mt19937(mRd());
        std::discrete_distribution<> random_choice(validProbs.begin(), validProbs.end());
        int idx = random_choice(mt);
        //
        std::vector<string> cards;
        StringUtils::split(validHands[idx], "-", cards);
        if (cards.size() != 2) {
            TEST_E("dealDards failed!");
            return -1;
        }
        //
        dealDards.insert(dealDards.end(), cards.begin(), cards.end());
        mHands.emplace(i, cards);
    }
    //
    TEST_T("======== deal-dards =======");
    for (auto iter = mHands.begin(); iter != mHands.end(); iter++) {
        TEST_T("== Player" << (*iter).first << " " << (*iter).second[0] << " " << (*iter).second[1]);
    }
    //
    if (!pEngine->distributeCards5(mHands, boardCards)) {
        TEST_F("distributeCards5() failed");
        return -2;
    }
    //
    auto tt2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tt2 - tt1).count();
    TEST_T("Deal cards: duration=" << duration / pow(10, 6));
    return 0;
}
