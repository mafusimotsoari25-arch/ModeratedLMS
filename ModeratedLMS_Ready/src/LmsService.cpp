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

std::string columnText(sqlite3_stmt* s, int col) {
    const auto* value = sqlite3_column_text(s, col);
    return value ? reinterpret_cast<const char*>(value) : "";
}

}

LmsService::LmsService(Database& database) : database_(database), audit_(database), notifications_(database) {}

Role LmsService::roleForUser(int userId) const {
    Statement s(database_.handle(), "SELECT role FROM users WHERE user_id=? AND active=1");
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

void LmsService::seedDemoData() {
    database_.execute("INSERT OR IGNORE INTO departments(department_id,name) VALUES(1,'Computing');");
    database_.execute("INSERT OR IGNORE INTO courses(course_id,department_id,code,name) VALUES(1,1,'CSE101','Introduction to Computing');");
    struct Demo { const char* name; const char* password; const char* role; };
    const Demo users[] = {{"demo_student","Student123!","Student"},{"demo_lecturer","Lecturer123!","Lecturer"},{"demo_hod","Hod123456!","HeadOfDepartment"},{"demo_admin","Admin123456!","Administrator"}};
    for (const auto& u : users) {
        Statement q(database_.handle(), "INSERT INTO users(username,password_hash,role) VALUES(?,?,?) ON CONFLICT(username) DO UPDATE SET password_hash=excluded.password_hash,role=excluded.role,active=1");
        sqlite3_bind_text(q.get(),1,u.name,-1,SQLITE_STATIC);
        const auto hash = PasswordService::hashPassword(u.password);
        sqlite3_bind_text(q.get(),2,hash.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(q.get(),3,u.role,-1,SQLITE_STATIC); q.done();
    }
    database_.execute("INSERT OR IGNORE INTO course_lecturers(course_id,lecturer_user_id) SELECT 1,user_id FROM users WHERE username='demo_lecturer';");
    database_.execute("INSERT OR IGNORE INTO enrolments(course_id,student_user_id,status) SELECT 1,user_id,'Active' FROM users WHERE username='demo_student';");
    database_.execute("INSERT OR IGNORE INTO assignments(assignment_id,course_id,title,deadline,max_mark) VALUES(1,1,'C++ Basics Assignment','2099-10-20 23:59:59',100);");
}

std::unique_ptr<User> LmsService::login(const std::string& username, const std::string& password) {
    Statement s(database_.handle(), "SELECT user_id,username,password_hash,role FROM users WHERE username=? AND active=1");
    sqlite3_bind_text(s.get(),1,username.c_str(),-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s.get()) != SQLITE_ROW) throw std::runtime_error("Invalid username or password");
    const int id = sqlite3_column_int(s.get(),0); const auto name = columnText(s.get(),1);
    const auto hash = columnText(s.get(),2); const auto role = columnText(s.get(),3);
    if (!PasswordService::verifyPassword(hash,password)) throw std::runtime_error("Invalid username or password");
    return UserFactory::create(id,name,role);
}
int LmsService::createUser(const User& actor, const std::string& username, const std::string& password, Role role) {
    requireRole(actor,Role::Administrator);
    if (username.empty() || username.find_first_of(" \t\r\n") != std::string::npos) throw std::invalid_argument("Username must be non-empty and contain no spaces");
    const auto hash = PasswordService::hashPassword(password);
    Statement s(database_.handle(), "INSERT INTO users(username,password_hash,role) VALUES(?,?,?)");
    sqlite3_bind_text(s.get(),1,username.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(s.get(),2,hash.c_str(),-1,SQLITE_TRANSIENT);
    const auto roleName = roleText(role); sqlite3_bind_text(s.get(),3,roleName.c_str(),-1,SQLITE_TRANSIENT); s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}
int LmsService::createAssignment(const User& actor,int courseId,const std::string& title,const std::string& deadline,double maximumMark) {
    requireRole(actor,Role::Lecturer);
    if (title.empty() || deadline.size()<16 || maximumMark<=0) throw std::invalid_argument("Assignment title, deadline (YYYY-MM-DD HH:MM:SS), and positive maximum mark are required");
    Statement assigned(database_.handle(),"SELECT COUNT(*) FROM course_lecturers WHERE course_id=? AND lecturer_user_id=?");
    sqlite3_bind_int(assigned.get(),1,courseId); sqlite3_bind_int(assigned.get(),2,actor.id());
    if (sqlite3_step(assigned.get())!=SQLITE_ROW || sqlite3_column_int(assigned.get(),0)==0) throw std::runtime_error("You are not assigned to this course");
    Statement s(database_.handle(),"INSERT INTO assignments(course_id,title,deadline,max_mark) VALUES(?,?,?,?)");
    sqlite3_bind_int(s.get(),1,courseId); sqlite3_bind_text(s.get(),2,title.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(s.get(),3,deadline.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_double(s.get(),4,maximumMark); s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}
bool LmsService::isEnrolled(int studentId,int assignmentId) const {
    Statement s(database_.handle(),"SELECT COUNT(*) FROM enrolments e JOIN assignments a ON a.course_id=e.course_id WHERE e.student_user_id=? AND a.assignment_id=? AND e.status='Active'");
    sqlite3_bind_int(s.get(),1,studentId); sqlite3_bind_int(s.get(),2,assignmentId); return sqlite3_step(s.get())==SQLITE_ROW && sqlite3_column_int(s.get(),0)>0;
}
std::string LmsService::effectiveDeadline(int assignmentId,int studentId) const {
    Statement s(database_.handle(),"SELECT COALESCE((SELECT revised_deadline FROM extensions WHERE assignment_id=? AND student_user_id=?),(SELECT deadline FROM assignments WHERE assignment_id=?))");
    sqlite3_bind_int(s.get(),1,assignmentId); sqlite3_bind_int(s.get(),2,studentId); sqlite3_bind_int(s.get(),3,assignmentId);
    if (sqlite3_step(s.get())!=SQLITE_ROW) throw std::runtime_error("Assignment not found");
    return columnText(s.get(),0);
}
int LmsService::submitAssignment(const User& actor,int assignmentId,const std::string& content,const std::string& submittedAt) {
    requireRole(actor,Role::Student); if (content.empty()) throw std::invalid_argument("Submission content cannot be empty");
    if (!isEnrolled(actor.id(),assignmentId)) throw std::runtime_error("You are not enrolled in this assignment's course");
    Statement existing(database_.handle(),"SELECT submission_id FROM submissions WHERE assignment_id=? AND student_user_id=?");
    sqlite3_bind_int(existing.get(),1,assignmentId); sqlite3_bind_int(existing.get(),2,actor.id());
    if (sqlite3_step(existing.get())==SQLITE_ROW) throw std::runtime_error("You already submitted this assignment; use resubmit before the deadline");
    const std::string time = submittedAt.empty()?nowText():submittedAt; const auto deadline=effectiveDeadline(assignmentId,actor.id());
    const bool late = time > deadline;
    Statement ins(database_.handle(),"INSERT INTO submissions(assignment_id,student_user_id,submitted_at,status,current_version) VALUES(?,?,?,?,1)");
    sqlite3_bind_int(ins.get(),1,assignmentId); sqlite3_bind_int(ins.get(),2,actor.id()); sqlite3_bind_text(ins.get(),3,time.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ins.get(),4,late?"Late":"Submitted",-1,SQLITE_STATIC); ins.done();
    const int id=static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
    Statement ver(database_.handle(),"INSERT INTO submission_versions(submission_id,version_number,content,submitted_at,is_late) VALUES(?,1,?,?,?)");
    sqlite3_bind_int(ver.get(),1,id); sqlite3_bind_text(ver.get(),2,content.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ver.get(),3,time.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(ver.get(),4,late?1:0); ver.done();
    notifications_.notifyUser(2,"A student submitted assignment ID "+std::to_string(assignmentId)+"."); return id;
}
int LmsService::grantExtension(const User& actor,int assignmentId,int studentId,const std::string& revisedDeadline,const std::string& reason) {
    requireRole(actor,Role::Lecturer); if(revisedDeadline.size()<16 || reason.empty()) throw std::invalid_argument("A revised deadline and reason are required");
    Statement assigned(database_.handle(),"SELECT COUNT(*) FROM assignments a JOIN course_lecturers cl ON cl.course_id=a.course_id WHERE a.assignment_id=? AND cl.lecturer_user_id=?");
    sqlite3_bind_int(assigned.get(),1,assignmentId); sqlite3_bind_int(assigned.get(),2,actor.id());
    if(sqlite3_step(assigned.get())!=SQLITE_ROW || sqlite3_column_int(assigned.get(),0)==0) throw std::runtime_error("You are not assigned to this assignment's course");
    if(!isEnrolled(studentId,assignmentId)) throw std::runtime_error("Student is not enrolled in this course");
    Statement s(database_.handle(),"INSERT INTO extensions(assignment_id,student_user_id,revised_deadline,granted_by_user_id,reason) VALUES(?,?,?,?,?) ON CONFLICT(assignment_id,student_user_id) DO UPDATE SET revised_deadline=excluded.revised_deadline,granted_by_user_id=excluded.granted_by_user_id,reason=excluded.reason");
    sqlite3_bind_int(s.get(),1,assignmentId); sqlite3_bind_int(s.get(),2,studentId); sqlite3_bind_text(s.get(),3,revisedDeadline.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(s.get(),4,actor.id()); sqlite3_bind_text(s.get(),5,reason.c_str(),-1,SQLITE_TRANSIENT); s.done();
    notifications_.notifyUser(studentId,"An extension was granted for assignment "+std::to_string(assignmentId)+" until "+revisedDeadline+"."); return static_cast<int>(sqlite3_last_insert_rowid(database_.handle()));
}
int LmsService::resubmitAssignment(const User& actor,int assignmentId,const std::string& content,const std::string& submittedAt) {
    requireRole(actor,Role::Student); if(content.empty()) throw std::invalid_argument("Submission content cannot be empty");
    Statement q(database_.handle(),"SELECT submission_id,current_version,status FROM submissions WHERE assignment_id=? AND student_user_id=?");
    sqlite3_bind_int(q.get(),1,assignmentId); sqlite3_bind_int(q.get(),2,actor.id());
    if(sqlite3_step(q.get())!=SQLITE_ROW) throw std::runtime_error("Submit the assignment before resubmitting");
    const int id=sqlite3_column_int(q.get(),0), current=sqlite3_column_int(q.get(),1); const auto status=columnText(q.get(),2);
    if(status=="Released" || status=="Moderated" || status=="Marked") throw std::runtime_error("Resubmission is closed after marking or release");
    const std::string time=submittedAt.empty()?nowText():submittedAt; const auto deadline=effectiveDeadline(assignmentId,actor.id());
    if(time>deadline) throw std::runtime_error("Resubmission deadline has passed");
    Statement ver(database_.handle(),"INSERT INTO submission_versions(submission_id,version_number,content,submitted_at,is_late) VALUES(?,?,?,?,0)");
    sqlite3_bind_int(ver.get(),1,id); sqlite3_bind_int(ver.get(),2,current+1); sqlite3_bind_text(ver.get(),3,content.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(ver.get(),4,time.c_str(),-1,SQLITE_TRANSIENT); ver.done();
    Statement upd(database_.handle(),"UPDATE submissions SET current_version=?,submitted_at=?,status='Submitted' WHERE submission_id=?");
    sqlite3_bind_int(upd.get(),1,current+1); sqlite3_bind_text(upd.get(),2,time.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(upd.get(),3,id); upd.done();
    notifications_.notifyUser(2,"A student resubmitted assignment ID "+std::to_string(assignmentId)+" (version "+std::to_string(current+1)+")."); return id;
}
void LmsService::markSubmission(const User& actor,int submissionId,double rawMark,const std::string& feedback) {
    requireRole(actor,Role::Lecturer);
    Statement s(database_.handle(),"SELECT a.max_mark,s.assignment_id,s.student_user_id,s.current_version FROM submissions s JOIN assignments a ON a.assignment_id=s.assignment_id JOIN course_lecturers cl ON cl.course_id=a.course_id WHERE s.submission_id=? AND cl.lecturer_user_id=?");
    sqlite3_bind_int(s.get(),1,submissionId); sqlite3_bind_int(s.get(),2,actor.id()); if(sqlite3_step(s.get())!=SQLITE_ROW) throw std::runtime_error("Submission not found or you are not assigned to its course");
    const double maxMark=sqlite3_column_double(s.get(),0); const int assignmentId=sqlite3_column_int(s.get(),1); const int studentId=sqlite3_column_int(s.get(),2);
    Statement latest(database_.handle(),"SELECT submitted_at FROM submission_versions WHERE submission_id=? ORDER BY version_number DESC LIMIT 1");
    sqlite3_bind_int(latest.get(),1,submissionId); if(sqlite3_step(latest.get())!=SQLITE_ROW) throw std::runtime_error("Submission version not found");
    const int daysLate=daysLateBetween(columnText(latest.get(),0),effectiveDeadline(assignmentId,studentId));
    const double finalMark=penalty_.finalMark(rawMark,maxMark,daysLate);
    Statement old(database_.handle(),"SELECT final_mark FROM marks WHERE submission_id=?"); sqlite3_bind_int(old.get(),1,submissionId); const bool hasOld=sqlite3_step(old.get())==SQLITE_ROW; const double oldMark=hasOld?sqlite3_column_double(old.get(),0):0;
    Statement save(database_.handle(),"INSERT INTO marks(submission_id,raw_mark,final_mark,feedback,marked_by_user_id,released) VALUES(?,?,?,?,?,0) ON CONFLICT(submission_id) DO UPDATE SET raw_mark=excluded.raw_mark,final_mark=excluded.final_mark,feedback=excluded.feedback,marked_by_user_id=excluded.marked_by_user_id,moderated_by_user_id=NULL,released=0");
    sqlite3_bind_int(save.get(),1,submissionId); sqlite3_bind_double(save.get(),2,rawMark); sqlite3_bind_double(save.get(),3,finalMark); sqlite3_bind_text(save.get(),4,feedback.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(save.get(),5,actor.id()); save.done();
    Statement status(database_.handle(),"UPDATE submissions SET status='Marked' WHERE submission_id=?"); sqlite3_bind_int(status.get(),1,submissionId); status.done();
    audit_.recordMarkChange(submissionId,actor.id(),hasOld?std::optional<double>(oldMark):std::nullopt,finalMark,hasOld?"Mark updated by lecturer":"Initial marking");
    notifications_.notifyUser(studentId,"Your assignment has been marked and is awaiting HoD moderation.");
    notifications_.notifyUser(3,"A mark is awaiting moderation.");
}
void LmsService::moderateAndRelease(const User& actor,int submissionId,bool approve,const std::string& comments) {
    requireRole(actor,Role::HeadOfDepartment);
    Statement s(database_.handle(),"SELECT m.final_mark,s.student_user_id FROM marks m JOIN submissions s ON s.submission_id=m.submission_id WHERE m.submission_id=? AND m.released=0 AND s.status='Marked'");
    sqlite3_bind_int(s.get(),1,submissionId); if(sqlite3_step(s.get())!=SQLITE_ROW) throw std::runtime_error("No unmoderated mark exists for this submission"); const int student=sqlite3_column_int(s.get(),1);
    if(!approve) { Statement reset(database_.handle(),"UPDATE submissions SET status='Submitted' WHERE submission_id=?"); sqlite3_bind_int(reset.get(),1,submissionId); reset.done(); notifications_.notifyUser(student,"Your mark was returned for lecturer review. "+comments); return; }
    Statement m(database_.handle(),"UPDATE marks SET moderated_by_user_id=?,released=1 WHERE submission_id=?"); sqlite3_bind_int(m.get(),1,actor.id()); sqlite3_bind_int(m.get(),2,submissionId); m.done();
    Statement st(database_.handle(),"UPDATE submissions SET status='Released' WHERE submission_id=?"); sqlite3_bind_int(st.get(),1,submissionId); st.done();
    notifications_.notifyUser(student,"Your mark has been approved and released. "+comments);
}
void LmsService::changeMark(const User& actor,int submissionId,double newRawMark,const std::string& reason) {
    requireRole(actor,Role::Lecturer); if(reason.empty()) throw std::invalid_argument("A reason is required for a mark change");
    Statement q(database_.handle(),"SELECT a.max_mark,m.final_mark,s.student_user_id FROM marks m JOIN submissions s ON s.submission_id=m.submission_id JOIN assignments a ON a.assignment_id=s.assignment_id JOIN course_lecturers cl ON cl.course_id=a.course_id WHERE s.submission_id=? AND cl.lecturer_user_id=? AND m.released=0");
    sqlite3_bind_int(q.get(),1,submissionId); sqlite3_bind_int(q.get(),2,actor.id()); if(sqlite3_step(q.get())!=SQLITE_ROW) throw std::runtime_error("Mark not found, already released, or not your course");
    const double max=sqlite3_column_double(q.get(),0), old=sqlite3_column_double(q.get(),1); const int student=sqlite3_column_int(q.get(),2); const double final=penalty_.finalMark(newRawMark,max,0);
    Statement u(database_.handle(),"UPDATE marks SET raw_mark=?,final_mark=?,moderated_by_user_id=NULL,released=0 WHERE submission_id=?"); sqlite3_bind_double(u.get(),1,newRawMark); sqlite3_bind_double(u.get(),2,final); sqlite3_bind_int(u.get(),3,submissionId); u.done();
    audit_.recordMarkChange(submissionId,actor.id(),old,final,reason); notifications_.notifyUser(student,"Your mark was updated and must be moderated again.");
}
void LmsService::showStudentMarks(const User& actor) {
    requireRole(actor,Role::Student); Statement s(database_.handle(),"SELECT a.title,m.final_mark,m.feedback FROM marks m JOIN submissions sub ON sub.submission_id=m.submission_id JOIN assignments a ON a.assignment_id=sub.assignment_id WHERE sub.student_user_id=? AND m.released=1 ORDER BY a.title"); sqlite3_bind_int(s.get(),1,actor.id());
    std::cout<<"\nReleased marks:\n"; bool any=false; while(sqlite3_step(s.get())==SQLITE_ROW){any=true;std::cout<<"- "<<columnText(s.get(),0)<<": "<<sqlite3_column_double(s.get(),1)<<" | "<<columnText(s.get(),2)<<"\n";} if(!any)std::cout<<"No marks released yet.\n";
}
void LmsService::showPendingModeration(const User& actor) {
    requireRole(actor,Role::HeadOfDepartment); Statement s(database_.handle(),"SELECT sub.submission_id,a.title,u.username,m.final_mark FROM marks m JOIN submissions sub ON sub.submission_id=m.submission_id JOIN assignments a ON a.assignment_id=sub.assignment_id JOIN users u ON u.user_id=sub.student_user_id WHERE m.released=0 AND sub.status='Marked' ORDER BY sub.submission_id");
    std::cout<<"\nMarks awaiting moderation:\n"; bool any=false; while(sqlite3_step(s.get())==SQLITE_ROW){any=true;std::cout<<"Submission "<<sqlite3_column_int(s.get(),0)<<" | "<<columnText(s.get(),1)<<" | student "<<columnText(s.get(),2)<<" | mark "<<sqlite3_column_double(s.get(),3)<<"\n";} if(!any)std::cout<<"None.\n";
}
void LmsService::showAssignments(const User& actor) {
    requireAnyRole(actor,{Role::Student,Role::Lecturer,Role::HeadOfDepartment});
    const char* sql=actor.role()==Role::Student ? "SELECT a.assignment_id,a.title,a.deadline,a.max_mark FROM assignments a JOIN enrolments e ON e.course_id=a.course_id WHERE e.student_user_id=? AND e.status='Active' ORDER BY a.assignment_id" : "SELECT a.assignment_id,a.title,a.deadline,a.max_mark FROM assignments a JOIN course_lecturers cl ON cl.course_id=a.course_id WHERE cl.lecturer_user_id=? ORDER BY a.assignment_id";
    Statement s(database_.handle(),sql); sqlite3_bind_int(s.get(),1,actor.id()); std::cout<<"\nAssignments:\n"; bool any=false;
    while(sqlite3_step(s.get())==SQLITE_ROW){any=true;std::cout<<"ID "<<sqlite3_column_int(s.get(),0)<<" | "<<columnText(s.get(),1)<<" | deadline "<<columnText(s.get(),2)<<" | max "<<sqlite3_column_double(s.get(),3)<<"\n";} if(!any)std::cout<<"No assignments found.\n";
}
void LmsService::showNotifications(const User& actor) { requireAnyRole(actor,{Role::Student,Role::Lecturer,Role::HeadOfDepartment,Role::Administrator}); Statement s(database_.handle(),"SELECT message,created_at FROM notifications WHERE user_id=? ORDER BY notification_id DESC LIMIT 20"); sqlite3_bind_int(s.get(),1,actor.id()); std::cout<<"\nNotifications:\n"; bool any=false; while(sqlite3_step(s.get())==SQLITE_ROW){any=true;std::cout<<"- ["<<columnText(s.get(),1)<<"] "<<columnText(s.get(),0)<<"\n";} if(!any)std::cout<<"No notifications.\n"; }

void LmsService::runDemonstration() {
    seedDemoData();
    std::cout << "Demo accounts (change these passwords before real use):\n"
              << "Student: demo_student / Student123!\nLecturer: demo_lecturer / Lecturer123!\n"
              << "HoD: demo_hod / Hod123456!\nAdmin: demo_admin / Admin123456!\n";
    for (;;) {
        std::cout << "\n1 Login  0 Exit\nChoice: "; int choice; if(!(std::cin>>choice)) return; if(choice==0)return; if(choice!=1){std::cout<<"Invalid choice.\n";continue;}
        std::string username,password; std::cout<<"Username: ";std::cin>>username;std::cout<<"Password: ";std::cin>>password;
        try {
            auto user=login(username,password); std::cout<<"Welcome "<<user->username()<<" ("<<user->roleName()<<")\n";
            bool logout=false; while(!logout){
                if(user->role()==Role::Student) std::cout<<"1 List assignments  2 Submit  3 Resubmit  4 View released marks  5 Notifications  0 Logout\n";
                else if(user->role()==Role::Lecturer) std::cout<<"1 List assignments  2 Grant extension  3 Mark submission  4 Create assignment  5 Notifications  0 Logout\n";
                else if(user->role()==Role::HeadOfDepartment) std::cout<<"1 Pending moderation  2 Approve/release  3 Notifications  0 Logout\n";
                else std::cout<<"1 Create user  2 Notifications  0 Logout\n";
                std::cout<<"Choice: "; int c; if(!(std::cin>>c))return; if(c==0){logout=true;continue;}
                try {
                    if(user->role()==Role::Student){
                        if(c==1)showAssignments(*user); else if(c==2||c==3){int aid;std::string content;std::cout<<"Assignment ID: ";std::cin>>aid;std::cout<<"Submission text (one line, use underscores for spaces): ";std::cin>>content;int id=c==2?submitAssignment(*user,aid,content):resubmitAssignment(*user,aid,content);std::cout<<"Saved submission ID "<<id<<"\n";} else if(c==4)showStudentMarks(*user);else if(c==5)showNotifications(*user);else std::cout<<"Invalid choice.\n";
                    } else if(user->role()==Role::Lecturer){
                        if(c==1)showAssignments(*user); else if(c==2){int aid,sid;std::string deadline,reason;std::cout<<"Assignment ID: ";std::cin>>aid;std::cout<<"Student user ID: ";std::cin>>sid;std::cout<<"Revised deadline (YYYY-MM-DD HH:MM:SS; enter as one token with underscore instead of space): ";std::cin>>deadline;std::replace(deadline.begin(),deadline.end(),'_',' ');std::cout<<"Reason (one token): ";std::cin>>reason;grantExtension(*user,aid,sid,deadline,reason);std::cout<<"Extension saved.\n";} else if(c==3){int sid;double mark;std::string feedback;std::cout<<"Submission ID: ";std::cin>>sid;std::cout<<"Raw mark: ";std::cin>>mark;std::cout<<"Feedback (one token): ";std::cin>>feedback;markSubmission(*user,sid,mark,feedback);std::cout<<"Mark saved; waiting for HoD.\n";} else if(c==4){int course;double max;std::string title,deadline;std::cout<<"Course ID: ";std::cin>>course;std::cout<<"Title (one token): ";std::cin>>title;std::cout<<"Deadline YYYY-MM-DD_HH:MM:SS: ";std::cin>>deadline;std::replace(deadline.begin(),deadline.end(),'_',' ');std::cout<<"Maximum mark: ";std::cin>>max;std::cout<<"Created assignment ID "<<createAssignment(*user,course,title,deadline,max)<<"\n";}else if(c==5)showNotifications(*user);else std::cout<<"Invalid choice.\n";
                    } else if(user->role()==Role::HeadOfDepartment){
                        if(c==1)showPendingModeration(*user);else if(c==2){int sid;char yes;std::string comments;std::cout<<"Submission ID: ";std::cin>>sid;std::cout<<"Approve and release? (y/n): ";std::cin>>yes;std::cout<<"Comments (one token, or -): ";std::cin>>comments;moderateAndRelease(*user,sid,yes=='y'||yes=='Y',comments=="-"?"":comments);std::cout<<"Moderation decision saved.\n";}else if(c==3)showNotifications(*user);else std::cout<<"Invalid choice.\n";
                    } else {
                        if(c==1){std::string name,pass;int r;std::cout<<"New username: ";std::cin>>name;std::cout<<"New password (8+ chars): ";std::cin>>pass;std::cout<<"Role 1 Student 2 Lecturer 3 HoD 4 Admin: ";std::cin>>r;if(r<1||r>4)throw std::invalid_argument("Invalid role number");Role role= r==1?Role::Student:r==2?Role::Lecturer:r==3?Role::HeadOfDepartment:Role::Administrator;std::cout<<"Created user ID "<<createUser(*user,name,pass,role)<<"\n";}else if(c==2)showNotifications(*user);else std::cout<<"Invalid choice.\n";
                    }
                } catch(const std::exception& e){std::cout<<"Action failed: "<<e.what()<<"\n";}
            }
        } catch(const std::exception& e){std::cout<<"Login failed: "<<e.what()<<"\n";}
    }
}
