#!/bin/sh
docker build -t toksbuild .
docker run -it -v "$PWD:/toks" toksbuild make -C /toks $1
