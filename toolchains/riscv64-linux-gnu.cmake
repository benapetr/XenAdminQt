# CMake toolchain file for cross-compiling to riscv64-linux-gnu
# Requires: gcc-riscv64-linux-gnu, g++-riscv64-linux-gnu
# On Ubuntu: sudo apt-get install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(CMAKE_C_COMPILER riscv64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER riscv64-linux-gnu-g++)
set(CMAKE_STRIP riscv64-linux-gnu-strip)

# Where to find target libraries and headers (Ubuntu multiarch layout)
set(CMAKE_FIND_ROOT_PATH /usr/lib/riscv64-linux-gnu /usr/include/riscv64-linux-gnu)

# Never look in the sysroot for programs (use host tools)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Only look in the sysroot for libraries and headers
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
