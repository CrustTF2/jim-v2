cmake_minimum_required(VERSION 3.16)
project(JengererItemManagerLinux LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JIM_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps" CACHE PATH "Pinned dependency sources")

if(NOT EXISTS "${JIM_DEPS_DIR}/protobuf/cmake/CMakeLists.txt" OR
   NOT EXISTS "${JIM_DEPS_DIR}/jsoncpp/src/lib_json/json_reader.cpp")
  message(FATAL_ERROR "Pinned dependencies are missing. Run ./prepare-dependencies.sh first.")
endif()
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
add_subdirectory("${JIM_DEPS_DIR}/protobuf/cmake" protobuf-build EXCLUDE_FROM_ALL)
add_library(jsoncpp STATIC "${JIM_DEPS_DIR}/jsoncpp/src/lib_json/json_reader.cpp"
    "${JIM_DEPS_DIR}/jsoncpp/src/lib_json/json_value.cpp"
    "${JIM_DEPS_DIR}/jsoncpp/src/lib_json/json_writer.cpp")
target_include_directories(jsoncpp PUBLIC "${JIM_DEPS_DIR}/jsoncpp/include")

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(CURL REQUIRED)
find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
find_library(SDL2_LIBRARY NAMES SDL2)
if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
  message(FATAL_ERROR "SDL2 development headers and libraries are required.")
endif()
if(NOT TARGET OpenGL::GLU)
  message(FATAL_ERROR "OpenGL Utility Library (GLU) is required.")
endif()

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER APP_SOURCES EXCLUDE REGEX "/(main|updater)\\.cpp$")
file(GLOB PROTO_SOURCES CONFIGURE_DEPENDS "src/protobuf/*.pb.cc")
file(GLOB_RECURSE JUI_SOURCES CONFIGURE_DEPENDS "src/jui/src/*.cpp")
list(FILTER JUI_SOURCES EXCLUDE REGEX "/application/window\\.cpp$")
file(GLOB_RECURSE JUTIL_SOURCES CONFIGURE_DEPENDS "src/jutil/src/*.cpp")
list(FILTER JUTIL_SOURCES EXCLUDE REGEX "/base/base_services\\.cpp$")

add_executable(item-manager ${APP_SOURCES} ${PROTO_SOURCES} ${JUI_SOURCES} ${JUTIL_SOURCES}
    src/jutil/include/string/string_helpers.cpp src/linux/main.cpp src/linux/jui_window.cpp
    src/linux/window.cpp src/linux/steam_linux.cpp src/linux/steam_paths.cpp)
target_include_directories(item-manager PRIVATE src src/jui/include src/jutil/include
    "${JIM_DEPS_DIR}/protobuf/src" "${SDL2_INCLUDE_DIR}")
target_link_libraries(item-manager PRIVATE libprotobuf jsoncpp Threads::Threads
    "${SDL2_LIBRARY}" OpenGL::GL OpenGL::GLU Freetype::Freetype PNG::PNG CURL::libcurl ${CMAKE_DL_LIBS})
set_target_properties(item-manager PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/package")
add_custom_command(TARGET item-manager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../windows/package/fonts" "${CMAKE_BINARY_DIR}/package/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../windows/package/img" "${CMAKE_BINARY_DIR}/package/img"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../windows/package/resources" "${CMAKE_BINARY_DIR}/package/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../windows/package/third-party-notices" "${CMAKE_BINARY_DIR}/package/third-party-notices"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/thumbnail_renderer" "${CMAKE_BINARY_DIR}/package/thumbnail_renderer"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/linux-package" "${CMAKE_BINARY_DIR}/package")
