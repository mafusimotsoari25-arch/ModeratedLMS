#include "AuditRepository.h"
#include <stdexcept>

void AuditRepository::recordMarkChange(int submissionId, int changedByUserId,
                                       std::optional<double> oldMark, double newMark,
                                       const std::string& reason) {
    if (reason.empty()) throw std::invalid_argument("An audit reason is required");
    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT INTO mark_change_audit_record "
        "(submission_id, changed_by_user_id, old_mark, new_mark, reason) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(database_.handle(), sql, -1, &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database_.handle()));
    sqlite3_bind_int(statement, 1, submissionId);
    sqlite3_bind_int(statement, 2, changedByUserId);
    if (oldMark) sqlite3_bind_double(statement, 3, *oldMark);
    else sqlite3_bind_null(statement, 3);
    sqlite3_bind_double(statement, 4, newMark);
    sqlite3_bind_text(statement, 5, reason.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(database_.handle()));
}
