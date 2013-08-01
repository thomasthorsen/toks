cmake_minimum_required(VERSION 2.8)
project(toks CXX)

SET(CMAKE_CXX_FLAGS "-Wall -Werror -O3 -g")

add_executable(toks
src/align.cpp
src/align_stack.cpp
src/args.cpp
src/backup.cpp
src/brace_cleanup.cpp
src/braces.cpp
src/chunk_list.cpp
src/ChunkStack.cpp
src/combine.cpp
src/compat_posix.cpp
src/compat_win32.cpp
src/defines.cpp
src/detect.cpp
src/indent.cpp
src/keywords.cpp
src/lang_pawn.cpp
src/logger.cpp
src/logmask.cpp
src/md5.cpp
src/newlines.cpp
src/options.cpp
src/output.cpp
src/parens.cpp
src/parse_frame.cpp
src/punctuators.cpp
src/semicolons.cpp
src/sorting.cpp
src/space.cpp
src/tokenize_cleanup.cpp
src/tokenize.cpp
src/uncrustify.cpp
src/unc_text.cpp
src/unicode.cpp
src/universalindentgui.cpp
src/width.cpp)
