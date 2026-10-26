cmake_minimum_required(VERSION 3.20)
project(ModeratedLMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(lms_core
    src/Database.cpp
    src/PasswordService.cpp
    src/LatePenalty.cpp
    src/AuditRepository.cpp
    src/Notification.cpp
    src/UserFactory.cpp
    src/LmsService.cpp
)
target_include_directories(lms_core PUBLIC include)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto)
if(MSVC)
    target_compile_options(lms_core PRIVATE /W4)
else()
    target_compile_options(lms_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_core)

