cmake_minimum_required(VERSION 3.26)
project(mitsuba_camera_generator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(glm REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

add_executable(mitsuba_camera_generator main.cpp)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -mavx2 -mavx)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${Boost_LIBRARIES})
