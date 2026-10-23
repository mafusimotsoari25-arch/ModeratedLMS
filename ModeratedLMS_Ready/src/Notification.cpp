#include "Notification.h"
#include <iostream>
#include <stdexcept>

void ConsoleNotificationObserver::onNotification(int userId, const std::string& message) {
    std::cout << "[Notification for user " << userId << "] " << message << '\n';
}

void NotificationService::subscribe(INotificationObserver& observer) {
    observers_.push_back(&observer);
}

void NotificationService::notifyUser(int userId, const std::string& message) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO notifications(user_id, message) VALUES(?, ?);";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database_.handle()));
    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_text(stmt, 2, message.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(database_.handle()));
    for (auto* observer : observers_) observer->onNotification(userId, message);
}
