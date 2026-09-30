#pragma once
#include "common.h"

struct Transfer {
    std::string from;
    std::string to;
    double amount;
    time_t timestamp;
};

class ledger {
public:
    double getbalance(const std::string& holderID) const;  // gets balance from holder and checks for duplicates
    double transfer(const std::string& from,const std::string& to, double amount); // transfers from amount into balances
        
    
private:
    std::map<std::string, double> balances_;
    std::vector<Transfer> history;
    std::uint64_t totalsupply = 21000000;
};
