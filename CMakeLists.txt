cmake_minimum_required(VERSION 3.1)

set(PROJECT_NAME "markov")
project(${PROJECT_NAME})

FILE(GLOB_RECURSE APP_SRC_FILES src/*h src/*.cpp src/*.c src/*.cc src/*.hh src/*.hpp src/*.hp)

add_executable(${PROJECT_NAME} ${APP_SRC_FILES})

# some compiler options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF)

## GNU Compiler Flags
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wno-sign-compare -Wreturn-type")
    
    target_link_libraries(${PROJECT_NAME} -lX11 -no-pie)

    IF(CMAKE_BUILD_TYPE MATCHES Release)
        message("Release build.")
        set(CMAKE_CXX_FLAGS "-O2 -Wreturn-type")
    else()
        message("Debug build.")
        set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wreturn-type")
    endif()
else()
    set(BUILD_ARCH "-m64")
endif()
