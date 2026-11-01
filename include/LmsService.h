#pragma once

#include "AuditRepository.h"
#include "Database.h"
#include "LatePenalty.h"
#include "Notification.h"
#include "User.h"

#include <memory>
#include <string>
#include <initializer_list>

class LmsService {
public:
    explicit LmsService(Database& database);

    void runApplication();

    void seedDemoData();
    bool hasAdministrator() const;
    int bootstrapAdministrator(const std::string& username,
                               const std::string& password);

    std::unique_ptr<User> login(const std::string& username,
                                const std::string& password);

    int registerStudent(const std::string& username,
                        const std::string& password);

    void enrolStudent(const User& actor, int studentId, int courseId);

    int createDepartment(const User& actor, const std::string& name);

    int createCourse(const User& actor, int departmentId,
                     const std::string& code, const std::string& name);

    void assignLecturer(const User& actor, int courseId, int lecturerId);

    void assignHeadOfDepartment(const User& actor,
                                int departmentId, int hodId);

    void setUserActive(const User& actor, int userId, bool active);

    void listAdminData(const User& actor);

    int createUser(const User& actor, const std::string& username,
                   const std::string& password, Role role);

    int createAssignment(const User& actor, int courseId,
                         const std::string& title,
                         const std::string& deadline,
                         double maximumMark);

    int submitAssignment(const User& actor, int assignmentId,
                         const std::string& content,
                         const std::string& submittedAt = "",
                         const std::string& filePath = "");

    int grantExtension(const User& actor, int assignmentId, int studentId,
                       const std::string& revisedDeadline,
                       const std::string& reason);

    int resubmitAssignment(const User& actor, int assignmentId,
                           const std::string& content,
                           const std::string& submittedAt = "",
                           const std::string& filePath = "");

    void markSubmission(const User& actor, int submissionId,
                        double rawMark, const std::string& feedback);

    void moderateAndRelease(const User& actor, int submissionId,
                            bool approve,
                            const std::string& comments = "");

    void changeMark(const User& actor, int submissionId,
                    double newRawMark, const std::string& reason);

    void showStudentMarks(const User& actor);
    void showPendingModeration(const User& actor);
    void showAssignments(const User& actor);
    void showNotifications(const User& actor);

private:
    Database& database_;
    StandardLatePenalty penalty_;
    AuditRepository audit_;
    NotificationService notifications_;

    Role roleForUser(int userId) const;

    void requireRole(const User& actor, Role required) const;

    void requireAnyRole(const User& actor,
                        std::initializer_list<Role> allowed) const;

    bool isEnrolled(int studentId, int assignmentId) const;

    std::string effectiveDeadline(int assignmentId,
                                  int studentId) const;
};