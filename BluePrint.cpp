#include "BluePrint.hpp"

BluePrint::BluePrint(): bluePrint(nullptr) {

}

BluePrint::~BluePrint() {

}

int BluePrint::init() {
    if (nullptr == bluePrint) {
        bluePrint = new Pluribus();
        bluePrint->load();
    }
    //
    return 0;
}

int BluePrint::final() {
    if (nullptr != bluePrint) {
        delete bluePrint;
        bluePrint = nullptr;
    }
    //
    return 0;
}

Pluribus *BluePrint::tree() {
    return bluePrint;
}