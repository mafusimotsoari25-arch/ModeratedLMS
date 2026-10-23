#include "LatePenalty.h"
#include "PasswordService.h"
#include "UserFactory.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    StandardLatePenalty p;
    check(std::abs(p.finalMark(80,100,0)-80.0)<0.001, "On-time mark");
    check(std::abs(p.finalMark(80,100,1)-70.0)<0.001, "One day late");
    check(std::abs(p.finalMark(80,100,2)-60.0)<0.001, "Two days late");
    check(std::abs(p.finalMark(80,100,3)-50.0)<0.001, "Three days late");
    check(std::abs(p.finalMark(80,100,4)-0.0)<0.001, "Over three days late");

    auto user = UserFactory::create(7, "test_student", "Student");
    check(user->role() == Role::Student, "UserFactory creates student");
    check(user->roleName() == "Student", "Polymorphic role name");

    const auto hash = PasswordService::hashPassword("CorrectHorseBattery!");
    check(PasswordService::verifyPassword(hash, "CorrectHorseBattery!"), "Correct password verifies");
    check(!PasswordService::verifyPassword(hash, "wrong-password"), "Wrong password rejected");

    std::cout << "All LMS starter tests passed.\n";
    return 0;
}
