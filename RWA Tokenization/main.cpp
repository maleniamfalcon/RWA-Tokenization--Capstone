#include "common.h"
#include "HolderRegistry.h"
#include <sqlite3.h>
#include "Database.h"




void printholder(const Holder& h) { // print holder;
    std::cout << h.name << " " << h.id << " " << h.jurisdictionCode << " "
              << (h.KYCVerified ? "Yes" : "No") << " " << h.wallet << '\n';
}

// ---------------------------------------------------------------
// Main
// ---------------------------------------------------------------
int main() {
    
    
    
    
    HolderRegistry registry;

    // Holders created in code
    Holder Ubaid("Ubaid Khan", "A001", "US", true, "0xA11c000000000000000000000000000000000001", 0);
    registry.addHolder(Ubaid);
    
    
    saveHolderToDb(Ubaid);
   

    // Holder loaded from the database
    loadHolderFromDb("B003", registry);

    // Show everyone in the registry
    for (const std::string& id : {"A001", "B003"}) {
        const Holder* h = registry.findHolder(id);
        if (h != nullptr) printholder(*h);
    }
    

    return 0;
}
