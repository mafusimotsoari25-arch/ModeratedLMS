#include "Database.h"
#include "LatePenalty.h"
#include "LmsService.h"
#include "PasswordService.h"
#include "UserFactory.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int scalarInt(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    const int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); throw std::runtime_error("Scalar query returned no row"); }
    const int value = sqlite3_column_int(stmt, 0); sqlite3_finalize(stmt); return value;
}
template<class F> void mustThrow(F f, const char* message) { bool threw=false; try { f(); } catch (const std::exception&) { threw=true; } check(threw,message); }

int main() {
    try {
        StandardLatePenalty p;
        check(std::abs(p.finalMark(80,100,0)-80.0)<0.001,"On-time mark");
        check(std::abs(p.finalMark(80,100,1)-70.0)<0.001,"One day late");
        check(std::abs(p.finalMark(80,100,2)-60.0)<0.001,"Two days late");
        check(std::abs(p.finalMark(80,100,3)-50.0)<0.001,"Three days late");
        check(std::abs(p.finalMark(80,100,4)-0.0)<0.001,"Over three days late");
        mustThrow([&]{p.finalMark(101,100,0);},"Marks above maximum rejected");
        auto factoryUser=UserFactory::create(7,"test_student","Student");
        check(factoryUser->role()==Role::Student,"Factory creates student");
        check(factoryUser->roleName()=="Student","Polymorphic role name");
        const auto hash=PasswordService::hashPassword("CorrectHorseBattery!");
        check(PasswordService::verifyPassword(hash,"CorrectHorseBattery!"),"Correct password verifies");
        check(!PasswordService::verifyPassword(hash,"wrong-password"),"Wrong password rejected");

        Database db(":memory:"); db.initialiseSchema(); LmsService service(db); service.seedDemoData();
        auto student=service.login("demo_student","Student123!");
        auto lecturer=service.login("demo_lecturer","Lecturer123!");
        auto hod=service.login("demo_hod","Hod123456!");
        auto admin=service.login("demo_admin","Admin123456!");
        int registeredId=service.registerStudent("new_student","SecurePass123!");
        auto registered=service.login("new_student","SecurePass123!");
        check(registered->id()==registeredId && registered->role()==Role::Student,"New student can register and log in");
        mustThrow([&]{service.registerStudent("new_student","AnotherPass123!");},"Duplicate username rejected");
        mustThrow([&]{service.login("new_student","wrong-password");},"Wrong password rejected for registered user");
        check(student->role()==Role::Student && lecturer->role()==Role::Lecturer && hod->role()==Role::HeadOfDepartment && admin->role()==Role::Administrator,"All four roles log in");
        mustThrow([&]{service.createUser(*student,"illegal_user","StrongPass123!",Role::Student);},"Student cannot create users");

        // Make a test assignment with a deadline in the future for deterministic submission checks.
        db.execute("UPDATE assignments SET deadline='2099-12-31 23:59:59' WHERE assignment_id=1;");
        int submission=service.submitAssignment(*student,1,"first version","2099-12-01 10:00:00");
        service.resubmitAssignment(*student,1,"second version","2099-12-02 10:00:00");
        service.markSubmission(*lecturer,submission,80,"Good work");
        check(scalarInt(db.handle(), "SELECT COUNT(*) FROM mark_change_audit WHERE submission_id=1") == 1, "Initial marking is audited");
        service.changeMark(*lecturer,submission,75,"Correction after review");
        check(scalarInt(db.handle(), "SELECT COUNT(*) FROM mark_change_audit WHERE submission_id=1") == 2, "Every mark change is audited");
        check(scalarInt(db.handle(), "SELECT COUNT(*) FROM marks WHERE submission_id=1 AND released=0") == 1, "Mark remains hidden before release");
        service.moderateAndRelease(*hod,submission,true,"Approved");
        check(scalarInt(db.handle(), "SELECT COUNT(*) FROM marks WHERE submission_id=1 AND released=1") == 1, "HoD approval releases mark");
        service.showStudentMarks(*student);
        mustThrow([&]{service.moderateAndRelease(*lecturer,submission,true);},"Lecturer cannot moderate marks");
        int newAssignment=service.createAssignment(*lecturer,1,"Extension test","2099-12-31 23:59:59",100);
        service.grantExtension(*lecturer,newAssignment,student->id(),"2100-01-10 23:59:59","Medical_reason");
        int extensionSubmission=service.submitAssignment(*student,newAssignment,"late but extended","2100-01-05 10:00:00");
        check(extensionSubmission>0,"Granted extension allows submission");
        check(scalarInt(db.handle(), "SELECT COUNT(*) FROM extensions WHERE assignment_id=2 AND student_user_id=1") == 1, "Extension is persisted");
        mustThrow([&]{service.submitAssignment(*student,newAssignment,"duplicate","2100-01-05 10:00:00");},"Duplicate initial submission rejected");
        mustThrow([&]{service.resubmitAssignment(*student,newAssignment,"too late","2100-01-11 10:00:00");},"Resubmission after deadline rejected");
        std::cout << "All LMS tests passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "TEST FAILED: " << e.what() << '\n'; return 1; }
}
