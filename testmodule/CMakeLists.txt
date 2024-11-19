cmake_minimum_required(VERSION 3.4)

project(wvtable VERSION 1.0.0 LANGUAGES C)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
add_compile_options(-Wall -Wextra -Winline -ffast-math -funsafe-math-optimizations)
endif()

set(SRC ../src/WvTable.c ../src/wtdef.c)

add_library(wvtable SHARED ${SRC})

target_compile_definitions(wvtable PUBLIC WVLIB NO_FORCE_INLINE)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
set_target_properties(wvtable PROPERTIES LINK_FLAGS_RELEASE -s)
endif()
