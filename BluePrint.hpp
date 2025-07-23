#ifndef BLUEPRINT_HPP
#define BLUEPRINT_HPP

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <bitset>
//
#include "Log.hpp"
#include "Card.hpp"
#include "Single.hpp"
#include "Pluribus.hpp"

using namespace std;

class BluePrint : public Singleton<BluePrint> {
public:
    //
    BluePrint();
    //
    ~BluePrint();

public:
    //
    int init();
    //
    int final();
    //
    Pluribus *tree();

private:
    //
    Pluribus *bluePrint;
};

#endif