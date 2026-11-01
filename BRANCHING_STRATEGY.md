# Git Branching Strategy – Moderated Learning Management System

## 1. Purpose
Git is used to manage source-code versions, track changes and maintain the history of the Moderated Learning Management System.

## 2. Branching Strategy

The project uses a feature-branch workflow.

- **master:** Contains the stable version of the application.
- **feature branches:** Used to develop individual features or improvements.
- **Pull requests:** Used to review and test changes before merging them into master.

## 3. Commit Management

Meaningful commit messages are used to describe changes made to the project. Examples include:

- Initial LMS implementation
- Update LMS functionality
- Build and test LMS update
- Add GitHub Actions CI pipeline

## 4. Continuous Integration

GitHub Actions automatically builds the project, executes automated tests and generates code coverage reports when changes are pushed to master or submitted through a pull request.

## 5. Release Management

Stable versions are identified using Git tags. The release tag is created after implementation, testing and quality checks have been completed.

## 6. Repository

GitHub repository: https://github.com/mafusimotsoari25-arch/ModeratedLMS