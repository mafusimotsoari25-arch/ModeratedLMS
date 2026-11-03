
cmake_minimum_required(VERSION 3.20)
project(ModeratedLMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
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

# Phase 5: Automated testing
enable_testing()
find_package(GTest REQUIRED)

add_executable(lms_tests
    tests/test_main.cpp
)

target_link_libraries(lms_tests PRIVATE lms_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lms_tests)

# Phase 5: Code coverage
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(lms_core PRIVATE --coverage -O0 -g)

        target_link_options(lms_tests PRIVATE --coverage)
        target_link_options(lms PRIVATE --coverage)
    else()
        message(FATAL_ERROR "Code coverage is configured for GCC only.")
    endif()
endif()