toks
====

toks is a developer's tool for analyzing source code. Much like a normal compiler it performs tokenization and lexical analysis to identify and categorize the most useful tokens in the source code, such as:

 * Function definitions/declarations/references
 * Preprocessor definitions/macros
 * Type definitions
 * struct/union/enum definitions
 * enum value definitions
 * Variable definitions

However, unlike a normal compiler it uses fuzzy logic and heuristics to avoid the need for setting up compiler options and include paths (for languages that use such features), thus making it easy and quick to use on an unknown code base with a complicated build system. Currently, the following languages are supported:

 * C
 * C++
 * Objective-C
 * Java
 * ECMA (JavaScript/ActionScript/JScript)
 * C#
 * D
 * Pawn
 * Vala

Please note that this is a project currently under heavy development, and not everything will work the way it is supposed to and some features still need to be implemented, and testing is very limited. There are absolutely no guarantees as far as backward compatibility is concerned at this point in time. Future plans include implementing a database format for storing the identified tokens for quick retrieval. The goal is to make it easy and fast to use it from other tools such as editors and IDE's for code navigation.


Building
--------

The build system is CMake:

    toks> mkdir build
    toks> cd build
    toks/build> cmake ..
    toks/build> make

The resulting executable is build/toks.

To use LLVM/Clang, simply set CXX to clang++ when invoking cmake:

    toks/build> CXX=clang++ cmake ..
    toks/build> make

Building for Windows is only supported using the mingw compiler. To cross compile on Linux, use the following commands:

    toks/build> CXX=i586-mingw32msvc-c++ cmake ..
    toks/build> make


Usage
-----

To obtain usage information, execute:

toks --help


History
-------

This project is based on the tokenizer/lexer in uncrustify by Ben Gardner as that code base already implemented most of what was required to identify the tokens of interest to make this project possible. This project started out as a clone of that project which is evident throughout the code base. A big thank you to Ben Gardner for releasing the source code under GPL.
