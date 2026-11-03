# MLMS Installation Guide

## 1. Introduction

The Moderated Learning Management System (MLMS) is an offline C++17 console application designed to support teaching, assignment submission, marking and moderation in a school.

The system provides four user roles:
- Student
- Lecturer
- Head of Department (HoD)
- System Administrator

MLMS uses SQLite to store information and OpenSSL to support password security. The application runs locally and does not require an internet connection for its core functions.

## 2. System Requirements

The installation computer requires:

- Windows operating system.
- MSYS2 with the UCRT64 environment.
- C++17-compatible GCC compiler.
- CMake version 3.20 or later.
- Ninja build system.
- SQLite development library.
- OpenSSL development library.
- GoogleTest development library, required by the project's CMake configuration.

## 3. Installing Required Software

1. Install MSYS2 on the Windows computer.
2. Open the **MSYS2 UCRT64** terminal.
3. Install the required development packages:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-gtest
```

## 4. Installing MLMS

### Step 1: Obtain the project

Obtain the MLMS project folder from the authorised source and extract it into a suitable location, for example:

`C:\MLMS`

The folder must contain the `CMakeLists.txt`, `src`, `include` and `tests` directories.

### Step 2: Open the project directory

Open the MSYS2 UCRT64 terminal and navigate to the extracted project:

```bash
cd /c/MLMS
```

Make sure the terminal is inside the directory containing `CMakeLists.txt`.

### Step 3: Configure the project

Run:

```bash
cmake -S . -B build -G Ninja
```

This configures the project and prepares the build system.

### Step 4: Build the application

Run:

```bash
cmake --build build
```

Wait for the build process to finish successfully.

### Step 5: Start MLMS

Run:

```bash
./build/lms.exe
```

This starts the MLMS terminal application.

## 5. First-Time Database and Administrator Setup

When MLMS starts, it automatically opens or creates the local SQLite database named `lms.db` and initialises the database tables.

If no Administrator account exists, the application displays a first-time setup message.

The technician or authorised administrator must:

1. Enter an Administrator username without spaces.
2. Enter a password containing at least eight characters.
3. Wait for confirmation that the Administrator account has been created.
4. Log in using the newly created credentials.

The password is stored using the application's password-hashing service. The system does not require a public student registration process; the Administrator creates user accounts.

## 6. Initial School Configuration

After logging in as Administrator:

1. Create the school's departments.
2. Create the courses offered by the departments.
3. Register Lecturer accounts.
4. Register HoD accounts and assign them to departments.
5. Register Student accounts.
6. Assign Lecturers to courses.
7. Enrol Students in their respective courses.

Users can then log in using their assigned accounts and roles.

## 7. Installation Verification

The technician should confirm that:

- MLMS starts without an application error.
- The first-time Administrator setup works when the database has no Administrator.
- Users can log in using valid credentials.
- Each role receives the appropriate access.
- The application can create and retrieve database records.
- The application can run without an internet connection.

## 8. Database and File Backup

The application stores its database in `lms.db`.

File submissions are stored in the `submissions` directory. Both the database and the submissions directory must be kept together when the installation is moved or backed up.

The technician should:

- Make regular backups of `lms.db`.
- Back up the `submissions` directory.
- Store backups in a secure location.
- Restrict access to the database and submission files.
- Close the application before copying the database for backup.
- Verify that backups can be restored when needed.

**Important:** Do not delete `lms.db` when updating the application unless a fresh database is intentionally required. Back up the database and submissions folder before changing application versions.

## 9. Troubleshooting

| Problem | Recommended action |
|---|---|
| CMake is not recognised | Open the MSYS2 UCRT64 terminal and check the CMake installation. |
| Compiler not found | Check that the UCRT64 GCC package is installed. |
| SQLite or OpenSSL dependency error | Check that the required development packages are installed. |
| CMake cannot find GoogleTest | Install the UCRT64 GoogleTest package. |
| Application does not start | Confirm that the build completed and that `build/lms.exe` exists. |
| Database cannot be opened | Check the application directory and file permissions. |
| Submission files are missing | Confirm that the `submissions` directory is present. |

## 10. Conclusion

After successful installation and verification, MLMS is ready for use on the configured school computer. The System Administrator can manage accounts and school information, while Lecturers, HoDs and Students can use the functions assigned to their roles.

Regular backups and appropriate access controls must be maintained to protect school information.