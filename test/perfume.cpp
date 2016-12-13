//
// Created by Nuxes Lin on 16/12/6.
//

#include "perfume.h"

Amaranth::Amaranth(int64_t) : val_(0) {
    std::cout << "constructor" << std::endl;
}

void Amaranth::PlusOne() {
    val_++;
}

Amaranth::~Amaranth() {
    std::cout << "destroyed" << std::endl;
}

