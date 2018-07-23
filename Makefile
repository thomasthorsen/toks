
native:
	mkdir -p builds/native
	cd builds/native && cmake ../..
	cd builds/native && make

win64:
	mkdir -p builds/win64
	cd builds/win64 && cmake -DCMAKE_TOOLCHAIN_FILE=../../scripts/toolchain-x86_64-mingw32.cmake ../..
	cd builds/win64 && make
