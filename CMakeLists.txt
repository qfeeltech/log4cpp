cmake_minimum_required(VERSION 2.8.0)

if(BUILD_FOR_RK3326)
    set(BUILD_FOR_RK3326 ON)
    set(BUILD_TARGET build_rk3326)
elseif(BUILD_FOR_RV1126)
    set(BUILD_FOR_RV1126 ON)
    set(BUILD_TARGET build_rv1126)
else()
    set(BUILD_FOR_RV1108 ON)
    set(BUILD_TARGET build_rv1108)
endif()

set(MAIN_PRJ_SRC_DIR         ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "Building BasickModules for target: " ${BUILD_TARGET})

if(NOT ROOT_DIR)
    include(${MAIN_PRJ_SRC_DIR}/compiler/${BUILD_TARGET}/toolchain.cmake)
endif()

if(NOT ROOT_DIR)
    set(LIBS_P8                 ${MAIN_PRJ_SRC_DIR}/libs/${BUILD_TARGET})
    include_directories(${LIBS_P8}/include)
    link_directories(${LIBS_P8}/system)
    link_directories(${LIBS_P8}/lib)
endif()


add_compile_options(-std=c++11 -pthread -fpermissive -Wall -w -O3 )

set(CMAKE_BUILD_TYPE Release)


project(LogFilter)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(LogFilter_SOURCE
    src/log_filter.cpp
    src/test_logFilter.cpp
    src/WebsocketLogSend.cpp
    )
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/loglib)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
#link_libraries(liblog4cpp.a)
add_executable( logFilter src/log_filter.cpp src/WebsocketLogSend.cpp)
target_link_libraries(logFilter boost_regex cjson pthread boost_system curl crypto ssl log4cpp z)
add_executable( test_logFilter src/test_logFilter.cpp)

