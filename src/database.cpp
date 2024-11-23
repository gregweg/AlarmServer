#include "alarm_server.hpp"
#include <iostream>

Database::Database() {
    int rc = sqlite3_open("alarms.db", &db);
    if (rc) {
        throw DatabaseException("Can't open database: " + 
            std::string(sqlite3_errmsg(db)));
    }
    createTables();
}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

void Database::createTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS alarms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            description TEXT NOT NULL,
            datetime TEXT NOT NULL,
            recurrence INTEGER DEFAULT 0
        );
    )";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string error = errMsg;
        sqlite3_free(errMsg);
        throw DatabaseException("SQL error: " + error);
    }
}

int Database::addAlarm(const std::string& description, 
                      const std::string& datetime, 
                      RecurrenceType recurrence) {
    const char* sql = "INSERT INTO alarms (description, datetime, recurrence) "
                     "VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement: " + 
            std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, datetime.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, static_cast<int>(recurrence));
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to insert alarm: " + 
            std::string(sqlite3_errmsg(db)));
    }
    
    int id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return id;
}

std::vector<AlarmEvent> Database::loadAlarms() {
    std::vector<AlarmEvent> alarms;
    const char* sql = "SELECT id, description, datetime, recurrence FROM alarms";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement: " + 
            std::string(sqlite3_errmsg(db)));
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AlarmEvent event{
            sqlite3_column_int(stmt, 0),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            datetime_utils::parseDateTime(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))),
            static_cast<RecurrenceType>(sqlite3_column_int(stmt, 3))
        };
        alarms.push_back(event);
    }
    
    sqlite3_finalize(stmt);
    return alarms;
}

void Database::updateAlarmDateTime(int id, const std::string& new_datetime) {
    const char* sql = "UPDATE alarms SET datetime = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement: " + 
            std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, new_datetime.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, id);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to update alarm: " + 
            std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
}