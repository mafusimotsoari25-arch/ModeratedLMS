#include "UserFactory.h"
#include <stdexcept>

std::unique_ptr<User> UserFactory::create(int id, const std::string& username,
                                          const std::string& roleName) {
    if (roleName == "Student") return std::make_unique<Student>(id, username);
    if (roleName == "Lecturer") return std::make_unique<Lecturer>(id, username);
    if (roleName == "HeadOfDepartment") return std::make_unique<HeadOfDepartment>(id, username);
    if (roleName == "Administrator") return std::make_unique<Administrator>(id, username);
    throw std::invalid_argument("Unknown user role: " + roleName);
}
