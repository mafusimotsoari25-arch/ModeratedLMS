# Phase 8 — Maintenance: CR1 (Release v1.1)

## 1. Change request and classification

**CR1:** A lecturer can extend the deadline for a whole class, and the extension appears in each student's assignment list.

**Classification: Perfective maintenance (with a small adaptive element).** The existing LMS already supports individual extensions. This request improves an existing capability to support a class-wide workflow and improves deadline visibility. It does not primarily repair a defect or respond to a changed operating environment.

## 2. Impact analysis

| Artefact | Impact and required update |
|---|---|
| Requirement | Add FR-EXT-02: an assigned lecturer may set/replace one revised deadline for an assignment's active class; the system notifies enrolled students and displays the effective deadline in the student assignment list. |
| Use case/activity diagram | Add “Extend deadline for whole class”; lecturer selects assignment, enters revised deadline/reason, system validates course assignment, stores extension, and notifies active enrolled students. |
| Sequence diagram | Add lecturer → LmsService → Database class-extension save, followed by NotificationService messages to enrolled students. |
| Class design | Add `LmsService::grantClassExtension(...)`; existing `effectiveDeadline(...)` resolves individual extension first, then class extension, then original assignment deadline. |
| Database | Add `class_extension` table with one class-wide record per assignment, revised deadline, granting lecturer, reason, timestamp, and foreign keys. Existing `extension` remains for student-specific exceptions. |
| Student assignment list | Display the effective deadline, not always the original assignment deadline. Individual extension takes precedence over class-wide extension. |
| Lecturer interface | Add a menu action to extend an assignment deadline for the whole class. |
| Unit/integration tests | Test class-extension persistence, authorization, class notifications, effective-deadline precedence, assignment-list display, and unchanged individual extension/submission/late-penalty/moderation workflows. |
| Build/release | Update CMake target source only if new source files are introduced; document v1.1 and run the full existing suite. |

## 3. Implementation summary — v1.1

- Added `class_extension` to the SQLite schema. It is separate from `extension` so existing per-student extensions remain supported.
- Added `LmsService::grantClassExtension`. Only a lecturer assigned to the assignment's course can use it.
- The service creates or updates the class extension and sends a notification to every actively enrolled student in the assignment's course.
- Effective deadline resolution is: **individual extension → class extension → original assignment deadline**. This allows a lecturer to grant an exception to one student without losing the class-wide deadline.
- The lecturer menu now offers “Extend deadline for whole class”.
- Student assignment-list queries now show the effective deadline.
- Updated the database schema test for the additional table.

## 4. Regression test plan and results

Run from the project root in the configured CMake environment:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Required CR1 checks:

| Test ID | Test | Expected result |
|---|---|---|
| CR1-T01 | Assigned lecturer extends class deadline | Extension saved successfully |
| CR1-T02 | Lecturer not assigned to course attempts extension | Request rejected |
| CR1-T03 | Active enrolled students receive notifications | Each enrolled student receives a notification |
| CR1-T04 | Student assignment list after class extension | Revised class deadline is displayed |
| CR1-T05 | Student has an individual extension as well as class extension | Individual deadline takes precedence |
| CR1-T06 | No extension exists | Original assignment deadline remains visible |
| REG-T01 | Existing individual extension flow | Still works |
| REG-T02 | Submission/resubmission and late penalty | Existing rules remain unchanged |
| REG-T03 | Marking, HoD moderation and mark release | Existing behaviour remains unchanged |
| REG-T04 | Password, user roles, database, audit and notification tests | Existing behaviour remains unchanged |

**Evidence status:** The source and test configuration have been updated, but the test command must be run in the project's supported C++17/CMake environment before reporting a pass. Do not describe tests as passed until the command output confirms it.
