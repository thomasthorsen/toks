FROM ubuntu

RUN apt-get update -y
RUN apt-get upgrade -y

# Build system
RUN apt-get install cmake -y

# Compilers for Linux
RUN apt-get install gcc -y
RUN apt-get install g++ -y

# Compilers for Windows
RUN apt-get install gcc-mingw-w64-x86-64 -y
RUN apt-get install g++-mingw-w64-x86-64 -y
