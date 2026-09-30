#include "HolderRegistry.h"



  
    bool HolderRegistry::addHolder(const Holder& holder) {
        if (holder.id.empty()) {
            return false;
        }
        auto result = holders_.insert({holder.id, holder});
        return result.second;
    }

    const  Holder* HolderRegistry::findHolder(const std::string& id) const {
        auto it = holders_.find(id);
        if (it == holders_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    bool HolderRegistry::updateHolder(const Holder& holder) {
        auto it = holders_.find(holder.id);
        if ( it == holders_.end()) {
            return false;
        
        }
        it->second = holder;
        return true;
    }
    
    std::size_t HolderRegistry::size() const {
        return holders_.size();
    }
    
    
    
    
