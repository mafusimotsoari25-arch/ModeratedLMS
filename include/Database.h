#pragma once
#include <sqlite3.h>
#include <string>

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    sqlite3* handle() const { return db_; }
    void execute(const std::string& sql);
    void initialiseSchema();
private:
    sqlite3* db_ = nullptr;
};
