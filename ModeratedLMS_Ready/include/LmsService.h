#pragma once
#include "AuditRepository.h"
#include "Database.h"
#include "LatePenalty.h"
#include "Notification.h"
#include "User.h"
#include <memory>
#include <string>

class LmsService {
public:
    explicit LmsService(Database& database);
    void seedDemoData();
    std::unique_ptr<User> login(const std::string& username, const std::string& password);
    int createUser(const User& actor, const std::string& username,
                   const std::string& password, Role role);
    int createAssignment(const User& actor, int courseId, const std::string& title,
                         const std::string& deadline, double maximumMark);
    int submitAssignment(const User& actor, int assignmentId, const std::string& content,
                        const std::string& submittedAt = "");
    int grantExtension(const User& actor, int assignmentId, int studentId,
                       const std::string& revisedDeadline, const std::string& reason);
    int resubmitAssignment(const User& actor, int assignmentId, const std::string& content,
                           const std::string& submittedAt = "");
    void markSubmission(const User& actor, int submissionId, double rawMark,
                        const std::string& feedback);
    void moderateAndRelease(const User& actor, int submissionId, bool approve,
                            const std::string& comments = "");
    void changeMark(const User& actor, int submissionId, double newRawMark,
                    const std::string& reason);
    void showStudentMarks(const User& actor);
    void showPendingModeration(const User& actor);
    void showAssignments(const User& actor);
    void showNotifications(const User& actor);
    void runDemonstration();
private:
    Database& database_;
    StandardLatePenalty penalty_;
    AuditRepository audit_;
    NotificationService notifications_;
    Role roleForUser(int userId) const;
    void requireRole(const User& actor, Role required) const;
    void requireAnyRole(const User& actor, std::initializer_list<Role> allowed) const;
    bool isEnrolled(int studentId, int assignmentId) const;
    std::string effectiveDeadline(int assignmentId, int studentId) const;
};
