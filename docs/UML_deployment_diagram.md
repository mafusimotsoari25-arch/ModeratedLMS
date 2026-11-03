
# MLMS UML Deployment Diagram

The diagram below shows how the MLMS application is deployed on a school computer. The application and SQLite database run locally, allowing the system to operate without an internet connection.

```mermaid
flowchart TB
    subgraph SCHOOL["School Computer"]
        subgraph OS["Operating System"]
            APP["MLMS Application<br/>C++17 Console Application"]
            DB[("SQLite Database<br/>User, Course, Assignment,<br/>Submission, Mark and Audit Data")]
            APP -->|"Reads and writes data"| DB
        end
    end

    STUDENT["Student"]
    LECTURER["Lecturer"]
    HOD["Head of Department"]
    ADMIN["System Administrator"]

    STUDENT -->|"Username and password"| APP
    LECTURER -->|"Username and password"| APP
    HOD -->|"Username and password"| APP
    ADMIN -->|"Username and password"| APP
```
