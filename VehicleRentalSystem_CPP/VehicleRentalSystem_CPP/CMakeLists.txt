cmake_minimum_required(VERSION 3.16)
project(VehicleRentalSystem LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)
add_executable(VehicleRentalSystem main.cpp)
target_link_libraries(VehicleRentalSystem PRIVATE Qt6::Widgets Qt6::Sql)
add_custom_command(TARGET VehicleRentalSystem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/images"
    "$<TARGET_FILE_DIR:VehicleRentalSystem>/images"
)
