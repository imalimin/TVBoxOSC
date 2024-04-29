cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DANDROID)
add_definitions(-DNDEBUG)

set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/src/main/jni/third_party)
set(SOURCE_CPP_PATH ${CMAKE_SOURCE_DIR}/src/main/jni/ijkmedia)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/main/jni/android-ndk-prof)
add_subdirectory(${SOURCE_CPP_PATH}/ijksdl)
add_subdirectory(${SOURCE_CPP_PATH}/ijkplayer)