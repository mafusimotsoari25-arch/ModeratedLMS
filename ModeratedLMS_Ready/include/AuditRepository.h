#pragma once
#include "Database.h"
#include <optional>
#include <string>

class AuditRepository {
public:
    explicit AuditRepository(Database& database) : database_(database) {}
    void recordMarkChange(int submissionId, int changedByUserId,
                          std::optional<double> oldMark, double newMark,
                          const std::string& reason);
private:
    Database& database_;
};
