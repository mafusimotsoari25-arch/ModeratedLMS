# Git Branching Strategy – Moderated Learning Management System

## 1. Purpose
Git is used to manage source-code versions, track changes and maintain the history of the Moderated Learning Management System.

## 2. Branching Strategy
The project uses a feature-branch workflow.

- **master:** The official default branch containing the stable version of the application.
- **main:** An additional existing branch, not used as the official default branch.
- **feature branches:** Used to develop individual features or improvements before merging.
- **Pull requests:** Used to review and test changes before merging them into master.

The remote repository uses master as its default branch.

## 3. Commit Management
Meaningful commit messages describe changes made to the project.

Examples:
- Initial LMS implementation
- Update LMS functionality
- Build and test LMS update
- Add GitHub Actions CI pipeline

## 4. Continuous Integration
GitHub Actions builds the project, executes automated tests and generates code coverage reports when changes are pushed to master or submitted through a pull request targeting master.

## 5. Release Management
Stable versions are identified using Git tags. Release tags are created after implementation, testing and quality checks have been completed.

## 6. Repository
GitHub repository: https://github.com/mafusimotsoari25-arch/ModeratedLMS