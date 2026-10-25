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
        check(student->role()==Role::Student && lecturer->role()==Role::Lecturer && hod->role()==Role::HeadOfDepartment && admin->role()==Role::Administrator,"All four roles log in");
        mustThrow([&]{service.createUser(*student,"illegal_user","StrongPass123!",Role::Student);},"Student cannot create users");

        // Make a test assignment with a deadline in the future for deterministic submission checks.
        db.execute("UPDATE assignments SET deadline='2099-12-31 23:59:59' WHERE assignment_id=1;");
        int submission=service.submitAssignment(*student,1,"first version","2099-12-01 10:00:00");
        service.resubmitAssignment(*student,1,"second version","2099-12-02 10:00:00");
        service.markSubmission(*lecturer,submission,80,"Good work");
        check(sqlite3_exec(db.handle(), "SELECT 1 FROM marks WHERE submission_id=1 AND released=0", nullptr, nullptr, nullptr)==SQLITE_OK, "Unreleased mark query succeeds");
        service.moderateAndRelease(*hod,submission,true,"Approved");
        service.showStudentMarks(*student);
        mustThrow([&]{service.moderateAndRelease(*lecturer,submission,true);},"Lecturer cannot moderate marks");
        int newAssignment=service.createAssignment(*lecturer,1,"Extension test","2099-12-31 23:59:59",100);
        service.grantExtension(*lecturer,newAssignment,student->id(),"2100-01-10 23:59:59","Medical_reason");
        int extensionSubmission=service.submitAssignment(*student,newAssignment,"late but extended","2100-01-05 10:00:00");
        check(extensionSubmission>0,"Granted extension allows submission");
        mustThrow([&]{service.submitAssignment(*student,newAssignment,"duplicate","2100-01-05 10:00:00");},"Duplicate initial submission rejected");
        mustThrow([&]{service.resubmitAssignment(*student,newAssignment,"too late","2100-01-11 10:00:00");},"Resubmission after deadline rejected");
        std::cout << "All LMS tests passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "TEST FAILED: " << e.what() << '\n'; return 1; }
}
