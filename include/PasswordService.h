#pragma once
#include <string>

class PasswordService {
public:
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& encodedHash,
                               const std::string& password);
};
