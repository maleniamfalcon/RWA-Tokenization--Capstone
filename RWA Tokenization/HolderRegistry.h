#pragma once
#include "common.h"
#include "Holder.h"
#include <cstddef>
#include <map>
#include <string>
 



class HolderRegistry{
public:
  
    bool addHolder(const Holder& holder);   // adds holder and returns false if holder is ID is empty or duplicate
    const  Holder* findHolder(const std::string& id) const; // find holder by id, returns a pointer to the stored holder or nullptr if not found, pointer is read only
    bool updateHolder(const Holder& holder); // replaces stored data for existing holders, false if ID is not registered
    
    std::size_t size() const; //how many holders are registered;
    
    
    
private:
    std::map<std::string, Holder> holders_;
    
    
    
};
