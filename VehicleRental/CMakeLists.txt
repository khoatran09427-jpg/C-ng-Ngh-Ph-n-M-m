cmake_minimum_required(VERSION 3.16)
project(VehicleRentalSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(BUILD_APP   "Build the Qt Widgets application" ON)
option(BUILD_TESTS "Build Google Test unit tests"     ON)

# Qt6 (uu tien) hoac Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Sql)

if(BUILD_APP)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
  add_executable(VehicleRentalSystem main.cpp model.h)
  target_link_libraries(VehicleRentalSystem PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Core)
endif()

if(BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(test_model tests/test_model.cpp model.h)
  target_link_libraries(test_model PRIVATE
    GTest::gtest Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)
  include(GoogleTest)
  add_test(NAME model_tests COMMAND test_model)
endif()
if(BUILD_APP AND EXISTS ${CMAKE_SOURCE_DIR}/images)
  add_custom_command(TARGET VehicleRentalSystem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/images $<TARGET_FILE_DIR:VehicleRentalSystem>/images)
endif()

