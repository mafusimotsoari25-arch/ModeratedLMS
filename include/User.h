#pragma once
#include <string>
#include <utility>

enum class Role { Student, Lecturer, HeadOfDepartment, Administrator };

class User {
public:
    User(int id, std::string username) : id_(id), username_(std::move(username)) {}
    virtual ~User() = default;
    int id() const { return id_; }
    const std::string& username() const { return username_; }
    virtual Role role() const = 0;
    virtual std::string roleName() const = 0;
private:
    int id_;
    std::string username_;
};

class Student final : public User {
public:
    using User::User;
    Role role() const override { return Role::Student; }
    std::string roleName() const override { return "Student"; }
};
class Lecturer final : public User {
public:
    using User::User;
    Role role() const override { return Role::Lecturer; }
    std::string roleName() const override { return "Lecturer"; }
};
class HeadOfDepartment final : public User {
public:
    using User::User;
    Role role() const override { return Role::HeadOfDepartment; }
    std::string roleName() const override { return "Head of Department"; }
};
class Administrator final : public User {
public:
    using User::User;
    Role role() const override { return Role::Administrator; }
    std::string roleName() const override { return "Administrator"; }
};
