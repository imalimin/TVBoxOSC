cmake_minimum_required(VERSION 3.4.1)
project(ijkplayer LANGUAGES C CXX)

set(INCLUDES
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/../
        ${THIRD_PARTY_DIR}/ffmpeg
        ${THIRD_PARTY_DIR}/ffmpeg/include
        ${THIRD_PARTY_DIR}/ijkj4a/include
        ${THIRD_PARTY_DIR}/ijkj4a/include/ijkj4a
        ${THIRD_PARTY_DIR}/ijksoundtouch/include
        ${PROJECT_SOURCE_DIR}/../../android-ndk-prof
        )
include_directories(${INCLUDES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_FLAGS_CONFIGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_CONFIGS}")

message(STATUS "${PROJECT_NAME} CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

#------------------SHARED START----------------------
add_library(ijkffmpeg SHARED IMPORTED)
set_target_properties(ijkffmpeg PROPERTIES IMPORTED_LOCATION
        "${THIRD_PARTY_DIR}/ffmpeg/${ANDROID_ABI}/libijkffmpeg.so")

add_library(ijksoundtouch STATIC IMPORTED)
set_target_properties(ijksoundtouch PROPERTIES IMPORTED_LOCATION
        "${THIRD_PARTY_DIR}/ijksoundtouch/${ANDROID_ABI}/libijksoundtouch.a")

#------------------SHARED END----------------------

aux_source_directory(${PROJECT_SOURCE_DIR} SOURCES_ROOT)
aux_source_directory(${PROJECT_SOURCE_DIR}/android SOURCES_ANDROID)
aux_source_directory(${PROJECT_SOURCE_DIR}/android/pipeline SOURCES_ANDROID_PIP)
aux_source_directory(${PROJECT_SOURCE_DIR}/ijkavformat SOURCES_FORMAT)
aux_source_directory(${PROJECT_SOURCE_DIR}/ijkavutil SOURCES_UTIL)
aux_source_directory(${PROJECT_SOURCE_DIR}/pipeline SOURCES_PIP)

list(APPEND SOURCES_ROOT ${SOURCES_ROOT})
list(APPEND SOURCES_ROOT ${SOURCES_ANDROID})
list(APPEND SOURCES_ROOT ${SOURCES_ANDROID_PIP})
list(APPEND SOURCES_ROOT ${SOURCES_FORMAT})
list(APPEND SOURCES_ROOT ${SOURCES_UTIL})
list(APPEND SOURCES_ROOT ${SOURCES_PIP})

if (${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set_property(SOURCE ${SOURCES_ROOT} APPEND_STRING PROPERTY COMPILE_FLAGS " -mfpu=neon")
    add_definitions("-DHAVE_NEON=1")
elseif (${ANDROID_ABI} STREQUAL "x86")
    set_property(SOURCE ${SOURCES_ROOT} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -mssse3  -Wno-unknown-attributes \
                   -Wno-deprecated-declarations \
                   -Wno-constant-conversion \
                   -Wno-static-in-inline")
    add_definitions(-DHAVE_NEON_X86=1 -DHAVE_NEON=1)
endif ()

find_library(
        log-lib
        log)
add_library(${PROJECT_NAME} SHARED ${SOURCES_ROOT})
target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC
        GLESv2
        GLESv3
        EGL
        android
        ${log-lib}
        android-ndk-prof
        ijkffmpeg
        ijksoundtouch
        ijksdl)
