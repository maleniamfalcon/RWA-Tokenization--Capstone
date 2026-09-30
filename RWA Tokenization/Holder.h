#pragma once
#include "common.h"

struct Holder{
    std::string name;
    std::string id;
    std::string jurisdictionCode;
    bool KYCVerified;
    std::string wallet;
    std::time_t lockupEndDate;
};
