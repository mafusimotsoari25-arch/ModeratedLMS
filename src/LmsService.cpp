#include "LmsService.h"
#include "PasswordService.h"
#include "UserFactory.h"
#include <sqlite3.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace {
class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Statement() { if (stmt_) sqlite3_finalize(stmt_); }
    sqlite3_stmt* get() const { return stmt_; }
    void done() { if (sqlite3_step(stmt_) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_)); }
private:
    sqlite3* db_; sqlite3_stmt* stmt_ = nullptr;
};
std::string roleText(Role role) {
    switch (role) {
        case Role::Student: return "Student";
        case Role::Lecturer: return "Lecturer";
        case Role::HeadOfDepartment: return "HeadOfDepartment";
        case Role::Administrator: return "Administrator";
    }
    throw std::invalid_argument("Invalid role");
}
std::string nowText() {
    std::time_t now = std::time(nullptr); std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buffer[20]; std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm); return buffer;
}

std::time_t parseTime(const std::string& value) {
    std::tm tm{}; std::istringstream input(value); input >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (input.fail()) throw std::invalid_argument("Date/time must use YYYY-MM-DD HH:MM:SS");
    tm.tm_isdst = -1; const auto result = std::mktime(&tm);
    if (result == static_cast<std::time_t>(-1)) throw std::invalid_argument("Invalid date/time");
    return result;
}
int daysLateBetween(const std::string& submitted, const std::string& deadline) {
    const auto seconds = std::difftime(parseTime(submitted), parseTime(deadline));
    if (seconds <= 0) return 0;
    return static_cast<int>(std::ceil(seconds / 86400.0));
}

std::string copySubmissionFile(const std::string& source, int assignmentId, int studentId, int attempt) {
    if (source.empty()) return {};
    namespace fs = std::filesystem;
    const fs::path src(source);
    if (!fs::exists(src) || !fs::is_regular_file(src)) throw std::invalid_argument("Submission file does not exist or is not a regular file");
    const fs::path dir = fs::path("submissions") / ("assignment_" + std::to_string(assignmentId)) / ("student_" + std::to_string(studentId));
    fs::create_directories(dir);
    const fs::path dest = dir / ("attempt_" + std::to_string(attempt) + src.extension().string());
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
    return dest.string();
}

std::string columnText(sqlite3_stmt* s, int col) {
    const auto* value = sqlite3_column_text(s, col);
    return value ? reinterpret_cast<const char*>(value) : "";
}

}

LmsService::LmsService(Database& database) : database_(database), audit_(database), notifications_(database) {}

Role LmsService::roleForUser(int userId) const {
    Statement s(database_.handle(), "SELECT role FROM user WHERE user_id=? AND active=1");
    sqlite3_bind_int(s.get(), 1, userId);
    if (sqlite3_step(s.get()) != SQLITE_ROW) throw std::runtime_error("User account is missing or inactive");
    const auto role = columnText(s.get(), 0);
    if (role == "Student") return Role::Student;
    if (role == "Lecturer") return Role::Lecturer;
    if (role == "HeadOfDepartment") return Role::HeadOfDepartment;
    if (role == "Administrator") return Role::Administrator;
    throw std::runtime_error("Unknown role in database");
}
void LmsService::requireRole(const User& actor, Role required) const {
    if (roleForUser(actor.id()) != required || actor.role() != required) throw std::runtime_error("Access denied: your role cannot perform this action");
}
void LmsService::requireAnyRole(const User& actor, std::initializer_list<Role> allowed) const {
    const Role stored = roleForUser(actor.id());
    if (stored != actor.role() || std::find(allowed.begin(), allowed.end(), stored) == allowed.end())
        throw std::runtime_error("Access denied: your role cannot perform this action");
}

bool LmsService::hasAdministrator() const {
    Statement s(database_.handle(), "SELECT COUNT(*) FROM user WHERE role='Administrator'");
    return sqlite3_step(s.get()) == SQLITE_ROW && sqlite3_column_int(s.get(), 0) > 0;
}

int LmsService::bootstrapAdministrator(const std::string& username, const std::string& password) {
    if (hasAdministrator()) throw std::runtime_error("An administrator already exists; use an administrator account to create more user");
    if (username.empty() || username.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("Username must be non-empty and contain no spaces");
    if (password.size() < 8) throw std::invalid_argument("Password must contain at least 8 characters");
    const auto hash = PasswordService::hashPassword(password);
    Statement s(database_.handle(), "INSERT INTO user(username,password_hash,role,active) VALUES(?,?,'Administrator',1)");
    sqlite3_bind_text(s.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.get(), 2, hash.c_str(), -1, SQLITE_TRANSIENT);
    s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}

void LmsService::seedDemoData() {
    database_.execute("INSERT OR IGNORE INTO department(department_id,name) VALUES(1,'Computing');");
    database_.execute("INSERT OR IGNORE INTO course(course_id,department_id,code,name) VALUES(1,1,'CSE101','Introduction to Computing');");
    struct Demo { const char* name; const char* password; const char* role; };
    const Demo user[] = {{"demo_student","Student123!","Student"},{"demo_lecturer","Lecturer123!","Lecturer"},{"demo_hod","Hod123456!","HeadOfDepartment"},{"demo_admin","Admin123456!","Administrator"}};
    for (const auto& u : user) {
        Statement q(database_.handle(), "INSERT OR IGNORE INTO user(username,password_hash,role) VALUES(?,?,?)");
        sqlite3_bind_text(q.get(),1,u.name,-1,SQLITE_STATIC);
        const auto hash = PasswordService::hashPassword(u.password);
        sqlite3_bind_text(q.get(),2,hash.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(q.get(),3,u.role,-1,SQLITE_STATIC); q.done();
    }
    database_.execute("INSERT OR IGNORE INTO course_lecturer(course_id,lecturer_user_id) SELECT 1,user_id FROM user WHERE username='demo_lecturer';");
    database_.execute("INSERT OR IGNORE INTO department_hod(department_id,hod_user_id) SELECT 1,user_id FROM user WHERE username='demo_hod';");
    database_.execute("INSERT OR IGNORE INTO enrolment(course_id,student_user_id,status) SELECT 1,user_id,'Active' FROM user WHERE username='demo_student';");
    database_.execute("INSERT OR IGNORE INTO assignment(assignment_id,course_id,title,deadline,max_mark) VALUES(1,1,'C++ Basics Assignment','2099-10-20 23:59:59',100);");
}

std::unique_ptr<User> LmsService::login(const std::string& username, const std::string& password) {
    Statement s(database_.handle(), "SELECT user_id,username,password_hash,role FROM user WHERE username=? AND active=1");
    sqlite3_bind_text(s.get(),1,username.c_str(),-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s.get()) != SQLITE_ROW) throw std::runtime_error("Invalid username or password");
    const int id = sqlite3_column_int(s.get(),0); const auto name = columnText(s.get(),1);
    const auto hash = columnText(s.get(),2); const auto role = columnText(s.get(),3);
    if (!PasswordService::verifyPassword(hash,password)) throw std::runtime_error("Invalid username or password");
    return UserFactory::create(id,name,role);
}
int LmsService::registerStudent(const std::string& username, const std::string& password) {
    if (username.empty() || username.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("Username must be non-empty and contain no spaces");
    if (password.size() < 8) throw std::invalid_argument("Password must contain at least 8 characters");
    const auto hash = PasswordService::hashPassword(password);
    Statement s(database_.handle(), "INSERT INTO user(username,password_hash,role) VALUES(?,?,'Student')");
    sqlite3_bind_text(s.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.get(), 2, hash.c_str(), -1, SQLITE_TRANSIENT);
    s.done();
    const int newId = static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
    Statement admins(database_.handle(), "SELECT user_id FROM user WHERE role='Administrator' AND active=1");
    while (sqlite3_step(admins.get()) == SQLITE_ROW)
        notifications_.notifyUser(sqlite3_column_int(admins.get(), 0), "A new student account was registered: " + username + ". Enrolment may be required.");
    return newId;
}

void LmsService::enrolStudent(const User& actor, int studentId, int courseId) {
    requireRole(actor, Role::Administrator);
    Statement role(database_.handle(), "SELECT role FROM user WHERE user_id=? AND active=1");
    sqlite3_bind_int(role.get(), 1, studentId);
    if (sqlite3_step(role.get()) != SQLITE_ROW || columnText(role.get(), 0) != "Student")
        throw std::runtime_error("Only an active student account can be enrolled");
    Statement s(database_.handle(), "INSERT INTO enrolment(course_id,student_user_id,status) VALUES(?,?,'Active')");
    sqlite3_bind_int(s.get(), 1, courseId);
    sqlite3_bind_int(s.get(), 2, studentId);
    s.done();
}

int LmsService::createDepartment(const User& actor, const std::string& name) {
    requireRole(actor, Role::Administrator);
    if (name.empty()) throw std::invalid_argument("Department name is required");
    Statement s(database_.handle(), "INSERT INTO department(name) VALUES(?)");
    sqlite3_bind_text(s.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT); s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}

int LmsService::createCourse(const User& actor, int departmentId, const std::string& code, const std::string& name) {
    requireRole(actor, Role::Administrator);
    if (code.empty() || name.empty()) throw std::invalid_argument("Course code and name are required");
    Statement d(database_.handle(), "SELECT COUNT(*) FROM department WHERE department_id=?");
    sqlite3_bind_int(d.get(), 1, departmentId);
    if (sqlite3_step(d.get()) != SQLITE_ROW || sqlite3_column_int(d.get(), 0) == 0) throw std::runtime_error("Department not found");
    Statement s(database_.handle(), "INSERT INTO course(department_id,code,name) VALUES(?,?,?)");
    sqlite3_bind_int(s.get(), 1, departmentId); sqlite3_bind_text(s.get(), 2, code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.get(), 3, name.c_str(), -1, SQLITE_TRANSIENT); s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}

void LmsService::assignLecturer(const User& actor, int courseId, int lecturerId) {
    requireRole(actor, Role::Administrator);
    Statement u(database_.handle(), "SELECT COUNT(*) FROM user WHERE user_id=? AND role='Lecturer' AND active=1");
    sqlite3_bind_int(u.get(), 1, lecturerId);
    if (sqlite3_step(u.get()) != SQLITE_ROW || sqlite3_column_int(u.get(), 0) == 0) throw std::runtime_error("Active Lecturer account not found");
    Statement s(database_.handle(), "INSERT OR IGNORE INTO course_lecturer(course_id,lecturer_user_id) VALUES(?,?)");
    sqlite3_bind_int(s.get(), 1, courseId); sqlite3_bind_int(s.get(), 2, lecturerId); s.done();
    notifications_.notifyUser(lecturerId, "You have been assigned to course ID " + std::to_string(courseId) + ".");
}

void LmsService::assignHeadOfDepartment(const User& actor, int departmentId, int hodId) {
    requireRole(actor, Role::Administrator);
    Statement u(database_.handle(), "SELECT COUNT(*) FROM user WHERE user_id=? AND role='HeadOfDepartment' AND active=1");
    sqlite3_bind_int(u.get(), 1, hodId);
    if (sqlite3_step(u.get()) != SQLITE_ROW || sqlite3_column_int(u.get(), 0) == 0) throw std::runtime_error("Active HoD account not found");
    Statement s(database_.handle(), "INSERT OR IGNORE INTO department_hod(department_id,hod_user_id) VALUES(?,?)");
    sqlite3_bind_int(s.get(), 1, departmentId); sqlite3_bind_int(s.get(), 2, hodId); s.done();
    notifications_.notifyUser(hodId, "You have been assigned as HoD for department ID " + std::to_string(departmentId) + ".");
}

void LmsService::setUserActive(const User& actor, int userId, bool active) {
    requireRole(actor, Role::Administrator);
    if (actor.id() == userId && !active) throw std::runtime_error("You cannot deactivate your own account");
    Statement check(database_.handle(), "SELECT role FROM user WHERE user_id=?"); sqlite3_bind_int(check.get(), 1, userId);
    if (sqlite3_step(check.get()) != SQLITE_ROW) throw std::runtime_error("User not found");
    if (columnText(check.get(), 0) == "Administrator" && !active) {
        Statement count(database_.handle(), "SELECT COUNT(*) FROM user WHERE role='Administrator' AND active=1");
        if (sqlite3_step(count.get()) == SQLITE_ROW && sqlite3_column_int(count.get(), 0) <= 1)
            throw std::runtime_error("Cannot deactivate the last active administrator");
    }
    Statement s(database_.handle(), "UPDATE user SET active=? WHERE user_id=?");
    sqlite3_bind_int(s.get(), 1, active ? 1 : 0); sqlite3_bind_int(s.get(), 2, userId); s.done();
    notifications_.notifyUser(userId, active ? "Your account has been activated." : "Your account has been deactivated.");
}

void LmsService::listAdminData(const User& actor) {
    requireRole(actor, Role::Administrator);
    auto* db = database_.handle();
    std::cout << "\nUSERS (ID | username | role | status)\n";
    Statement user(db, "SELECT user_id,username,role,active FROM user ORDER BY user_id");
    while (sqlite3_step(user.get()) == SQLITE_ROW)
        std::cout << sqlite3_column_int(user.get(),0) << " | " << columnText(user.get(),1) << " | " << columnText(user.get(),2) << " | " << (sqlite3_column_int(user.get(),3) ? "Active" : "Inactive") << "\n";
    std::cout << "\nDEPARTMENTS (ID | name)\n";
    Statement deps(db, "SELECT department_id,name FROM department ORDER BY department_id");
    while (sqlite3_step(deps.get()) == SQLITE_ROW) std::cout << sqlite3_column_int(deps.get(),0) << " | " << columnText(deps.get(),1) << "\n";
    std::cout << "\nCOURSES (ID | code | name | department ID)\n";
    Statement course(db, "SELECT course_id,code,name,department_id FROM course ORDER BY course_id");
    while (sqlite3_step(course.get()) == SQLITE_ROW) std::cout << sqlite3_column_int(course.get(),0) << " | " << columnText(course.get(),1) << " | " << columnText(course.get(),2) << " | " << sqlite3_column_int(course.get(),3) << "\n";
    std::cout << "\nENROLMENTS (ID | student ID | course ID | status)\n";
    Statement enrol(db, "SELECT enrolment_id,student_user_id,course_id,status FROM enrolment ORDER BY enrolment_id");
    while (sqlite3_step(enrol.get()) == SQLITE_ROW) std::cout << sqlite3_column_int(enrol.get(),0) << " | " << sqlite3_column_int(enrol.get(),1) << " | " << sqlite3_column_int(enrol.get(),2) << " | " << columnText(enrol.get(),3) << "\n";
}

int LmsService::createUser(const User& actor, const std::string& username, const std::string& password, Role role) {
    requireRole(actor,Role::Administrator);
    if (username.empty() || username.find_first_of(" \t\r\n") != std::string::npos) throw std::invalid_argument("Username must be non-empty and contain no spaces");
    if (password.size() < 8) throw std::invalid_argument("Password must contain at least 8 characters");
    const auto hash = PasswordService::hashPassword(password);
    Statement s(database_.handle(), "INSERT INTO user(username,password_hash,role) VALUES(?,?,?)");
    sqlite3_bind_text(s.get(),1,username.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(s.get(),2,hash.c_str(),-1,SQLITE_TRANSIENT);
    const auto roleName = roleText(role); sqlite3_bind_text(s.get(),3,roleName.c_str(),-1,SQLITE_TRANSIENT); s.done();
    const int newId = static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
    notifications_.notifyUser(newId, "Your " + roleName + " account has been created. Contact an administrator if your role or department assignment is incorrect.");
    return newId;
}
int LmsService::createAssignment(const User& actor,int courseId,const std::string& title,const std::string& deadline,double maximumMark) {
    requireRole(actor,Role::Lecturer);
    if (title.empty() || deadline.size()<16 || maximumMark<=0) throw std::invalid_argument("Assignment title, deadline (YYYY-MM-DD HH:MM:SS), and positive maximum mark are required");
    Statement assigned(database_.handle(),"SELECT COUNT(*) FROM course_lecturer WHERE course_id=? AND lecturer_user_id=?");
    sqlite3_bind_int(assigned.get(),1,courseId); sqlite3_bind_int(assigned.get(),2,actor.id());
    if (sqlite3_step(assigned.get())!=SQLITE_ROW || sqlite3_column_int(assigned.get(),0)==0) throw std::runtime_error("You are not assigned to this course");
    Statement s(database_.handle(),"INSERT INTO assignment(course_id,lecturer_user_id,title,description,deadline,max_mark) VALUES(?,?,?,'',?,?)");
    sqlite3_bind_int(s.get(),1,courseId); sqlite3_bind_int(s.get(),2,actor.id()); sqlite3_bind_text(s.get(),3,title.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(s.get(),4,deadline.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_double(s.get(),5,maximumMark); s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}
bool LmsService::isEnrolled(int studentId,int assignmentId) const {
    Statement s(database_.handle(),"SELECT COUNT(*) FROM enrolment e JOIN assignment a ON a.course_id=e.course_id WHERE e.student_user_id=? AND a.assignment_id=? AND e.status='Active'");
    sqlite3_bind_int(s.get(),1,studentId); sqlite3_bind_int(s.get(),2,assignmentId); return sqlite3_step(s.get())==SQLITE_ROW && sqlite3_column_int(s.get(),0)>0;
}
std::string LmsService::effectiveDeadline(int assignmentId,int studentId) const {
    Statement s(database_.handle(),"SELECT COALESCE((SELECT revised_deadline FROM extension WHERE assignment_id=? AND student_user_id=?),(SELECT deadline FROM assignment WHERE assignment_id=?))");
    sqlite3_bind_int(s.get(),1,assignmentId); sqlite3_bind_int(s.get(),2,studentId); sqlite3_bind_int(s.get(),3,assignmentId);
    if (sqlite3_step(s.get())!=SQLITE_ROW) throw std::runtime_error("Assignment not found");
    return columnText(s.get(),0);
}
int LmsService::submitAssignment(const User& actor,int assignmentId,const std::string& content,const std::string& submittedAt,const std::string& filePath) {
    requireRole(actor,Role::Student);
    if (content.empty() && filePath.empty()) throw std::invalid_argument("Enter text or provide a file path");
    if (!isEnrolled(actor.id(),assignmentId)) throw std::runtime_error("You are not enrolled in this assignment's course");
    Statement existing(database_.handle(),"SELECT submission_id FROM submission WHERE assignment_id=? AND student_user_id=?");
    sqlite3_bind_int(existing.get(),1,assignmentId); sqlite3_bind_int(existing.get(),2,actor.id());
    if (sqlite3_step(existing.get())==SQLITE_ROW) throw std::runtime_error("You already submitted this assignment; use resubmit before the deadline");
    const std::string managedFile = copySubmissionFile(filePath, assignmentId, actor.id(), 1);
    const std::string time = submittedAt.empty()?nowText():submittedAt; const auto deadline=effectiveDeadline(assignmentId,actor.id());
    const bool late = time > deadline;
    Statement ins(database_.handle(),"INSERT INTO submission(assignment_id,student_user_id,submitted_at,status,current_version,file_path,text_content,is_late) VALUES(?,?,?,?,1,?,?,?)");
    sqlite3_bind_int(ins.get(),1,assignmentId); sqlite3_bind_int(ins.get(),2,actor.id()); sqlite3_bind_text(ins.get(),3,time.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ins.get(),4,late?"Late":"Submitted",-1,SQLITE_STATIC); sqlite3_bind_text(ins.get(),5,managedFile.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ins.get(),6,content.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(ins.get(),7,late?1:0); ins.done();
    const int id=static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
    Statement ver(database_.handle(),"INSERT INTO submission_version(submission_id,version_number,content,file_path,submitted_at,is_late) VALUES(?,1,?,?,?,?)");
    sqlite3_bind_int(ver.get(),1,id); sqlite3_bind_text(ver.get(),2,content.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ver.get(),3,managedFile.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ver.get(),4,time.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(ver.get(),5,late?1:0); ver.done();
    Statement staff(database_.handle(), "SELECT cl.lecturer_user_id FROM course_lecturer cl JOIN assignment a ON a.course_id=cl.course_id WHERE a.assignment_id=?");
    sqlite3_bind_int(staff.get(), 1, assignmentId);
    while (sqlite3_step(staff.get()) == SQLITE_ROW) notifications_.notifyUser(sqlite3_column_int(staff.get(), 0), "A student submitted assignment ID " + std::to_string(assignmentId) + ".");
    return id;
}
int LmsService::grantExtension(const User& actor,int assignmentId,int studentId,const std::string& revisedDeadline,const std::string& reason) {
    requireRole(actor,Role::Lecturer); if(revisedDeadline.size()<16 || reason.empty()) throw std::invalid_argument("A revised deadline and reason are required");
    Statement assigned(database_.handle(),"SELECT COUNT(*) FROM assignment a JOIN course_lecturer cl ON cl.course_id=a.course_id WHERE a.assignment_id=? AND cl.lecturer_user_id=?");
    sqlite3_bind_int(assigned.get(),1,assignmentId); sqlite3_bind_int(assigned.get(),2,actor.id());
    if(sqlite3_step(assigned.get())!=SQLITE_ROW || sqlite3_column_int(assigned.get(),0)==0) throw std::runtime_error("You are not assigned to this assignment's course");
    if(!isEnrolled(studentId,assignmentId)) throw std::runtime_error("Student is not enrolled in this course");
    Statement s(database_.handle(),"INSERT INTO extension(assignment_id,student_user_id,revised_deadline,granted_by_user_id,reason) VALUES(?,?,?,?,?) ON CONFLICT(assignment_id,student_user_id) DO UPDATE SET revised_deadline=excluded.revised_deadline,granted_by_user_id=excluded.granted_by_user_id,reason=excluded.reason");
    sqlite3_bind_int(s.get(),1,assignmentId); sqlite3_bind_int(s.get(),2,studentId); sqlite3_bind_text(s.get(),3,revisedDeadline.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(s.get(),4,actor.id()); sqlite3_bind_text(s.get(),5,reason.c_str(),-1,SQLITE_TRANSIENT); s.done();
    notifications_.notifyUser(studentId,"An extension was granted for assignment "+std::to_string(assignmentId)+" until "+revisedDeadline+"."); return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}
int LmsService::resubmitAssignment(const User& actor,int assignmentId,const std::string& content,const std::string& submittedAt,const std::string& filePath) {
    requireRole(actor,Role::Student);
    if(content.empty() && filePath.empty()) throw std::invalid_argument("Enter text or provide a file path");
    Statement q(database_.handle(),"SELECT submission_id,current_version,status FROM submission WHERE assignment_id=? AND student_user_id=?");
    sqlite3_bind_int(q.get(),1,assignmentId); sqlite3_bind_int(q.get(),2,actor.id());
    if(sqlite3_step(q.get())!=SQLITE_ROW) throw std::runtime_error("Submit the assignment before resubmitting");
    const int id=sqlite3_column_int(q.get(),0), current=sqlite3_column_int(q.get(),1); const auto status=columnText(q.get(),2);
    if(status=="Released" || status=="Moderated" || status=="Marked") throw std::runtime_error("Resubmission is closed after marking or release");
    const std::string managedFile = copySubmissionFile(filePath, assignmentId, actor.id(), current+1);
    const std::string time=submittedAt.empty()?nowText():submittedAt; const auto deadline=effectiveDeadline(assignmentId,actor.id());
    if(time>deadline) throw std::runtime_error("Resubmission deadline has passed");
    Statement ver(database_.handle(),"INSERT INTO submission_version(submission_id,version_number,content,file_path,submitted_at,is_late) VALUES(?,?,?,?,?,0)");
    sqlite3_bind_int(ver.get(),1,id); sqlite3_bind_int(ver.get(),2,current+1); sqlite3_bind_text(ver.get(),3,content.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ver.get(),4,managedFile.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ver.get(),5,time.c_str(),-1,SQLITE_TRANSIENT); ver.done();
    Statement upd(database_.handle(),"UPDATE submission SET current_version=?,submitted_at=?,status='Submitted',file_path=?,text_content=?,is_late=0 WHERE submission_id=?");
    sqlite3_bind_int(upd.get(),1,current+1); sqlite3_bind_text(upd.get(),2,time.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(upd.get(),3,managedFile.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(upd.get(),4,content.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(upd.get(),5,id); upd.done();
    Statement staff(database_.handle(), "SELECT cl.lecturer_user_id FROM course_lecturer cl JOIN assignment a ON a.course_id=cl.course_id WHERE a.assignment_id=?");
    sqlite3_bind_int(staff.get(), 1, assignmentId);
    while (sqlite3_step(staff.get()) == SQLITE_ROW) notifications_.notifyUser(sqlite3_column_int(staff.get(), 0), "A student resubmitted assignment ID " + std::to_string(assignmentId) + " (version " + std::to_string(current+1) + ").");
    return id;
}
void LmsService::markSubmission(const User& actor,int submissionId,double rawMark,const std::string& feedback) {
    requireRole(actor,Role::Lecturer);
    Statement s(database_.handle(),"SELECT a.max_mark,s.assignment_id,s.student_user_id,s.current_version FROM submission s JOIN assignment a ON a.assignment_id=s.assignment_id JOIN course_lecturer cl ON cl.course_id=a.course_id WHERE s.submission_id=? AND cl.lecturer_user_id=?");
    sqlite3_bind_int(s.get(),1,submissionId); sqlite3_bind_int(s.get(),2,actor.id()); if(sqlite3_step(s.get())!=SQLITE_ROW) throw std::runtime_error("Submission not found or you are not assigned to its course");
    const double maxMark=sqlite3_column_double(s.get(),0); const int assignmentId=sqlite3_column_int(s.get(),1); const int studentId=sqlite3_column_int(s.get(),2);
    Statement latest(database_.handle(),"SELECT submitted_at FROM submission_version WHERE submission_id=? ORDER BY version_number DESC LIMIT 1");
    sqlite3_bind_int(latest.get(),1,submissionId); if(sqlite3_step(latest.get())!=SQLITE_ROW) throw std::runtime_error("Submission version not found");
    const int daysLate=daysLateBetween(columnText(latest.get(),0),effectiveDeadline(assignmentId,studentId));
    const double finalMark=penalty_.finalMark(rawMark,maxMark,daysLate);
    Statement old(database_.handle(),"SELECT final_mark FROM mark WHERE submission_id=?"); sqlite3_bind_int(old.get(),1,submissionId); const bool hasOld=sqlite3_step(old.get())==SQLITE_ROW; const double oldMark=hasOld?sqlite3_column_double(old.get(),0):0;
    database_.execute("BEGIN IMMEDIATE;");
    try {
        Statement save(database_.handle(),"INSERT INTO mark(submission_id,raw_mark,final_mark,feedback,marked_by_user_id,released) VALUES(?,?,?,?,?,0) ON CONFLICT(submission_id) DO UPDATE SET raw_mark=excluded.raw_mark,final_mark=excluded.final_mark,feedback=excluded.feedback,marked_by_user_id=excluded.marked_by_user_id,moderated_by_user_id=NULL,released=0");
        sqlite3_bind_int(save.get(),1,submissionId); sqlite3_bind_double(save.get(),2,rawMark); sqlite3_bind_double(save.get(),3,finalMark); sqlite3_bind_text(save.get(),4,feedback.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(save.get(),5,actor.id()); save.done();
        Statement status(database_.handle(),"UPDATE submission SET status='Marked' WHERE submission_id=?"); sqlite3_bind_int(status.get(),1,submissionId); status.done();
        audit_.recordMarkChange(submissionId,actor.id(),hasOld?std::optional<double>(oldMark):std::nullopt,finalMark,hasOld?"Mark updated by lecturer":"Initial marking");
        database_.execute("COMMIT;");
    } catch (...) { try { database_.execute("ROLLBACK;"); } catch (...) {} throw; }
    notifications_.notifyUser(studentId,"Your assignment has been marked and is awaiting HoD moderation.");
    Statement hods(database_.handle(), "SELECT dh.hod_user_id FROM department_hod dh JOIN course c ON c.department_id=dh.department_id JOIN assignment a ON a.course_id=c.course_id WHERE a.assignment_id=?");
    sqlite3_bind_int(hods.get(), 1, assignmentId);
    while (sqlite3_step(hods.get()) == SQLITE_ROW) notifications_.notifyUser(sqlite3_column_int(hods.get(), 0), "A mark is awaiting moderation for submission " + std::to_string(submissionId) + ".");
}
void LmsService::moderateAndRelease(const User& actor,int submissionId,bool approve,const std::string& comments) {
    requireRole(actor,Role::HeadOfDepartment);
    Statement s(database_.handle(),"SELECT m.final_mark,s.student_user_id FROM mark m JOIN submission s ON s.submission_id=m.submission_id JOIN assignment a ON a.assignment_id=s.assignment_id JOIN course c ON c.course_id=a.course_id JOIN department_hod dh ON dh.department_id=c.department_id WHERE m.submission_id=? AND dh.hod_user_id=? AND m.released=0 AND s.status='Marked'");
    sqlite3_bind_int(s.get(),1,submissionId); sqlite3_bind_int(s.get(),2,actor.id()); if(sqlite3_step(s.get())!=SQLITE_ROW) throw std::runtime_error("No unmoderated mark exists in your department for this submission"); const int student=sqlite3_column_int(s.get(),1);
    Statement markIdQuery(database_.handle(), "SELECT mark_id FROM mark WHERE submission_id=?"); sqlite3_bind_int(markIdQuery.get(),1,submissionId);
    if(sqlite3_step(markIdQuery.get())!=SQLITE_ROW) throw std::runtime_error("Mark record not found");
    const int markId=sqlite3_column_int(markIdQuery.get(),0);
    Statement moderation(database_.handle(), "INSERT INTO moderation(mark_id,hod_user_id,decision,comments) VALUES(?,?,?,?)");
    sqlite3_bind_int(moderation.get(),1,markId); sqlite3_bind_int(moderation.get(),2,actor.id());
    sqlite3_bind_text(moderation.get(),3,approve?"Approved":"Returned",-1,SQLITE_STATIC); sqlite3_bind_text(moderation.get(),4,comments.c_str(),-1,SQLITE_TRANSIENT); moderation.done();
    Statement auditLog(database_.handle(), "INSERT INTO audit_log_entry(user_id,submission_id,action,new_value) VALUES(?,?,?,?)");
    sqlite3_bind_int(auditLog.get(),1,actor.id()); sqlite3_bind_int(auditLog.get(),2,submissionId); sqlite3_bind_text(auditLog.get(),3,approve?"Moderation approved":"Moderation returned",-1,SQLITE_STATIC); sqlite3_bind_text(auditLog.get(),4,comments.c_str(),-1,SQLITE_TRANSIENT); auditLog.done();
    if(!approve) { Statement reset(database_.handle(),"UPDATE submission SET status='Submitted' WHERE submission_id=?"); sqlite3_bind_int(reset.get(),1,submissionId); reset.done(); notifications_.notifyUser(student,"Your mark was returned for lecturer review. "+comments); return; }
    Statement m(database_.handle(),"UPDATE mark SET moderated_by_user_id=?,released=1 WHERE submission_id=?"); sqlite3_bind_int(m.get(),1,actor.id()); sqlite3_bind_int(m.get(),2,submissionId); m.done();
    Statement st(database_.handle(),"UPDATE submission SET status='Released' WHERE submission_id=?"); sqlite3_bind_int(st.get(),1,submissionId); st.done();
    notifications_.notifyUser(student,"Your mark has been approved and released. "+comments);
}
void LmsService::changeMark(const User& actor,int submissionId,double newRawMark,const std::string& reason) {
    requireRole(actor,Role::Lecturer); if(reason.empty()) throw std::invalid_argument("A reason is required for a mark change");
    Statement q(database_.handle(),"SELECT a.max_mark,m.final_mark,s.student_user_id FROM mark m JOIN submission s ON s.submission_id=m.submission_id JOIN assignment a ON a.assignment_id=s.assignment_id JOIN course_lecturer cl ON cl.course_id=a.course_id WHERE s.submission_id=? AND cl.lecturer_user_id=? AND m.released=0");
    sqlite3_bind_int(q.get(),1,submissionId); sqlite3_bind_int(q.get(),2,actor.id()); if(sqlite3_step(q.get())!=SQLITE_ROW) throw std::runtime_error("Mark not found, already released, or not your course");
    const double max=sqlite3_column_double(q.get(),0), old=sqlite3_column_double(q.get(),1); const int student=sqlite3_column_int(q.get(),2);
    Statement timing(database_.handle(), "SELECT s.assignment_id,s.student_user_id,v.submitted_at FROM submission s JOIN submission_version v ON v.submission_id=s.submission_id AND v.version_number=s.current_version WHERE s.submission_id=?");
    sqlite3_bind_int(timing.get(),1,submissionId); if(sqlite3_step(timing.get())!=SQLITE_ROW) throw std::runtime_error("Current submission version not found");
    const int assignmentId=sqlite3_column_int(timing.get(),0), studentId=sqlite3_column_int(timing.get(),1);
    const int daysLate=daysLateBetween(columnText(timing.get(),2),effectiveDeadline(assignmentId,studentId));
    const double final=penalty_.finalMark(newRawMark,max,daysLate);
    database_.execute("BEGIN IMMEDIATE;");
    try {
        Statement u(database_.handle(),"UPDATE mark SET raw_mark=?,final_mark=?,moderated_by_user_id=NULL,released=0 WHERE submission_id=?"); sqlite3_bind_double(u.get(),1,newRawMark); sqlite3_bind_double(u.get(),2,final); sqlite3_bind_int(u.get(),3,submissionId); u.done();
        audit_.recordMarkChange(submissionId,actor.id(),old,final,reason);
        database_.execute("COMMIT;");
    } catch (...) { try { database_.execute("ROLLBACK;"); } catch (...) {} throw; }
    notifications_.notifyUser(student,"Your mark was updated and must be moderated again.");
}
void LmsService::showStudentMarks(const User& actor) {
    requireRole(actor,Role::Student); Statement s(database_.handle(),"SELECT a.title,m.final_mark,m.feedback FROM mark m JOIN submission sub ON sub.submission_id=m.submission_id JOIN assignment a ON a.assignment_id=sub.assignment_id WHERE sub.student_user_id=? AND m.released=1 ORDER BY a.title"); sqlite3_bind_int(s.get(),1,actor.id());
    std::cout<<"\nReleased mark:\n"; bool any=false; while(sqlite3_step(s.get())==SQLITE_ROW){any=true;std::cout<<"- "<<columnText(s.get(),0)<<": "<<sqlite3_column_double(s.get(),1)<<" | "<<columnText(s.get(),2)<<"\n";} if(!any)std::cout<<"No mark released yet.\n";
}
void LmsService::showPendingModeration(const User& actor) {
    requireRole(actor,Role::HeadOfDepartment); Statement s(database_.handle(),"SELECT sub.submission_id,a.title,u.username,m.final_mark FROM mark m JOIN submission sub ON sub.submission_id=m.submission_id JOIN assignment a ON a.assignment_id=sub.assignment_id JOIN course c ON c.course_id=a.course_id JOIN department_hod dh ON dh.department_id=c.department_id JOIN user u ON u.user_id=sub.student_user_id WHERE dh.hod_user_id=? AND m.released=0 AND sub.status='Marked' ORDER BY sub.submission_id");
    sqlite3_bind_int(s.get(),1,actor.id());
    std::cout<<"\nMarks awaiting moderation:\n"; bool any=false; while(sqlite3_step(s.get())==SQLITE_ROW){any=true;std::cout<<"Submission "<<sqlite3_column_int(s.get(),0)<<" | "<<columnText(s.get(),1)<<" | student "<<columnText(s.get(),2)<<" | mark "<<sqlite3_column_double(s.get(),3)<<"\n";} if(!any)std::cout<<"None.\n";
}
void LmsService::showAssignments(const User& actor) {
    requireAnyRole(actor,{Role::Student,Role::Lecturer,Role::HeadOfDepartment});
    const char* sql=actor.role()==Role::Student ? "SELECT a.assignment_id,a.title,a.deadline,a.max_mark FROM assignment a JOIN enrolment e ON e.course_id=a.course_id WHERE e.student_user_id=? AND e.status='Active' ORDER BY a.assignment_id" : "SELECT a.assignment_id,a.title,a.deadline,a.max_mark FROM assignment a JOIN course_lecturer cl ON cl.course_id=a.course_id WHERE cl.lecturer_user_id=? ORDER BY a.assignment_id";
    Statement s(database_.handle(),sql); sqlite3_bind_int(s.get(),1,actor.id()); std::cout<<"\nAssignments:\n"; bool any=false;
    while(sqlite3_step(s.get())==SQLITE_ROW){any=true;std::cout<<"ID "<<sqlite3_column_int(s.get(),0)<<" | "<<columnText(s.get(),1)<<" | deadline "<<columnText(s.get(),2)<<" | max "<<sqlite3_column_double(s.get(),3)<<"\n";} if(!any)std::cout<<"No assignment found.\n";
}
void LmsService::showNotifications(const User& actor) {
    requireAnyRole(actor,{Role::Student,Role::Lecturer,Role::HeadOfDepartment,Role::Administrator});
    Statement s(database_.handle(),"SELECT notification_id,message,created_at,is_read FROM notification WHERE user_id=? ORDER BY notification_id DESC LIMIT 20");
    sqlite3_bind_int(s.get(),1,actor.id());
    std::cout<<"\nNotifications (new items are marked UNREAD):\n";
    bool any=false;
    while(sqlite3_step(s.get())==SQLITE_ROW){
        any=true;
        std::cout<<"- ["<<columnText(s.get(),2)<<"] ["<<(sqlite3_column_int(s.get(),3) ? "READ" : "UNREAD")<<"] "<<columnText(s.get(),1)<<"\n";
    }
    if(!any) std::cout<<"No notification.\n";
    Statement markRead(database_.handle(),"UPDATE notification SET is_read=1 WHERE user_id=? AND is_read=0");
    sqlite3_bind_int(markRead.get(),1,actor.id());
    markRead.done();
}

void LmsService::runApplication() {
    if (!hasAdministrator()) {
        std::cout << "First-time setup: create the initial administrator account.\n";
        std::string username, password;
        std::cout << "Administrator username (no spaces): "; std::cin >> username;
        std::cout << "Administrator password (at least 8 characters): "; std::cin >> password;
        try {
            const int id = bootstrapAdministrator(username, password);
            std::cout << "Initial administrator created (user ID " << id << "). Please log in.\n";
        } catch (const std::exception& e) {
            std::cout << "Setup failed: " << e.what() << "\n"; return;
        }
    }
    for (;;) {
        std::cout << "\n========================================\n"
                  << "       MODERATED LMS - WELCOME\n"
                  << "========================================\n"
                  << "1. Log in (existing account)\n"
                  << "0. Exit\n"
                  << "New accounts are created by the Administrator.\nChoose an option: ";
        int choice; if (!(std::cin >> choice)) return; if (choice == 0) return;
        if (choice != 1) { std::cout << "Invalid choice. The Administrator creates all accounts.\n"; continue; }
        std::string username, password;
        int loginRole = 0;
        std::cout << "LOG IN AS:\n  1. Student\n  2. Lecturer\n  3. Head of Department (HoD)\n  4. Administrator\nChoose your role: ";
        if (!(std::cin >> loginRole) || loginRole < 1 || loginRole > 4) { std::cin.clear(); std::cin.ignore(10000, '\n'); std::cout << "Invalid role choice.\n"; continue; }
        const std::string expectedRole = loginRole == 1 ? "Student" : loginRole == 2 ? "Lecturer" : loginRole == 3 ? "Head of Department" : "Administrator";
        std::cout << "Username: "; std::cin >> username; std::cout << "Password: "; std::cin >> password;
        try {
            auto user = login(username, password);
            if (user->roleName() != expectedRole) throw std::runtime_error("This account is not registered as " + expectedRole + ". Choose the correct role.");
            std::cout << "Welcome " << user->username() << " (" << user->roleName() << ")\n";
            bool logout = false;
            while (!logout) {
                if (user->role() == Role::Student) std::cout << "\nSTUDENT: 1 Assignments 2 Submit 3 Resubmit 4 Released mark 5 Notifications 0 Logout\n";
                else if (user->role() == Role::Lecturer) std::cout << "\nLECTURER: 1 Assignments 2 Grant extension 3 Mark submission 4 Create assignment 5 Notifications 0 Logout\n";
                else if (user->role() == Role::HeadOfDepartment) std::cout << "\nHOD: 1 Pending moderation 2 Approve/release 3 Notifications 0 Logout\n";
                else std::cout << "\nADMINISTRATION\n1 Create user account  2 Create department  3 Create course  4 Assign lecturer to course  5 Assign HoD to department  6 Enrol student  7 Activate/deactivate account  8 View user/department/course/enrolment  9 Notifications  0 Logout\n";
                std::cout << "Choice: "; int c; if (!(std::cin >> c)) return; if (c == 0) { logout = true; continue; }
                try {
                    if (user->role() == Role::Student) {
                        if (c == 1) showAssignments(*user);
                        else if (c == 2 || c == 3) {
                            int aid, mode; std::string content, filePath;
                            std::cout << "Assignment ID: "; std::cin >> aid;
                            std::cout << "Submission type: 1 Text, 2 File, 3 File and text: "; std::cin >> mode;
                            if (mode < 1 || mode > 3) throw std::invalid_argument("Choose 1, 2, or 3");
                            if (mode == 1 || mode == 3) { std::cout << "Enter text (spaces allowed): "; std::getline(std::cin >> std::ws, content); }
                            if (mode == 2 || mode == 3) { std::cout << "Full path to file: "; std::getline(std::cin >> std::ws, filePath); }
                            int id = c == 2 ? submitAssignment(*user, aid, content, "", filePath) : resubmitAssignment(*user, aid, content, "", filePath);
                            std::cout << "Saved submission ID " << id << "\n";
                        }
                        else if (c == 4) showStudentMarks(*user); else if (c == 5) showNotifications(*user); else std::cout << "Invalid choice.\n";
                    } else if (user->role() == Role::Lecturer) {
                        if (c == 1) showAssignments(*user);
                        else if (c == 2) { int aid, sid; std::string deadline, reason; std::cout << "Assignment ID: "; std::cin >> aid; std::cout << "Student user ID: "; std::cin >> sid; std::cout << "Revised deadline YYYY-MM-DD_HH:MM:SS: "; std::cin >> deadline; std::replace(deadline.begin(), deadline.end(), '_', ' '); std::cout << "Reason (use underscores for spaces): "; std::cin >> reason; grantExtension(*user, aid, sid, deadline, reason); std::cout << "Extension saved.\n"; }
                        else if (c == 3) { int sid; double mark; std::string feedback; std::cout << "Submission ID: "; std::cin >> sid; std::cout << "Raw mark: "; std::cin >> mark; std::cout << "Feedback (use underscores for spaces): "; std::cin >> feedback; markSubmission(*user, sid, mark, feedback); std::cout << "Mark saved; waiting for HoD.\n"; }
                        else if (c == 4) { int course; double max; std::string title, deadline; std::cout << "Course ID: "; std::cin >> course; std::cout << "Title (use underscores for spaces): "; std::cin >> title; std::cout << "Deadline YYYY-MM-DD_HH:MM:SS: "; std::cin >> deadline; std::replace(deadline.begin(), deadline.end(), '_', ' '); std::cout << "Maximum mark: "; std::cin >> max; std::cout << "Created assignment ID " << createAssignment(*user, course, title, deadline, max) << "\n"; }
                        else if (c == 5) showNotifications(*user); else std::cout << "Invalid choice.\n";
                    } else if (user->role() == Role::HeadOfDepartment) {
                        if (c == 1) showPendingModeration(*user);
                        else if (c == 2) { int sid; char yes; std::string comments; std::cout << "Submission ID: "; std::cin >> sid; std::cout << "Approve and release? (y/n): "; std::cin >> yes; std::cout << "Comments (one token, or -): "; std::cin >> comments; moderateAndRelease(*user, sid, yes == 'y' || yes == 'Y', comments == "-" ? "" : comments); std::cout << "Moderation decision saved.\n"; }
                        else if (c == 3) showNotifications(*user); else std::cout << "Invalid choice.\n";
                    } else {
                        if (c == 1) { std::string name, pass; int r; std::cout << "New username: "; std::cin >> name; std::cout << "Temporary password (8+ characters): "; std::cin >> pass; std::cout << "Role: 1 Student, 2 Lecturer, 3 HoD, 4 Administrator: "; std::cin >> r; if (r < 1 || r > 4) throw std::invalid_argument("Invalid role number"); Role role = r == 1 ? Role::Student : r == 2 ? Role::Lecturer : r == 3 ? Role::HeadOfDepartment : Role::Administrator; std::cout << "Created user ID " << createUser(*user, name, pass, role) << "\n"; }
                        else if (c == 2) { std::string name; std::cout << "Department name (use underscores for spaces): "; std::cin >> name; std::replace(name.begin(), name.end(), '_', ' '); std::cout << "Created department ID " << createDepartment(*user, name) << "\n"; }
                        else if (c == 3) { int dept; std::string code, name; std::cout << "Department ID: "; std::cin >> dept; std::cout << "Course code: "; std::cin >> code; std::cout << "Course name (use underscores for spaces): "; std::cin >> name; std::replace(name.begin(), name.end(), '_', ' '); std::cout << "Created course ID " << createCourse(*user, dept, code, name) << "\n"; }
                        else if (c == 4) { int course, lecturer; std::cout << "Course ID: "; std::cin >> course; std::cout << "Lecturer user ID: "; std::cin >> lecturer; assignLecturer(*user, course, lecturer); std::cout << "Lecturer assigned.\n"; }
                        else if (c == 5) { int dept, hod; std::cout << "Department ID: "; std::cin >> dept; std::cout << "HoD user ID: "; std::cin >> hod; assignHeadOfDepartment(*user, dept, hod); std::cout << "HoD assigned.\n"; }
                        else if (c == 6) { int sid, cid; std::cout << "Student user ID: "; std::cin >> sid; std::cout << "Course ID: "; std::cin >> cid; enrolStudent(*user, sid, cid); std::cout << "Student enrolled.\n"; }
                        else if (c == 7) { int id, active; std::cout << "User ID: "; std::cin >> id; std::cout << "1 Activate, 0 Deactivate: "; std::cin >> active; if (active != 0 && active != 1) throw std::invalid_argument("Enter 1 or 0"); setUserActive(*user, id, active == 1); std::cout << "Account status updated.\n"; }
                        else if (c == 8) listAdminData(*user);
                        else if (c == 9) showNotifications(*user);
                        else std::cout << "Invalid choice.\n";
                    }
                } catch (const std::exception& e) { std::cout << "Action failed: " << e.what() << "\n"; }
            }
        } catch (const std::exception& e) { std::cout << "Login failed: " << e.what() << "\n"; }
    }
}
