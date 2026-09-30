#include "Ledger.h"
#include <string>

double ledger::getbalance(const std::string& holderID) const {
    auto it = balances_.find(holderID);
    if(it != balances_.end()) {
        return it->second;
    }
        
    return 0.0;
}
double ledger::transfer(const std::string& from,const std::string& to,  double amount) {
     auto it = balances_.find(from);
     if(it == balances_.end()) {
         return false;
     }
     if (it->second < amount) {
         return false;
     }
     it->second -= amount;
     auto aT = balances_.find(to);
     if(aT != balances_.end()){
         return it->second;
     }
     
     
     
     return 0.0;
    
}
    
    
    
    
    




