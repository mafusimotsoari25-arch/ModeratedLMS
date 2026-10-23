# Moderated LMS — runnable C++17 starter

This is a runnable, layered console starter aligned with the case study. It includes:
- C++17 and CMake
- SQLite persistence with prepared statements for data values
- Password hashing with PBKDF2-HMAC-SHA256 using OpenSSL (salted, encoded hash; never plaintext)
- Abstract `User` class, role subclasses and a `UserFactory`
- `ILatePenaltyStrategy` / `StandardLatePenalty` Strategy pattern
- `AuditRepository` (Repository/DAO pattern)
- `NotificationService` using the Observer pattern
- A service layer for a demonstration submission, mark, moderation and release flow
- Unit tests for late penalties, password verification and role factory
- SQLite schema for users, departments, courses, enrolments, assignments, extensions, submissions, versions, marks, audit log, and notifications

## Windows 11 setup (MSYS2 UCRT64)
Install/update packages from the **MSYS2 UCRT64** terminal:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
```

If MSYS2 asks you to close the terminal during update, close it, reopen UCRT64, and repeat `pacman -Syu` if needed. Add `C:\msys64\ucrt64\bin` to Windows PATH, then open a new terminal.

## Build and run
Run these commands in **MSYS2 UCRT64**, from this project folder:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/lms.exe
```

The executable creates `lms.db` in its current working directory. The demo adds a demo account/course/assignment/submission when it is run for the first time, demonstrates the submission -> marking -> HoD moderation -> release path, records a mark audit entry, and sends Observer notifications. It avoids duplicating the demo submission on later runs.

## Design mapping
- Layered architecture: console presentation (`main.cpp`) -> application service (`LmsService`) -> repositories/database -> SQLite.
- Strategy: `ILatePenaltyStrategy` and `StandardLatePenalty`.
- Repository/DAO: `AuditRepository`; database setup in `Database`.
- Observer: `NotificationService` and `INotificationObserver`.
- Factory: `UserFactory` creates the correct role-specific `User` subclass.
- Inheritance/polymorphism: abstract `User` and concrete Student/Lecturer/HoD/Administrator.
- Audit: every mark update performed by the service is recorded with actor, time, old mark, new mark and reason.

## Scope and limitations
This is a working foundation and demo workflow, not a claim that every assessed requirement is finished. The console demo does not yet provide complete interactive account/course administration, secure login sessions, individual extension management UI, or comprehensive role-permission enforcement for every possible operation. Those must be completed and validated against the final SRS before submission. Do not use demo credentials or demo records for real student data.

## Password security
PBKDF2-HMAC-SHA256 is a password-based key derivation function provided by OpenSSL. This starter uses a random salt, a high iteration count, and constant-time comparison during verification. For a production deployment, review the iteration count against the school's hardware and follow current security guidance.
