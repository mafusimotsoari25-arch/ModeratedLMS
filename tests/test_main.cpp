#include <gtest/gtest.h>

#include "AuditRepository.h"
#include "Database.h"
#include "LatePenalty.h"
#include "LmsService.h"
#include "Notification.h"
#include "PasswordService.h"
#include "User.h"
#include "UserFactory.h"

#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <fstream>

// ------------------------------------------------------------
// Helper function
// ------------------------------------------------------------

static int getTableCount(Database& database) {
    sqlite3_stmt* statement = nullptr;

    const char* sql =
        "SELECT COUNT(*) FROM sqlite_master "
        "WHERE type = 'table' AND name NOT LIKE 'sqlite_%';";

    if (sqlite3_prepare_v2(database.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Could not prepare table-count query");
    }

    int count = 0;

    if (sqlite3_step(statement) == SQLITE_ROW) {
        count = sqlite3_column_int(statement, 0);
    }

    sqlite3_finalize(statement);
    return count;
}

static int getUserId(Database& database, const std::string& username) {
    sqlite3_stmt* statement = nullptr;

    const char* sql =
        "SELECT user_id FROM user WHERE username = ?;";

    if (sqlite3_prepare_v2(database.handle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Could not prepare user ID query");
    }

    sqlite3_bind_text(statement, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    int id = -1;

    if (sqlite3_step(statement) == SQLITE_ROW) {
        id = sqlite3_column_int(statement, 0);
    }

    sqlite3_finalize(statement);
    return id;
}
// ------------------------------------------------------------
// Late penalty tests
// ------------------------------------------------------------

TEST(LatePenaltyTest, NoLateDaysReturnsOriginalMark) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(80, 100, 0), 80);
}

TEST(LatePenaltyTest, NegativeLateDaysReturnsOriginalMark) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(80, 100, -1), 80);
}

TEST(LatePenaltyTest, OneDayLateDeductsTenPercent) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(80, 100, 1), 70);
}

TEST(LatePenaltyTest, TwoDaysLateDeductsTwentyPercent) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(80, 100, 2), 60);
}

TEST(LatePenaltyTest, ThreeDaysLateDeductsThirtyPercent) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(80, 100, 3), 50);
}

TEST(LatePenaltyTest, MoreThanThreeDaysLateReturnsZero) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(80, 100, 4), 0);
}

TEST(LatePenaltyTest, ThreeDaysLateCannotProduceNegativeMark) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(20, 100, 3), 0);
}

TEST(LatePenaltyTest, ZeroRawMarkRemainsZero) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(0, 100, 1), 0);
}

TEST(LatePenaltyTest, MaximumMarkCanBeUsed) {
    StandardLatePenalty penalty;
    EXPECT_DOUBLE_EQ(penalty.finalMark(100, 100, 1), 90);
}

TEST(LatePenaltyTest, ZeroMaximumMarkThrowsException) {
    StandardLatePenalty penalty;

    EXPECT_THROW(
        penalty.finalMark(50, 0, 1),
        std::invalid_argument
    );
}

TEST(LatePenaltyTest, NegativeMaximumMarkThrowsException) {
    StandardLatePenalty penalty;

    EXPECT_THROW(
        penalty.finalMark(50, -100, 1),
        std::invalid_argument
    );
}

TEST(LatePenaltyTest, NegativeRawMarkThrowsException) {
    StandardLatePenalty penalty;

    EXPECT_THROW(
        penalty.finalMark(-1, 100, 0),
        std::invalid_argument
    );
}

TEST(LatePenaltyTest, RawMarkAboveMaximumThrowsException) {
    StandardLatePenalty penalty;

    EXPECT_THROW(
        penalty.finalMark(101, 100, 0),
        std::invalid_argument
    );
}

// ------------------------------------------------------------
// Password service tests
// ------------------------------------------------------------

TEST(PasswordServiceTest, HashPasswordProducesAHash) {
    const std::string hash =
        PasswordService::hashPassword("Password123!");

    EXPECT_FALSE(hash.empty());
    EXPECT_NE(hash, "Password123!");
}

TEST(PasswordServiceTest, CorrectPasswordIsAccepted) {
    const std::string hash =
        PasswordService::hashPassword("Password123!");

    EXPECT_TRUE(
        PasswordService::verifyPassword(hash, "Password123!")
    );
}

TEST(PasswordServiceTest, IncorrectPasswordIsRejected) {
    const std::string hash =
        PasswordService::hashPassword("Password123!");

    EXPECT_FALSE(
        PasswordService::verifyPassword(hash, "WrongPassword123!")
    );
}

TEST(PasswordServiceTest, SamePasswordProducesDifferentHashes) {
    const std::string first =
        PasswordService::hashPassword("Password123!");

    const std::string second =
        PasswordService::hashPassword("Password123!");

    EXPECT_NE(first, second);
}

TEST(PasswordServiceTest, ShortPasswordThrowsException) {
    EXPECT_THROW(
        PasswordService::hashPassword("short"),
        std::invalid_argument
    );
}

TEST(PasswordServiceTest, EmptyPasswordThrowsException) {
    EXPECT_THROW(
        PasswordService::hashPassword(""),
        std::invalid_argument
    );
}

TEST(PasswordServiceTest, InvalidHashIsRejected) {
    EXPECT_FALSE(
        PasswordService::verifyPassword("invalid-hash", "Password123!")
    );
}

TEST(PasswordServiceTest, EmptyHashIsRejected) {
    EXPECT_FALSE(
        PasswordService::verifyPassword("", "Password123!")
    );
}

// ------------------------------------------------------------
// User factory tests
// ------------------------------------------------------------

TEST(UserFactoryTest, CreatesStudent) {
    std::unique_ptr<User> user =
        UserFactory::create(1, "student1", "Student");

    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->role(), Role::Student);
    EXPECT_EQ(user->roleName(), "Student");
    EXPECT_EQ(user->username(), "student1");
}

TEST(UserFactoryTest, CreatesLecturer) {
    std::unique_ptr<User> user =
        UserFactory::create(2, "lecturer1", "Lecturer");

    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->role(), Role::Lecturer);
    EXPECT_EQ(user->roleName(), "Lecturer");
}

TEST(UserFactoryTest, CreatesHeadOfDepartment) {
    std::unique_ptr<User> user =
        UserFactory::create(3, "hod1", "HeadOfDepartment");

    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->role(), Role::HeadOfDepartment);
    EXPECT_EQ(user->roleName(), "Head of Department");
}

TEST(UserFactoryTest, CreatesAdministrator) {
    std::unique_ptr<User> user =
        UserFactory::create(4, "admin1", "Administrator");

    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->role(), Role::Administrator);
    EXPECT_EQ(user->roleName(), "Administrator");
}

TEST(UserFactoryTest, UnknownRoleThrowsException) {
    EXPECT_THROW(
        UserFactory::create(5, "user1", "UnknownRole"),
        std::invalid_argument
    );
}

// ------------------------------------------------------------
// Database tests
// ------------------------------------------------------------

TEST(DatabaseTest, CreatesDatabaseSuccessfully) {
    EXPECT_NO_THROW({
        Database database(":memory:");
    });
}

TEST(DatabaseTest, CreatesAllFifteenTables) {
    Database database(":memory:");
    database.initialiseSchema();

    EXPECT_EQ(getTableCount(database), 15);
}

TEST(DatabaseTest, SchemaCanBeInitialisedMoreThanOnce) {
    Database database(":memory:");

    EXPECT_NO_THROW({
        database.initialiseSchema();
        database.initialiseSchema();
    });

    EXPECT_EQ(getTableCount(database), 15);
}

// ------------------------------------------------------------
// Audit repository tests
// ------------------------------------------------------------

TEST(AuditRepositoryTest, RejectsEmptyReason) {
    Database database(":memory:");
    database.initialiseSchema();

    AuditRepository audit(database);

    EXPECT_THROW(
        audit.recordMarkChange(1, 1, 50.0, 60.0, ""),
        std::invalid_argument
    );
}

// ------------------------------------------------------------
// Notification tests
// ------------------------------------------------------------

class TestNotificationObserver : public INotificationObserver {
public:
    int receivedUserId = -1;
    std::string receivedMessage;
    int notificationCount = 0;

    void onNotification(int userId, const std::string& message) override {
        receivedUserId = userId;
        receivedMessage = message;
        notificationCount++;
    }
};

TEST(NotificationTest, ObserverReceivesNotification) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    const int adminId = getUserId(database, "admin");
    ASSERT_GT(adminId, 0);

    NotificationService notifications(database);
    TestNotificationObserver observer;

    notifications.subscribe(observer);
    notifications.notifyUser(adminId, "Test notification");

    EXPECT_EQ(observer.receivedUserId, adminId);
    EXPECT_EQ(observer.receivedMessage, "Test notification");
    EXPECT_EQ(observer.notificationCount, 1);
}

TEST(NotificationTest, NotificationCanBeStoredInDatabase) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    const int adminId = getUserId(database, "admin");
    ASSERT_GT(adminId, 0);

    NotificationService notifications(database);
    notifications.notifyUser(adminId, "Stored notification");

    sqlite3_stmt* statement = nullptr;

    const char* sql =
        "SELECT COUNT(*) FROM notification WHERE user_id = ?;";

    ASSERT_EQ(
        sqlite3_prepare_v2(database.handle(), sql, -1,
                           &statement, nullptr),
        SQLITE_OK
    );

    sqlite3_bind_int(statement, 1, adminId);

    int count = 0;

    if (sqlite3_step(statement) == SQLITE_ROW) {
        count = sqlite3_column_int(statement, 0);
    }

    sqlite3_finalize(statement);

    EXPECT_EQ(count, 1);
}

// ------------------------------------------------------------
// LMS administrator and authentication tests
// ------------------------------------------------------------

TEST(LmsServiceTest, NewDatabaseHasNoAdministrator) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);

    EXPECT_FALSE(service.hasAdministrator());
}

TEST(LmsServiceTest, CanCreateFirstAdministrator) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);

    EXPECT_NO_THROW(
        service.bootstrapAdministrator("admin", "Admin123456!")
    );

    EXPECT_TRUE(service.hasAdministrator());
}

TEST(LmsServiceTest, FirstAdministratorCanLogIn) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> user =
        service.login("admin", "Admin123456!");

    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->role(), Role::Administrator);
}

TEST(LmsServiceTest, IncorrectPasswordIsRejected) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    EXPECT_THROW(
        service.login("admin", "WrongPassword123!"),
        std::exception
    );
}
TEST(LmsServiceTest, UnknownUsernameCannotLogIn) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    EXPECT_THROW(
        service.login("unknown", "Password123!"),
        std::exception
    );
}
// ------------------------------------------------------------
// LMS user management tests
// ------------------------------------------------------------

TEST(LmsUserManagementTest, AdministratorCanCreateStudent) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    EXPECT_NO_THROW(
        service.createUser(*admin, "student1", "Student123!", Role::Student)
    );

    std::unique_ptr<User> student =
        service.login("student1", "Student123!");

    ASSERT_NE(student, nullptr);
    EXPECT_EQ(student->role(), Role::Student);
}

TEST(LmsUserManagementTest, AdministratorCanCreateLecturer) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    EXPECT_NO_THROW(
        service.createUser(*admin, "lecturer1", "Lecturer123!", Role::Lecturer)
    );

    std::unique_ptr<User> lecturer =
        service.login("lecturer1", "Lecturer123!");

    ASSERT_NE(lecturer, nullptr);
    EXPECT_EQ(lecturer->role(), Role::Lecturer);
}

TEST(LmsUserManagementTest, AdministratorCanCreateHeadOfDepartment) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    EXPECT_NO_THROW(
        service.createUser(
            *admin, "hod1", "HodPassword123!", Role::HeadOfDepartment
        )
    );

    std::unique_ptr<User> hod =
        service.login("hod1", "HodPassword123!");

    ASSERT_NE(hod, nullptr);
    EXPECT_EQ(hod->role(), Role::HeadOfDepartment);
}

TEST(LmsUserManagementTest, ShortPasswordIsRejected) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    EXPECT_THROW(
        service.createUser(*admin, "student1", "short", Role::Student),
        std::invalid_argument
    );
}

TEST(LmsUserManagementTest, EmptyUsernameIsRejected) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    EXPECT_THROW(
        service.createUser(*admin, "", "Student123!", Role::Student),
        std::invalid_argument
    );
}

TEST(LmsUserManagementTest, DuplicateUsernameIsRejected) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    service.createUser(*admin, "student1", "Student123!", Role::Student);

    EXPECT_THROW(
        service.createUser(*admin, "student1", "Another123!", Role::Student),
        std::exception
    );
}

// ------------------------------------------------------------
// LMS role permission tests
// ------------------------------------------------------------

TEST(LmsPermissionTest, StudentCannotCreateUser) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    service.createUser(*admin, "student1", "Student123!", Role::Student);

    std::unique_ptr<User> student =
        service.login("student1", "Student123!");

    ASSERT_NE(student, nullptr);

    EXPECT_THROW(
        service.createUser(*student, "student2", "Student456!", Role::Student),
        std::exception
    );
}

TEST(LmsPermissionTest, StudentCannotCreateDepartment) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    service.createUser(*admin, "student1", "Student123!", Role::Student);

    std::unique_ptr<User> student =
        service.login("student1", "Student123!");

    ASSERT_NE(student, nullptr);

    EXPECT_THROW(
        service.createDepartment(*student, "Computing"),
        std::exception
    );
}

TEST(LmsPermissionTest, StudentCannotCreateCourse) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    service.createUser(*admin, "student1", "Student123!", Role::Student);

    std::unique_ptr<User> student =
        service.login("student1", "Student123!");

    ASSERT_NE(student, nullptr);

    EXPECT_THROW(
        service.createCourse(*student, 1, "CSE101", "Programming"),
        std::exception
    );
}

// ------------------------------------------------------------
// LMS department and course tests
// ------------------------------------------------------------

TEST(LmsDepartmentTest, AdministratorCanCreateDepartment) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    const int departmentId =
        service.createDepartment(*admin, "Computing");

    EXPECT_GT(departmentId, 0);
}

TEST(LmsDepartmentTest, AdministratorCanCreateCourse) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    const int departmentId =
        service.createDepartment(*admin, "Computing");

    const int courseId =
        service.createCourse(*admin, departmentId, "CSE101", "Programming");

    EXPECT_GT(courseId, 0);
}

TEST(LmsDepartmentTest, CourseCannotUseInvalidDepartment) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);
    service.bootstrapAdministrator("admin", "Admin123456!");

    std::unique_ptr<User> admin =
        service.login("admin", "Admin123456!");

    ASSERT_NE(admin, nullptr);

    EXPECT_THROW(
        service.createCourse(*admin, 9999, "CSE101", "Programming"),
        std::exception
    );
}

// ------------------------------------------------------------
// Assignment workflow test fixture
// ------------------------------------------------------------

class AssignmentWorkflowTest : public ::testing::Test {
protected:
    Database database{":memory:"};
    LmsService service{database};

    int adminId{};
    int studentId{};
    int lecturerId{};
    int hodId{};
    int departmentId{};
    int courseId{};
    int assignmentId{};

    std::unique_ptr<User> admin;
    std::unique_ptr<User> student;
    std::unique_ptr<User> lecturer;
    std::unique_ptr<User> hod;

    void SetUp() override {
        database.initialiseSchema();

        adminId = service.bootstrapAdministrator(
            "admin", "Admin123456!"
        );

        admin = service.login("admin", "Admin123456!");

        studentId = service.createUser(
            *admin, "student", "Student123!", Role::Student
        );

        lecturerId = service.createUser(
            *admin, "lecturer", "Lecturer123!", Role::Lecturer
        );

        hodId = service.createUser(
            *admin, "hod", "HodPassword123!", Role::HeadOfDepartment
        );

        student = service.login("student", "Student123!");
        lecturer = service.login("lecturer", "Lecturer123!");
        hod = service.login("hod", "HodPassword123!");

        departmentId = service.createDepartment(
            *admin, "Computing"
        );

        courseId = service.createCourse(
            *admin, departmentId, "CSE101", "Programming"
        );

        service.assignLecturer(*admin, courseId, lecturerId);
        service.assignHeadOfDepartment(*admin, departmentId, hodId);
        service.enrolStudent(*admin, studentId, courseId);

        assignmentId = service.createAssignment(
            *lecturer,
            courseId,
            "Programming Assignment",
            "2099-10-20 23:59:59",
            100.0
        );
    }

    int submitOnTime() {
        return service.submitAssignment(
            *student,
            assignmentId,
            "My assignment answer",
            "2099-10-19 12:00:00"
        );
    }
};

// ------------------------------------------------------------
// Assignment creation tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, LecturerCanCreateAssignment) {
    EXPECT_GT(assignmentId, 0);
}

TEST_F(AssignmentWorkflowTest, StudentCannotCreateAssignment) {
    EXPECT_THROW(
        service.createAssignment(
            *student, courseId, "Another Assignment",
            "2099-10-20 23:59:59", 100.0
        ),
        std::exception
    );
}

// ------------------------------------------------------------
// Submission tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, StudentCanSubmitText) {
    const int submissionId = submitOnTime();

    EXPECT_GT(submissionId, 0);
}

TEST_F(AssignmentWorkflowTest, StudentCannotSubmitWithoutTextOrFile) {
    EXPECT_THROW(
        service.submitAssignment(
            *student, assignmentId, "", "2099-10-19 12:00:00"
        ),
        std::invalid_argument
    );
}

TEST_F(AssignmentWorkflowTest, StudentCannotSubmitTwice) {
    submitOnTime();

    EXPECT_THROW(
        submitOnTime(),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, StudentCannotSubmitIfNotEnrolled) {
    service.createUser(
        *admin, "student2", "Student456!", Role::Student
    );

    auto anotherStudent = service.login(
        "student2", "Student456!"
    );

    EXPECT_THROW(
        service.submitAssignment(
            *anotherStudent,
            assignmentId,
            "My answer",
            "2099-10-19 12:00:00"
        ),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, StudentCanSubmitFile) {
    const std::string fileName = "test_submission.txt";

    {
        std::ofstream file(fileName);
        ASSERT_TRUE(file.is_open());
        file << "Test assignment file";
    }

    EXPECT_NO_THROW(
        service.submitAssignment(
            *student,
            assignmentId,
            "",
            "2099-10-19 12:00:00",
            fileName
        )
    );

    std::filesystem::remove(fileName);
}

TEST_F(AssignmentWorkflowTest, MissingSubmissionFileIsRejected) {
    EXPECT_THROW(
        service.submitAssignment(
            *student,
            assignmentId,
            "",
            "2099-10-19 12:00:00",
            "missing_submission_file.txt"
        ),
        std::invalid_argument
    );
}

// ------------------------------------------------------------
// Extension tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, LecturerCanGrantExtension) {
    EXPECT_NO_THROW(
        service.grantExtension(
            *lecturer,
            assignmentId,
            studentId,
            "2099-10-25 23:59:59",
            "Medical reason"
        )
    );
}

TEST_F(AssignmentWorkflowTest, StudentCannotGrantExtension) {
    EXPECT_THROW(
        service.grantExtension(
            *student,
            assignmentId,
            studentId,
            "2099-10-25 23:59:59",
            "Reason"
        ),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, ExtensionAllowsSubmissionAfterOriginalDeadline) {
    service.grantExtension(
        *lecturer,
        assignmentId,
        studentId,
        "2099-10-25 23:59:59",
        "Approved extension"
    );

    EXPECT_NO_THROW(
        service.submitAssignment(
            *student,
            assignmentId,
            "Submitted during extension",
            "2099-10-23 12:00:00"
        )
    );
}

TEST_F(AssignmentWorkflowTest, ExtensionRequiresReason) {
    EXPECT_THROW(
        service.grantExtension(
            *lecturer,
            assignmentId,
            studentId,
            "2099-10-25 23:59:59",
            ""
        ),
        std::invalid_argument
    );
}

// ------------------------------------------------------------
// Resubmission tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, StudentCanResubmitBeforeDeadline) {
    const int submissionId = submitOnTime();

    const int returnedId = service.resubmitAssignment(
        *student,
        assignmentId,
        "Updated assignment answer",
        "2099-10-19 15:00:00"
    );

    EXPECT_EQ(returnedId, submissionId);
}

TEST_F(AssignmentWorkflowTest, ResubmissionUpdatesCurrentVersion) {
    const int submissionId = submitOnTime();

    service.resubmitAssignment(
        *student,
        assignmentId,
        "Updated answer",
        "2099-10-19 15:00:00"
    );

    sqlite3_stmt* statement = nullptr;

    ASSERT_EQ(
        sqlite3_prepare_v2(
            database.handle(),
            "SELECT current_version FROM submission WHERE submission_id=?;",
            -1,
            &statement,
            nullptr
        ),
        SQLITE_OK
    );

    sqlite3_bind_int(statement, 1, submissionId);

    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);

    EXPECT_EQ(sqlite3_column_int(statement, 0), 2);

    sqlite3_finalize(statement);
}

TEST_F(AssignmentWorkflowTest, ResubmissionCannotOccurAfterDeadline) {
    submitOnTime();

    EXPECT_THROW(
        service.resubmitAssignment(
            *student,
            assignmentId,
            "Late resubmission",
            "2099-10-21 12:00:00"
        ),
        std::exception
    );
}

// ------------------------------------------------------------
// Marking tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, LecturerCanMarkSubmission) {
    const int submissionId = submitOnTime();

    EXPECT_NO_THROW(
        service.markSubmission(
            *lecturer, submissionId, 80.0, "Good work"
        )
    );
}

TEST_F(AssignmentWorkflowTest, StudentCannotMarkSubmission) {
    const int submissionId = submitOnTime();

    EXPECT_THROW(
        service.markSubmission(
            *student, submissionId, 80.0, "Good work"
        ),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, MarkAboveMaximumIsRejected) {
    const int submissionId = submitOnTime();

    EXPECT_THROW(
        service.markSubmission(
            *lecturer, submissionId, 110.0, "Good work"
        ),
        std::invalid_argument
    );
}

TEST_F(AssignmentWorkflowTest, LateSubmissionReceivesPenalty) {
    const int submissionId = service.submitAssignment(
        *student,
        assignmentId,
        "Late assignment",
        "2099-10-21 12:00:00"
    );

    service.markSubmission(
        *lecturer, submissionId, 80.0, "Late submission"
    );

    sqlite3_stmt* statement = nullptr;

    ASSERT_EQ(
        sqlite3_prepare_v2(
            database.handle(),
            "SELECT final_mark FROM mark WHERE submission_id=?;",
            -1,
            &statement,
            nullptr
        ),
        SQLITE_OK
    );

    sqlite3_bind_int(statement, 1, submissionId);

    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);

    EXPECT_DOUBLE_EQ(sqlite3_column_double(statement, 0), 70.0);

    sqlite3_finalize(statement);
}

// ------------------------------------------------------------
// Moderation and release tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, HoDCanApproveAndReleaseMark) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 80.0, "Good work"
    );

    EXPECT_NO_THROW(
        service.moderateAndRelease(
            *hod, submissionId, true, "Approved"
        )
    );

    sqlite3_stmt* statement = nullptr;

    ASSERT_EQ(
        sqlite3_prepare_v2(
            database.handle(),
            "SELECT released FROM mark WHERE submission_id=?;",
            -1,
            &statement,
            nullptr
        ),
        SQLITE_OK
    );

    sqlite3_bind_int(statement, 1, submissionId);

    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);

    EXPECT_EQ(sqlite3_column_int(statement, 0), 1);

    sqlite3_finalize(statement);
}

TEST_F(AssignmentWorkflowTest, HoDCanReturnMarkForReview) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 80.0, "Good work"
    );

    EXPECT_NO_THROW(
        service.moderateAndRelease(
            *hod, submissionId, false, "Please review this mark"
        )
    );
}

TEST_F(AssignmentWorkflowTest, LecturerCannotApproveMarks) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 80.0, "Good work"
    );

    EXPECT_THROW(
        service.moderateAndRelease(
            *lecturer, submissionId, true, "Approved"
        ),
        std::exception
    );
}

// ------------------------------------------------------------
// Mark change tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, LecturerCanChangeUnreleasedMark) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 70.0, "Initial mark"
    );

    EXPECT_NO_THROW(
        service.changeMark(
            *lecturer, submissionId, 85.0, "Correction"
        )
    );
}

TEST_F(AssignmentWorkflowTest, LecturerCannotChangeMarkWithoutReason) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 70.0, "Initial mark"
    );

    EXPECT_THROW(
        service.changeMark(
            *lecturer, submissionId, 85.0, ""
        ),
        std::invalid_argument
    );
}

TEST_F(AssignmentWorkflowTest, LecturerCannotChangeReleasedMark) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 70.0, "Initial mark"
    );

    service.moderateAndRelease(
        *hod, submissionId, true, "Approved"
    );

    EXPECT_THROW(
        service.changeMark(
            *lecturer, submissionId, 85.0, "Correction"
        ),
        std::exception
    );
}

// ------------------------------------------------------------
// Additional mark visibility and audit tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, StudentCanOnlySeeMarkAfterRelease) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 80.0, "Good work"
    );

    // Capture what the student sees before release.
    std::ostringstream beforeRelease;
    std::streambuf* originalOutput = std::cout.rdbuf(
        beforeRelease.rdbuf()
    );

    service.showStudentMarks(*student);

    std::cout.rdbuf(originalOutput);

    // The mark must not be visible before HoD approval.
    EXPECT_EQ(beforeRelease.str().find("80"), std::string::npos);

    // HoD approves and releases the mark.
    service.moderateAndRelease(
        *hod, submissionId, true, "Approved"
    );

    // Capture what the student sees after release.
    std::ostringstream afterRelease;
    originalOutput = std::cout.rdbuf(afterRelease.rdbuf());

    service.showStudentMarks(*student);

    std::cout.rdbuf(originalOutput);

    // The released mark should now be visible.
    EXPECT_NE(afterRelease.str().find("80"), std::string::npos);
}

TEST_F(AssignmentWorkflowTest, ChangingMarkCreatesAuditRecord) {
    const int submissionId = submitOnTime();

    service.markSubmission(
        *lecturer, submissionId, 70.0, "Initial mark"
    );

    // Count audit records before changing the mark.
    sqlite3_stmt* statement = nullptr;

    ASSERT_EQ(
        sqlite3_prepare_v2(
            database.handle(),
            "SELECT COUNT(*) FROM mark_change_audit_record;",
            -1,
            &statement,
            nullptr
        ),
        SQLITE_OK
    );

    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);

    const int beforeChange = sqlite3_column_int(statement, 0);

    sqlite3_finalize(statement);

    // Lecturer changes the mark and provides a reason.
    service.changeMark(
        *lecturer, submissionId, 85.0, "Correction"
    );

    // Count audit records after changing the mark.
    ASSERT_EQ(
        sqlite3_prepare_v2(
            database.handle(),
            "SELECT COUNT(*) FROM mark_change_audit_record;",
            -1,
            &statement,
            nullptr
        ),
        SQLITE_OK
    );

    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);

    const int afterChange = sqlite3_column_int(statement, 0);

    sqlite3_finalize(statement);

    EXPECT_GT(afterChange, beforeChange);
}


// ------------------------------------------------------------
// Additional user management tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, AdministratorCanDeactivateUser) {
    service.setUserActive(*admin, studentId, false);

    EXPECT_THROW(
        service.login("student", "Student123!"),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, AdministratorCanReactivateUser) {
    service.setUserActive(*admin, studentId, false);
    service.setUserActive(*admin, studentId, true);

    auto activeStudent = service.login("student", "Student123!");

    EXPECT_EQ(activeStudent->role(), Role::Student);
}

TEST_F(AssignmentWorkflowTest, AdministratorCannotDeactivateOwnAccount) {
    EXPECT_THROW(
        service.setUserActive(*admin, adminId, false),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, AdministratorCannotDeactivateLastAdministrator) {
    EXPECT_THROW(
        service.setUserActive(*admin, adminId + 1000, false),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, CannotDeactivateLastAdministratorAccount) {
    EXPECT_THROW(
        service.setUserActive(*admin, adminId, false),
        std::exception
    );
}

// ------------------------------------------------------------
// Additional assignment creation tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, CannotCreateAssignmentWithEmptyTitle) {
    EXPECT_THROW(
        service.createAssignment(
            *lecturer, courseId, "",
            "2099-10-20 23:59:59", 100.0
        ),
        std::invalid_argument
    );
}

TEST_F(AssignmentWorkflowTest, CannotCreateAssignmentWithShortDeadline) {
    EXPECT_THROW(
        service.createAssignment(
            *lecturer, courseId, "Test Assignment",
            "2099", 100.0
        ),
        std::invalid_argument
    );
}

TEST_F(AssignmentWorkflowTest, CannotCreateAssignmentWithZeroMaximumMark) {
    EXPECT_THROW(
        service.createAssignment(
            *lecturer, courseId, "Test Assignment",
            "2099-10-20 23:59:59", 0.0
        ),
        std::invalid_argument
    );
}

TEST_F(AssignmentWorkflowTest, CannotCreateAssignmentWithNegativeMaximumMark) {
    EXPECT_THROW(
        service.createAssignment(
            *lecturer, courseId, "Test Assignment",
            "2099-10-20 23:59:59", -10.0
        ),
        std::invalid_argument
    );
}

// ------------------------------------------------------------
// Additional extension tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, CannotGrantExtensionToUnenrolledStudent) {
    const int anotherStudentId = service.createUser(
        *admin, "student2", "Student456!", Role::Student
    );

    EXPECT_THROW(
        service.grantExtension(
            *lecturer, assignmentId, anotherStudentId,
            "2099-10-25 23:59:59", "Valid reason"
        ),
        std::runtime_error
    );
}

TEST_F(AssignmentWorkflowTest, LecturerCanUpdateExistingExtension) {
    service.grantExtension(
        *lecturer, assignmentId, studentId,
        "2099-10-25 23:59:59", "First extension"
    );

    EXPECT_NO_THROW(
        service.grantExtension(
            *lecturer, assignmentId, studentId,
            "2099-10-27 23:59:59", "Updated extension"
        )
    );
}

// ------------------------------------------------------------
// Additional enrolment tests
// ------------------------------------------------------------

TEST_F(AssignmentWorkflowTest, AdministratorCannotEnrolLecturerAsStudent) {
    EXPECT_THROW(
        service.enrolStudent(*admin, lecturerId, courseId),
        std::runtime_error
    );
}

// ------------------------------------------------------------
// Additional tests for uncovered LMS functionality
// ------------------------------------------------------------

// Student registration tests

TEST(LmsServiceTest, StudentCanRegisterSuccessfully) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);

    const int studentId =
        service.registerStudent("newstudent", "Student123!");

    EXPECT_GT(studentId, 0);

    auto student = service.login("newstudent", "Student123!");

    ASSERT_NE(student, nullptr);
    EXPECT_EQ(student->role(), Role::Student);
}

TEST(LmsServiceTest, DuplicateStudentRegistrationIsRejected) {
    Database database(":memory:");
    database.initialiseSchema();

    LmsService service(database);

    service.registerStudent("newstudent", "Student123!");

    EXPECT_THROW(
        service.registerStudent("newstudent", "Another123!"),
        std::exception
    );
}

// Administrator data display tests

TEST_F(AssignmentWorkflowTest, AdministratorCanDisplayAdminData) {
    EXPECT_NO_THROW(
        service.listAdminData(*admin)
    );
}

TEST_F(AssignmentWorkflowTest, StudentCannotDisplayAdminData) {
    EXPECT_THROW(
        service.listAdminData(*student),
        std::exception
    );
}

// Assignment display tests

TEST_F(AssignmentWorkflowTest, StudentCanDisplayAssignments) {
    EXPECT_NO_THROW(
        service.showAssignments(*student)
    );
}

TEST_F(AssignmentWorkflowTest, LecturerCanDisplayAssignments) {
    EXPECT_NO_THROW(
        service.showAssignments(*lecturer)
    );
}

TEST_F(AssignmentWorkflowTest, HeadOfDepartmentCanDisplayAssignments) {
    EXPECT_NO_THROW(
        service.showAssignments(*hod)
    );
}

// Pending moderation display tests

TEST_F(AssignmentWorkflowTest, HeadOfDepartmentCanDisplayPendingModeration) {
    EXPECT_NO_THROW(
        service.showPendingModeration(*hod)
    );
}

TEST_F(AssignmentWorkflowTest, StudentCannotDisplayPendingModeration) {
    EXPECT_THROW(
        service.showPendingModeration(*student),
        std::exception
    );
}

TEST_F(AssignmentWorkflowTest, LecturerCannotDisplayPendingModeration) {
    EXPECT_THROW(
        service.showPendingModeration(*lecturer),
        std::exception
    );
}

// Notification display tests

TEST_F(AssignmentWorkflowTest, StudentCanDisplayNotifications) {
    EXPECT_NO_THROW(
        service.showNotifications(*student)
    );
}

TEST_F(AssignmentWorkflowTest, LecturerCanDisplayNotifications) {
    EXPECT_NO_THROW(
        service.showNotifications(*lecturer)
    );
}

TEST_F(AssignmentWorkflowTest, HeadOfDepartmentCanDisplayNotifications) {
    EXPECT_NO_THROW(
        service.showNotifications(*hod)
    );
}

TEST_F(AssignmentWorkflowTest, AdministratorCanDisplayNotifications) {
    EXPECT_NO_THROW(
        service.showNotifications(*admin)
    );
}

// ------------------------------------------------------------
// Main test runner
// ------------------------------------------------------------

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}