# cpu-frequency
Tool to black box determine the frequency CPU cores are running at.

cpu-frequency can be used as a library or as a standalone tool.

## Dependencies
NASM - https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/win64/

CMake - https://cmake.org/download/

Ninja - https://github.com/ninja-build/ninja/releases

Make sure all the above are available on your PATH

## Building
Linux: cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE="$GGP_SDK_PATH/cmake/ggp.cmake" .
Windows: cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE="%GGP_SDK_PATH%/cmake/ggp.cmake" .

Followed by
cmake --build .

## Uploading
ggp ssh put cpu-frequency

## Running
ggp ssh shell -- /mnt/developer/cpu-frequency --threads 7
