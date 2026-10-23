#pragma once
#include "AuditRepository.h"
#include "Database.h"
#include "LatePenalty.h"
#include "Notification.h"

class LmsService {
public:
    explicit LmsService(Database& database);
    void runDemonstration();
private:
    Database& database_;
    StandardLatePenalty penalty_;
    AuditRepository audit_;
    NotificationService notifications_;
    void seedDemoData();
    int scalarInt(const char* sql);
};
