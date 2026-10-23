#include "Database.h"
#include <stdexcept>

Database::Database(const std::string& path) {
    const int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string message = db_ ? sqlite3_errmsg(db_) : "Could not allocate SQLite handle";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot open database: " + message);
    }
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");
}

Database::~Database() { if (db_) sqlite3_close(db_); }

void Database::execute(const std::string& sql) {
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = error ? error : sqlite3_errmsg(db_);
        sqlite3_free(error);
        throw std::runtime_error("SQLite error: " + message);
    }
}

void Database::initialiseSchema() {
    execute(R"SQL(
    CREATE TABLE IF NOT EXISTS users (
      user_id INTEGER PRIMARY KEY AUTOINCREMENT,
      username TEXT NOT NULL UNIQUE,
      password_hash TEXT NOT NULL,
      role TEXT NOT NULL CHECK(role IN ('Student','Lecturer','HeadOfDepartment','Administrator')),
      active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
    );
    CREATE TABLE IF NOT EXISTS departments (
      department_id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT NOT NULL UNIQUE
    );
    CREATE TABLE IF NOT EXISTS courses (
      course_id INTEGER PRIMARY KEY AUTOINCREMENT,
      department_id INTEGER NOT NULL,
      code TEXT NOT NULL UNIQUE,
      name TEXT NOT NULL,
      FOREIGN KEY(department_id) REFERENCES departments(department_id)
    );
    CREATE TABLE IF NOT EXISTS course_lecturers (
      course_id INTEGER NOT NULL,
      lecturer_user_id INTEGER NOT NULL,
      PRIMARY KEY(course_id, lecturer_user_id),
      FOREIGN KEY(course_id) REFERENCES courses(course_id),
      FOREIGN KEY(lecturer_user_id) REFERENCES users(user_id)
    );
    CREATE TABLE IF NOT EXISTS enrolments (
      enrolment_id INTEGER PRIMARY KEY AUTOINCREMENT,
      course_id INTEGER NOT NULL,
      student_user_id INTEGER NOT NULL,
      enrolment_date TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
      status TEXT NOT NULL DEFAULT 'Active' CHECK(status IN ('Active','Withdrawn','Completed')),
      UNIQUE(course_id, student_user_id),
      FOREIGN KEY(course_id) REFERENCES courses(course_id),
      FOREIGN KEY(student_user_id) REFERENCES users(user_id)
    );
    CREATE TABLE IF NOT EXISTS assignments (
      assignment_id INTEGER PRIMARY KEY AUTOINCREMENT,
      course_id INTEGER NOT NULL,
      title TEXT NOT NULL,
      deadline TEXT NOT NULL,
      max_mark REAL NOT NULL CHECK(max_mark > 0),
      FOREIGN KEY(course_id) REFERENCES courses(course_id)
    );
    CREATE TABLE IF NOT EXISTS extensions (
      extension_id INTEGER PRIMARY KEY AUTOINCREMENT,
      assignment_id INTEGER NOT NULL,
      student_user_id INTEGER NOT NULL,
      revised_deadline TEXT NOT NULL,
      granted_by_user_id INTEGER NOT NULL,
      reason TEXT NOT NULL,
      UNIQUE(assignment_id, student_user_id),
      FOREIGN KEY(assignment_id) REFERENCES assignments(assignment_id),
      FOREIGN KEY(student_user_id) REFERENCES users(user_id),
      FOREIGN KEY(granted_by_user_id) REFERENCES users(user_id)
    );
    CREATE TABLE IF NOT EXISTS submissions (
      submission_id INTEGER PRIMARY KEY AUTOINCREMENT,
      assignment_id INTEGER NOT NULL,
      student_user_id INTEGER NOT NULL,
      submitted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
      status TEXT NOT NULL DEFAULT 'Submitted'
        CHECK(status IN ('Submitted','Late','Marked','Moderated','Released')),
      current_version INTEGER NOT NULL DEFAULT 1,
      UNIQUE(assignment_id, student_user_id),
      FOREIGN KEY(assignment_id) REFERENCES assignments(assignment_id),
      FOREIGN KEY(student_user_id) REFERENCES users(user_id)
    );
    CREATE TABLE IF NOT EXISTS submission_versions (
      version_id INTEGER PRIMARY KEY AUTOINCREMENT,
      submission_id INTEGER NOT NULL,
      version_number INTEGER NOT NULL,
      content TEXT NOT NULL,
      submitted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
      is_late INTEGER NOT NULL CHECK(is_late IN (0,1)),
      UNIQUE(submission_id, version_number),
      FOREIGN KEY(submission_id) REFERENCES submissions(submission_id)
    );
    CREATE TABLE IF NOT EXISTS marks (
      mark_id INTEGER PRIMARY KEY AUTOINCREMENT,
      submission_id INTEGER NOT NULL UNIQUE,
      raw_mark REAL NOT NULL,
      final_mark REAL NOT NULL,
      feedback TEXT NOT NULL,
      marked_by_user_id INTEGER NOT NULL,
      moderated_by_user_id INTEGER,
      released INTEGER NOT NULL DEFAULT 0 CHECK(released IN (0,1)),
      FOREIGN KEY(submission_id) REFERENCES submissions(submission_id),
      FOREIGN KEY(marked_by_user_id) REFERENCES users(user_id),
      FOREIGN KEY(moderated_by_user_id) REFERENCES users(user_id)
    );
    CREATE TABLE IF NOT EXISTS mark_change_audit (
      audit_id INTEGER PRIMARY KEY AUTOINCREMENT,
      submission_id INTEGER NOT NULL,
      changed_by_user_id INTEGER NOT NULL,
      changed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
      old_mark REAL,
      new_mark REAL NOT NULL,
      reason TEXT NOT NULL,
      FOREIGN KEY(submission_id) REFERENCES submissions(submission_id),
      FOREIGN KEY(changed_by_user_id) REFERENCES users(user_id)
    );
    CREATE TABLE IF NOT EXISTS notifications (
      notification_id INTEGER PRIMARY KEY AUTOINCREMENT,
      user_id INTEGER NOT NULL,
      message TEXT NOT NULL,
      created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
      is_read INTEGER NOT NULL DEFAULT 0 CHECK(is_read IN (0,1)),
      FOREIGN KEY(user_id) REFERENCES users(user_id)
    );
    )SQL");
}
