#include "Database.h"
#include <sqlite3.h>

// Full path to the database file. Change this one line if the file moves.
static const char* DB_PATH = "/Users/maleniam_falcon/RWATOK/RWATOKDB";

// Opens the database with foreign keys enforced. Returns nullptr on failure.
// "static" keeps this helper private to this file.
static sqlite3* openDb() {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        std::cerr << "Could not open database: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return nullptr;
    }
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    return db;
}

bool loadHolderFromDb(const std::string& holderId, HolderRegistry& registry) {
    sqlite3* db = openDb();
    if (db == nullptr) return false;

    const char* sql = "SELECT holder_id, name, kyc_verified, jurisdiction_code, token_balance "
                      "FROM holder WHERE holder_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Query failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, holderId.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string name         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        bool kyc                 = sqlite3_column_int(stmt, 2) == 1;
        std::string jurisdiction = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        long balance             = sqlite3_column_int64(stmt, 4);

        // Constructor order: name, id, jurisdiction, KYC, wallet, balance
        Holder h(name, id, jurisdiction, kyc, "", balance);
        found = registry.addHolder(h);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return found;
}

bool saveHolderToDb(const Holder& h) {
    sqlite3* db = openDb();
    if (db == nullptr) return false;

    const char* sql = "INSERT INTO holder (holder_id, name, kyc_verified, jurisdiction_code) "
                      "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Query failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, h.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, h.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, h.KYCVerified ? 1 : 0);
    sqlite3_bind_text(stmt, 4, h.jurisdictionCode.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;   // DONE means the insert worked
    if (!ok) std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

