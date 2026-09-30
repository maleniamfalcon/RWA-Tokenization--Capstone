//
//  Database.h
//  RWA Tokenization
//
//  Created by Ubaid Khan on 9/30/26.
//

#pragma once
#include "common.h"
#include "HolderRegistry.h"



// Looks up one holder in the database and adds them to the registry.
// Returns false if the holder isn't found or the database can't be opened.
bool loadHolderFromDb(const std::string& holderId, HolderRegistry& registry);

// Saves one holder to the database. Returns true if it worked.
// Fails if a holder with the same ID already exists, or if the
// holder's jurisdiction code is not in the jurisdiction table.
bool saveHolderToDb(const Holder& h);

