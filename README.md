# Moderated Learning Management System (MLMS) — Phase 4

Offline C++17 console application using SQLite and OpenSSL. User accounts are created by the Administrator; there is no public student sign-up. The login screen asks the user to choose Student, Lecturer, HoD, or Administrator and checks the selected role against the database.

## Included workflow
- First-run setup for the initial Administrator.
- Administrator creates accounts, departments, courses, lecturer assignments, HoD assignments, and student enrolments.
- Passwords are hashed using OpenSSL-backed password service; never store plain-text passwords.
- Assignment creation, deadlines, extension records, resubmission versions, marking, moderation, mark release, notifications, and mark audit records.
- File submissions are copied into `submissions/assignment_<id>/student_<id>/` and the managed copy path is stored in SQLite. Keep the `submissions/` folder with the database when backing up or moving the project.
- Local SQLite database is `lms.db`. The application does not require internet or email for its core workflow.

## Requirements on Windows
Install MSYS2 and use the **MSYS2 UCRT64** terminal. Install packages:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
```

## Build and run (MSYS2 UCRT64)
Run these commands from this folder (the folder containing `CMakeLists.txt`):

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/lms.exe
```

If you previously built an older version, remove only the generated build directory before rebuilding:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
./build/lms.exe
```

Do not remove `lms.db` unless you have backed it up and intentionally want to start with a new database. Existing databases with an older schema may not be compatible; back up the database and `submissions/` folder before changing versions.

## Git: save the project locally
Open UCRT64 in this folder, then run:

```bash
git init
git add .
git commit -m "Implement offline moderated LMS Phase 4"
```

If Git asks for your identity, set it (use your own details):

```bash
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```
Then repeat the commit command.

## GitHub: upload the project
1. Sign in to GitHub in your browser and create a **new empty repository** (do not add a README, licence, or .gitignore if you already have them here).
2. Copy the repository's HTTPS URL.
3. In UCRT64, run the following, replacing the example URL with your own:

```bash
git branch -M main
git remote add origin https://github.com/YOUR-USERNAME/YOUR-REPOSITORY.git
git push -u origin main
```

For later changes:

```bash
git add .
git commit -m "Describe your change"
git push
```

Do not upload real student information, passwords, or a database containing private school data to a public GitHub repository. The `.gitignore` excludes local build output and database files.

## Important scope note
This is a local console application, not a web application. On a single computer, the local database and managed submission files are available to users of that installation. Separate computers would need a properly designed school local-network server/shared-storage setup. Build success alone does not prove every workflow has been fully tested.
