//
// Created by Nuxes Lin on 16/12/6.
//

#ifndef CMAKE_TEST_PERFUME_H
#define CMAKE_TEST_PERFUME_H

#include <iostream>

class Amaranth {
public:
    inline Amaranth(int64_t);
    virtual ~Amaranth();
    void PlusOne();

private:
    int64_t val_;
};

class DyingResource {
public:
    DyingResource() {}

private:

};

#endif //CMAKE_TEST_PERFUME_H
