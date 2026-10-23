#pragma once
#include "User.h"
#include <memory>
#include <string>

class UserFactory {
public:
    static std::unique_ptr<User> create(int id, const std::string& username,
                                        const std::string& roleName);
};
