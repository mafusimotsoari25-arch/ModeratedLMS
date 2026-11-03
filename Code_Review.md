Self-Code Review – Moderated Learning Management System

1. Purpose

This review evaluates the quality, maintainability, security and reliability of the Moderated Learning Management System before submission.

2. Review Activities

The following activities were performed:

- Reviewed the project structure and implementation.
- Examined the automated test suite.
- Ran Cppcheck static analysis.
- Ran Lizard to measure code complexity.
- Built the application and executed the automated tests.

3. Automated Quality Assurance Results

Quality Check| Result
Cppcheck| 8 source files analysed; no reported warnings or errors
Lizard| 16 files analysed
Total NLOC| 2,357
Average cyclomatic complexity| 4.3
Automated tests| 102 passed
Failed tests| 0
Test pass rate| 100%

4. Issues Identified

4.1 High Cyclomatic Complexity

"LmsService::runApplication()" has a cyclomatic complexity of 63 and contains 426 lines of code.

Recommendation: Separate the role-specific menus into smaller functions to improve readability, maintainability and testability.

Status: Not yet fixed.

4.2 Deadline Validation

Assignment creation and extension operations require proper date and time validation.

Recommendation: Ensure deadlines are validated using the application's date-parsing functionality before they are stored.

Status: Requires further review.

4.3 Moderation Transaction Handling

The moderation and mark-release process involves multiple database updates.

Recommendation: Use a database transaction so that related updates either all succeed or all roll back if an operation fails.

Status: Requires further review.

5. Positive Findings

- Password hashing and verification have dedicated automated tests.
- Role-based access restrictions have automated tests.
- Late-penalty calculations have automated tests.
- Assignment submission and resubmission workflows have automated tests.
- File submissions have automated tests.
- Marking, moderation and mark-release workflows have automated tests.
- Mark changes and audit records have automated tests.
- All 102 automated tests passed.

6. Review Conclusion

The project passed its automated test suite, and Cppcheck reported no findings under the selected analysis settings.

Lizard identified one high-complexity function that should be refactored. Additional review is also recommended for deadline validation and database transaction handling.

The identified improvements have not yet been implemented. The review therefore records both the successful quality checks and the outstanding issues honestly.

7. Evidence

- "cppcheck-report.txt"
- "lizard-report.txt"
- "tests/test_main.cpp"
- Automated test execution using CTest