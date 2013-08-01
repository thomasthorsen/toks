toks
====

toks is a token searcher for source files. It allows the user to 
generate a list of any or specific tokens in a source file along 
with useful information about the tokens.

Supported languages:
 * C
 * C++
 * Objective-C
 * Java
 * JavaScript
 * C#
 * D
 * Pawn
 * Vala


Building
--------

 $ mkdir build
 $ cd build
 $ cmake ..
 $ make

The executable is build/toks.


Usage
-----

To obtain usage information, execute:

toks --help


History
-------

Based on the tokenizer in uncrustify by Ben Gardner.

