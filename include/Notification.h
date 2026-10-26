#pragma once
#include "Database.h"
#include <string>
#include <vector>

class INotificationObserver {
public:
    virtual ~INotificationObserver() = default;
    virtual void onNotification(int userId, const std::string& message) = 0;
};

class ConsoleNotificationObserver final : public INotificationObserver {
public:
    void onNotification(int userId, const std::string& message) override;
};

class NotificationService {
public:
    explicit NotificationService(Database& database) : database_(database) {}
    void subscribe(INotificationObserver& observer);
    void notifyUser(int userId, const std::string& message);
private:
    Database& database_;
    std::vector<INotificationObserver*> observers_;
};
