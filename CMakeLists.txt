cmake_minimum_required(VERSION 2.8)
project(toks C CXX)

find_package(Threads REQUIRED)

set(SQLITE_FLAGS "-DSQLITE_OMIT_LOAD_EXTENSION")

set(C_FLAGS "-O3 -g ${SQLITE_FLAGS}")

# Create a stand-alone executable when compiling for Windows
if(MINGW)
    set(C_FLAGS "${C_FLAGS} -static")
endif()

set(CMAKE_C_FLAGS "${C_FLAGS}")
set(CMAKE_CXX_FLAGS "${C_FLAGS} -Wall -Werror -std=c++0x")

set(VERSION "0.0")

configure_file("${PROJECT_SOURCE_DIR}/src/config.h.in"
               "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}"
                    "${PROJECT_SOURCE_DIR}/sqlite3")

add_executable(toks
sqlite3/sqlite3080200.c
src/args.cpp
src/brace_cleanup.cpp
src/chunk_list.cpp
src/ChunkStack.cpp
src/combine.cpp
src/index.cpp
src/keywords.cpp
src/lang_pawn.cpp
src/logger.cpp
src/logmask.cpp
src/md5.cpp
src/output.cpp
src/parse_frame.cpp
src/punctuators.cpp
src/scope.cpp
src/tokenize_cleanup.cpp
src/tokenize.cpp
src/toks.cpp
src/unc_text.cpp
src/unicode.cpp)

target_link_libraries(toks ${CMAKE_THREAD_LIBS_INIT})
