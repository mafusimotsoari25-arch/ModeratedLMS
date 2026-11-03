# MLMS Release v1.1

## Change
CR1 adds class-wide assignment deadline extensions.

## Behaviour
- Assigned lecturers can set or update a deadline for the whole assignment class.
- Active enrolled students receive notifications.
- The assignment list shows the effective deadline.
- An individual extension overrides the class-wide deadline for that student.
- The original deadline remains in force when no extension exists.

## Database
Added `class_extension`; existing `extension` records remain available.

## Verification
- CMake configuration completed successfully.
- Build completed successfully.
- Full regression suite: 105 of 105 tests passed (100%).
- Final CTest run completed in 78.93 seconds.
- Class-wide extension, deadline application, and individual-extension override tests passed.

See `PHASE8_CR1_V1_1.md` for the change-impact analysis and test details.

## Design artefacts
The UML diagrams and other design documents were not found in the project folder during the Phase 8 review. Their CR1 updates still need to be completed or added to the repository.
