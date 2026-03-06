cmake_minimum_required(VERSION 3.20)

project(RobotRemoteServerCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ROBOT_REMOTE_ENABLE_HTTP_SERVER "Enable integrated HTTP docs page for keywords" OFF)

find_package(Threads REQUIRED)

add_library(robot_remote
    src/RemoteServer.cpp
    src/XmlRpcServer.cpp
)

target_include_directories(robot_remote PUBLIC include)

target_link_libraries(robot_remote PUBLIC Threads::Threads)

if (ROBOT_REMOTE_ENABLE_HTTP_SERVER)
    target_compile_definitions(robot_remote PUBLIC ROBOT_REMOTE_ENABLE_HTTP_SERVER)
endif()

target_compile_features(robot_remote PUBLIC cxx_std_20)

add_executable(robot_remote_example examples/basic_host.cpp)

target_link_libraries(robot_remote_example PRIVATE robot_remote)

find_package(Doxygen)
if (DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    add_custom_target(docs ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
