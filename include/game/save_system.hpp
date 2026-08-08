#pragma once
// KronoUniverse — Save/Load System (v0.3)
//
// Persiste estado do mundo usando SQLite:
// - Player position, health, inventory, XP, level
// - World block deltas (only modified blocks)
// - Mob spawns (only boss deaths, special spawns)
// - Time of day, weather state
// - Crafting recipe discovery
// - Statistics (kills, deaths, blocks mined)

#include <sqlite3.h>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

namespace krono {

struct SaveData {
    float player_x = 0, player_y = 0;
    float player_health = 100;
    float player_max_health = 100;
    float time_of_day = 0.5f;
    int player_level = 1;
    float player_xp = 0;
    int total_kills = 0;
    int blocks_mined = 0;
    int blocks_placed = 0;
    int deaths = 0;
    int mobs_spawned = 0;
    uint32_t world_seed = 42;
};

class SaveSystem {
public:
    sqlite3* db = nullptr;

    bool open(const std::string& path) {
        int rc = sqlite3_open(path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::cerr << "Cannot open save: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        // Create tables
        exec("CREATE TABLE IF NOT EXISTS player ("
             "id INTEGER PRIMARY KEY,"
             "x REAL, y REAL, health REAL, max_health REAL,"
             "level INTEGER, xp REAL,"
             "time_of_day REAL, world_seed INTEGER);");
        exec("CREATE TABLE IF NOT EXISTS stats ("
             "key TEXT PRIMARY KEY, value INTEGER);");
        exec("CREATE TABLE IF NOT EXISTS block_deltas ("
             "bx INTEGER, by INTEGER, type INTEGER, hp INTEGER,"
             "PRIMARY KEY (bx, by));");
        exec("CREATE TABLE IF NOT EXISTS discovered_recipes ("
             "recipe_id INTEGER PRIMARY KEY);");
        return true;
    }

    void close() {
        if (db) { sqlite3_close(db); db = nullptr; }
    }

    bool exec(const std::string& sql) {
        char* err = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << (err ? err : "unknown") << std::endl;
            if (err) sqlite3_free(err);
            return false;
        }
        return true;
    }

    bool save_player(const SaveData& data) {
        // Clear and re-insert
        exec("DELETE FROM player;");
        std::string sql = "INSERT INTO player VALUES (1, " +
            std::to_string(data.player_x) + ", " +
            std::to_string(data.player_y) + ", " +
            std::to_string(data.player_health) + ", " +
            std::to_string(data.player_max_health) + ", " +
            std::to_string(data.player_level) + ", " +
            std::to_string(data.player_xp) + ", " +
            std::to_string(data.time_of_day) + ", " +
            std::to_string(data.world_seed) + ");";
        return exec(sql);
    }

    bool load_player(SaveData& data) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT * FROM player WHERE id=1;", -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            data.player_x = (float)sqlite3_column_double(stmt, 1);
            data.player_y = (float)sqlite3_column_double(stmt, 2);
            data.player_health = (float)sqlite3_column_double(stmt, 3);
            data.player_max_health = (float)sqlite3_column_double(stmt, 4);
            data.player_level = sqlite3_column_int(stmt, 5);
            data.player_xp = (float)sqlite3_column_double(stmt, 6);
            data.time_of_day = (float)sqlite3_column_double(stmt, 7);
            data.world_seed = (uint32_t)sqlite3_column_int(stmt, 8);
            sqlite3_finalize(stmt);
            return true;
        }
        sqlite3_finalize(stmt);
        return false;
    }

    bool save_stat(const std::string& key, int value) {
        std::string sql = "INSERT OR REPLACE INTO stats VALUES ('" + key + "', " + std::to_string(value) + ");";
        return exec(sql);
    }

    int load_stat(const std::string& key, int default_val = 0) {
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "SELECT value FROM stats WHERE key='" + key + "';";
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return default_val;
        int result = default_val;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool save_block_delta(int bx, int by, int type, int hp) {
        std::string sql = "INSERT OR REPLACE INTO block_deltas VALUES (" +
            std::to_string(bx) + ", " + std::to_string(by) + ", " +
            std::to_string(type) + ", " + std::to_string(hp) + ");";
        return exec(sql);
    }

    struct BlockDelta { int bx, by, type, hp; };
    std::vector<BlockDelta> load_block_deltas() {
        std::vector<BlockDelta> result;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT bx, by, type, hp FROM block_deltas;", -1, &stmt, nullptr) != SQLITE_OK) {
            return result;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BlockDelta d;
            d.bx = sqlite3_column_int(stmt, 0);
            d.by = sqlite3_column_int(stmt, 1);
            d.type = sqlite3_column_int(stmt, 2);
            d.hp = sqlite3_column_int(stmt, 3);
            result.push_back(d);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool mark_recipe_discovered(int recipe_id) {
        std::string sql = "INSERT OR IGNORE INTO discovered_recipes VALUES (" + std::to_string(recipe_id) + ");";
        return exec(sql);
    }

    bool is_recipe_discovered(int recipe_id) {
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "SELECT recipe_id FROM discovered_recipes WHERE recipe_id=" + std::to_string(recipe_id) + ";";
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
        bool result = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) result = true;
        sqlite3_finalize(stmt);
        return result;
    }

    int count_discovered_recipes() {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM discovered_recipes;", -1, &stmt, nullptr) != SQLITE_OK) return 0;
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return count;
    }
};

} // namespace krono
