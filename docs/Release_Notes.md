# MLMS – Release Notes

**Product:** Moderated Learning Management System (MLMS)  
**Version:** 1.0  
**Release:** Initial Release

## 1. Overview

MLMS is a C++-based Learning Management System designed to support the management of courses, assignments, submissions, marking and moderation within an educational institution.

The system provides separate functions for Students, Lecturers, Heads of Department (HoDs) and System Administrators. It uses an SQLite database to store and manage system information and supports local operation when an internet connection is unavailable.

## 2. Main Features

### Student
- Log in using an assigned account.
- View assignments for enrolled courses.
- Submit and resubmit assignments.
- View submission information.
- View marks and feedback after they have been approved and released by the HoD.
- Receive system notifications.

### Lecturer
- Create assignments and set deadlines.
- View student submissions.
- Mark submitted assignments and provide written feedback.
- Grant individual students deadline extensions.
- Receive system notifications.

### Head of Department (HoD)
- Review marks submitted by lecturers.
- Approve or reject marks.
- Release approved marks for students to view.
- Receive system notifications.

### System Administrator
- Create and manage user accounts.
- Manage departments and courses.
- Assign lecturers and Heads of Department.
- Enrol students in courses.
- Activate and deactivate user accounts.
- View system records.
- Receive system notifications.

## 3. Assessment and Moderation Rules

- Students can only view marks after the HoD has approved and released them.
- Late submissions receive a penalty of 10% of the available marks per late day.
- The late penalty applies for up to three days.
- Submissions more than three days late receive zero marks.
- Approved deadline extensions are taken into account when determining whether a submission is late.

## 4. Technical Information

- **Programming language:** C++17
- **Application type:** Terminal-based application
- **Database:** SQLite
- **Build system:** CMake
- **Operating environment:** Windows with the MSYS2 UCRT64 development environment
- **Connectivity:** Designed to support local offline operation

## 5. Known Limitations

- The application uses a terminal-based interface rather than a graphical web interface.
- The database is stored locally on the computer running the application.
- The system is designed for local use and does not provide remote web-based access.
- Database backups must be managed by the system administrator.

## 6. Release Scope

Version 1.0 contains the initial MLMS functionality for account management, course management, assignment creation, assignment submission, marking, moderation and mark release.

Changes introduced after this release are not included in Version 1.0.

## 7. Release Identification

**Release version:** v1.0  
**Release type:** Initial release  
**Project:** Moderated Learning Management System (MLMS)