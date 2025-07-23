#ifndef EVALUATE_HPP
#define EVALUATE_HPP

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <set>
#include <bitset>
#include <unordered_map>
//
#include "Log.hpp"
#include "Card.hpp"
#include "Single.hpp"
#include "NumPy.hpp"
#include "StringUtils.hpp"
#include "BluePrint.hpp"
using namespace std;

#define DEFAULT_PROBS (1.0/1326.0)

class Research;
class Evaluate : public Singleton<Evaluate> {
public:
    //
    Evaluate();
    //
    ~Evaluate();

public:
    //
    int init();
    //
    int final();
    //
    int eval(const std::string &infoSet,
             const std::vector<string> &holeCards,
             const std::vector<string> &boardCards,
             Research *mResearch,
             std::vector<string> &actions,
             std::vector<double> &probabilities);
    //
    int initCardsList();
    //
    int initHandsList();
    //
    int cloneHandPairs(std::vector<vector<string>> &cardPairs);
    //
    int cloneHandProbs(std::unordered_map<string, double> &handsProbs);

public:
    // Deal cards to everyone at random
    static int dealCards(const int iterations,
                         const std::vector<string> &boardCards,
                         const std::unordered_map<string, double> handRanges[],
                         Game *pEngine);
    // Deal 1326 first to your own hand
    static int dealCards2(const int iterations,
                          const int turnIndex,
                          const std::vector<vector<string>> &cardPairs,
                          const std::vector<string> &boardCards,
                          const std::unordered_map<string, double> handRanges[],
                          Game *pEngine);
    // Fixed in your own hand
    static int dealCards3(const int iterations,
                          const std::vector<string> &boardCards,
                          const std::unordered_map<string, double> handRanges[],
                          Game *pEngine);
    // Set your own hand
    static int dealCards4(const int iterations,
                          const std::vector<string> &holeCards,
                          const std::vector<string> &boardCards,
                          const std::unordered_map<string, double> handRanges[],
                          Game *pEngine);

private:
    //
    std::vector<string> vCards;
    //
    std::vector<string> vHands;
    //
    std::vector<vector<string>> vPairs;
    //
    std::unordered_map<string, double> mProbs;

    std::vector<string> testCards;
};

#endif
