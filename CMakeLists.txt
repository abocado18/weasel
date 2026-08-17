cmake_minimum_required(VERSION 4.4)
project(Weasel)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_C_STANDARD 17)





file(GLOB_RECURSE MY_CODE_FILES src/*.c src/*.cpp)



add_executable(${PROJECT_NAME} ${MY_CODE_FILES})

set(SDL_STATIC ON CACHE BOOL "Build sdl as static")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL-3.4)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)



message(STATUS "Release flags: ${CMAKE_CXX_FLAGS_RELEASE}")
