# Moderated Learning Management System (MLMS)

A C++17 console application using a layered design, SQLite persistence, and OpenSSL password hashing. This implementation includes role-checked actions for Students, Lecturers, Heads of Department (HoDs), and Administrators.

## Features implemented

- **Authentication:** username/password login; passwords are stored as salted PBKDF2-HMAC-SHA256 hashes, not plaintext.
- **Role-based access:** service methods check both the user's database role and the role object. Students submit/resubmit and view released marks; assigned Lecturers create assignments, grant extensions, and mark submissions; HoDs approve/reject marks for release; Administrators create user accounts.
- **Late penalty:** the system calculates late days from the latest submission version's timestamp and the applicable deadline. It deducts 10% of the maximum mark per late day for days 1–3, and gives zero after 3 days.
- **Extensions:** an assigned Lecturer can grant or update a student's revised deadline and must provide a reason.
- **Resubmissions:** a Student can add a new version before the applicable deadline. Previous versions remain in `submission_versions`; resubmission is blocked after marking/moderation/release.
- **Moderation and release:** a mark is not visible to the Student until a HoD approves and releases it.
- **Audit and notifications:** mark changes record actor, timestamp, previous/new final marks, and reason. Notifications are stored in SQLite.
- **Persistence:** SQLite schema and prepared statements; data remains in `lms.db` between runs.

## Requirements

Windows 11 with MSYS2 UCRT64, GCC, CMake, Ninja, SQLite and OpenSSL development packages.

Open **MSYS2 UCRT64** and install packages if needed:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
```

## Build, test, and run

Open MSYS2 UCRT64 in this project folder (the folder containing `CMakeLists.txt`) and run:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/lms.exe
```

The automated test suite checks password verification, the four roles, role access restrictions, late-penalty boundaries, extension use, resubmission rules, and mark release workflow.

## Demo accounts

On first run, the application creates demo accounts. For demonstration only:

| Role | Username | Password |
|---|---|---|
| Student | `demo_student` | `Student123!` |
| Lecturer | `demo_lecturer` | `Lecturer123!` |
| HoD | `demo_hod` | `Hod123456!` |
| Administrator | `demo_admin` | `Admin123456!` |

The demo setup refreshes these four demo passwords each time the application starts. Do not use demo accounts for real student records. Replace/remove demo accounts before real deployment.

## Using the console

1. Start the program and choose `1 Login`.
2. Sign in with one of the demo accounts above.
3. Use the menu shown for that role. Assignment deadlines use `YYYY-MM-DD_HH:MM:SS` in the console (underscore in place of the space). Feedback/reasons are currently entered as one token.
4. The Lecturer must be assigned to the course to create assignments, grant extensions, or mark submissions. The sample course (ID `1`) is assigned to the demo Lecturer and enrolled by the demo Student.
5. Students can only see marks after a HoD releases them.

## Design mapping

- Layered architecture: console presentation → `LmsService` → repository/database access → SQLite.
- Inheritance/polymorphism and Factory: `User`, four role subclasses, and `UserFactory`.
- Strategy: `ILatePenaltyStrategy` / `StandardLatePenalty`.
- Repository/DAO: `AuditRepository`.
- Observer: `NotificationService` / `INotificationObserver`.
- Persistence: SQLite; input values use prepared statements.

## Limitations to disclose

This is a coursework console implementation, not a production deployment. It has no file-upload UI, no password-reset flow, limited input formatting (single-token text fields), and no department-specific HoD assignment rules. Review the final SRS, test on the target Windows/MSYS2 environment, and remove demo data before submission.
