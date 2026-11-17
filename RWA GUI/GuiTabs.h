//
//  GuiTabs.h
//  RWA GUI
//
//  One function per tab. main.cpp calls these inside the window loop.
//
#pragma once
#include "HolderRegistry.h"
#include "Ledger.h"

// Holders tab: register a new holder, and list everyone with their balance.
void drawHoldersTab(HolderRegistry& registry, Ledger& book);

// Next tabs follow the same pattern, e.g.:
// void drawMintTab(HolderRegistry& registry, Ledger& book);
// void drawTransferTab(HolderRegistry& registry, TransferCoordinator& coord, Audit& audit);
