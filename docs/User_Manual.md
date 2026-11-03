# MLMS User Manual

## 1. Introduction

The Moderated Learning Management System (MLMS) is a C++ terminal-based application used to manage courses, assignments, submissions, marking and moderation.

The system has four user roles:

- Student
- Lecturer
- Head of Department (HoD)
- System Administrator

Each role has different permissions and menu options.

## 2. Starting the Application

1. Open the MSYS2 UCRT64 terminal.
2. Navigate to the MLMS project folder.
3. Start the application by running:

```bash
./build/lms.exe
```

4. The MLMS welcome screen will appear.
5. Select option `1` to log in.
6. Select the correct role.
7. Enter your username and password.

To close the application, select option `0` when the login screen is displayed.

## 3. Student User Manual

### 3.1 Logging in

1. Start MLMS.
2. Select `1. Log in`.
3. Select `1. Student`.
4. Enter your username and password.

The Student menu provides the following functions.

### 3.2 View Assignments

1. Select option `1. Assignments`.
2. The system displays assignments for courses in which the student is actively enrolled.
3. Each assignment displays its ID, title, deadline and maximum mark.

Extended deadlines are displayed where an applicable extension has been granted.

### 3.3 Submit an Assignment

1. Select option `2. Submit`.
2. Enter the Assignment ID.
3. Choose the submission type:
   - `1` – Text
   - `2` – File
   - `3` – File and text
4. Enter the assignment text or provide the full file path, depending on the selected type.
5. The system saves the submission and displays its Submission ID.

Students should keep the Submission ID for reference.

### 3.4 Resubmit an Assignment

1. Select option `3. Resubmit`.
2. Enter the Assignment ID.
3. Choose the submission type.
4. Enter the new text or file path.
5. The system saves the new submission version.

### 3.5 View Released Marks

1. Select option `4. Released mark`.
2. The system displays marks that have been approved and released by the HoD.
3. The assignment title, final mark and feedback are displayed.

Marks that have not yet been released will not appear.

### 3.6 View Notifications

1. Select option `5. Notifications`.
2. The system displays recent notifications.
3. Notifications show whether they are read or unread.

## 4. Lecturer User Manual

### 4.1 Logging in

1. Start MLMS.
2. Select `1. Log in`.
3. Select `2. Lecturer`.
4. Enter your username and password.

### 4.2 View Assignments

1. Select option `1. Assignments`.
2. The system displays assignments belonging to courses assigned to the Lecturer.
3. Review the assignment ID, title, deadline and maximum mark.

### 4.3 Grant an Individual Deadline Extension

1. Select option `2. Grant extension`.
2. Enter the Assignment ID.
3. Enter the Student user ID.
4. Enter the revised deadline in the format `YYYY-MM-DD_HH:MM:SS`.
5. Enter the reason, using underscores instead of spaces.
6. The system saves the extension.

### 4.4 Mark a Submission

1. Select option `3. Mark submission`.
2. Enter the Submission ID.
3. Enter the student's raw mark.
4. Enter feedback, using underscores instead of spaces.
5. The system calculates the final mark using the late-submission penalty rules.
6. The mark is saved and awaits HoD moderation.

The entered raw mark must be between zero and the assignment's maximum mark.

### 4.5 Create an Assignment

1. Select option `4. Create assignment`.
2. Enter the Course ID.
3. Enter the assignment title, using underscores instead of spaces.
4. Enter the deadline in the format `YYYY-MM-DD_HH:MM:SS`.
5. Enter the maximum mark.
6. The system creates the assignment and displays its Assignment ID.

### 4.6 View Notifications

Select option `5. Notifications` to view recent notifications.

### 4.7 Class-Wide Deadline Extension

The application code also contains a class-wide extension function. It accepts an Assignment ID, revised deadline and reason, and saves the extension for the class.

**Important:** This action is currently implemented as Lecturer option `6`, but option 6 is missing from the displayed Lecturer menu. The menu should be corrected before this function is used by ordinary users.

## 5. Head of Department (HoD) User Manual

### 5.1 Logging in

1. Start MLMS.
2. Select `1. Log in`.
3. Select `3. Head of Department (HoD)`.
4. Enter your username and password.

### 5.2 View Pending Moderation

1. Select option `1. Pending moderation`.
2. The system displays marks awaiting moderation for the HoD's assigned department.
3. Review the Submission ID, assignment title, student username and mark.

### 5.3 Approve or Reject a Mark

1. Select option `2. Approve/release`.
2. Enter the Submission ID.
3. Enter `y` to approve and release the mark, or `n` to reject it.
4. Enter a comment, or enter `-` if no comment is required.
5. The system saves the moderation decision.

Approved marks are released to students. Rejected marks are not released.

### 5.4 View Notifications

Select option `3. Notifications` to view recent notifications.

## 6. System Administrator User Manual

### 6.1 First-Time Setup

When the application is started for the first time and no Administrator account exists:

1. Enter an Administrator username without spaces.
2. Enter a password containing at least eight characters.
3. Wait for confirmation that the account has been created.
4. Return to the login screen.
5. Log in using the new Administrator credentials.

### 6.2 Create User Accounts

1. Select option `1. Create user account`.
2. Enter the new username.
3. Enter a temporary password containing at least eight characters.
4. Select the role:
   - `1` – Student
   - `2` – Lecturer
   - `3` – HoD
   - `4` – Administrator
5. The system creates the account and displays its User ID.

### 6.3 Create a Department

1. Select option `2. Create department`.
2. Enter the department name.
3. Use underscores for spaces.
4. The system creates the department and displays its Department ID.

### 6.4 Create a Course

1. Select option `3. Create course`.
2. Enter the Department ID.
3. Enter the Course code.
4. Enter the Course name, using underscores for spaces.
5. The system creates the course and displays its Course ID.

### 6.5 Assign a Lecturer to a Course

1. Select option `4. Assign lecturer to course`.
2. Enter the Course ID.
3. Enter the Lecturer user ID.
4. The system assigns the Lecturer to the course.

### 6.6 Assign an HoD to a Department

1. Select option `5. Assign HoD to department`.
2. Enter the Department ID.
3. Enter the HoD user ID.
4. The system saves the assignment.

### 6.7 Enrol a Student

1. Select option `6. Enrol student`.
2. Enter the Student user ID.
3. Enter the Course ID.
4. The system enrols the student in the course.

### 6.8 Activate or Deactivate an Account

1. Select option `7. Activate/deactivate account`.
2. Enter the User ID.
3. Enter `1` to activate or `0` to deactivate the account.
4. The system updates the account status.

### 6.9 View System Records

1. Select option `8. View user/department/course/enrolment`.
2. The system displays:
   - User IDs, usernames, roles and account statuses.
   - Department IDs and names.
   - Course IDs, codes, names and department IDs.
   - Enrolment IDs, student IDs, course IDs and enrolment statuses.

### 6.10 View Notifications

Select option `9. Notifications` to view recent notifications.

## 7. Late Submission Rules

MLMS applies the following late-submission penalty rules when a Lecturer marks a submission:

- Work submitted on time receives no late penalty.
- One day late results in a deduction of 10% of the available marks.
- Two days late results in a deduction of 20%.
- Three days late results in a deduction of 30%.
- More than three days late receives zero.

The system uses the effective deadline, taking applicable extensions into account.

## 8. Logging Out

To leave a role's menu:

1. Enter `0. Logout`.
2. The system returns to the login screen.
3. Another authorised user can then log in.

## 9. Important User Guidelines

- Use the correct role when logging in.
- Keep usernames and passwords confidential.
- Enter the correct IDs when performing operations.
- Use the required date and time format.
- Students should keep their Submission IDs.
- Lecturers should provide clear feedback when marking.
- HoDs should review marks before releasing them.
- Administrators should protect user accounts and school records.

## 10. Conclusion

The MLMS User Manual explains how Students, Lecturers, HoDs and System Administrators use the system. Following these instructions helps users complete their tasks correctly while ensuring that marks are reviewed and released through the moderation process.