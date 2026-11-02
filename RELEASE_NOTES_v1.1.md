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
Run the full CMake/CTest suite. See `PHASE8_CR1_V1_1.md` for the change-impact matrix and test plan. Test results must be taken from the actual run; this note does not claim an unexecuted suite passed.
