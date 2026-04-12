# CMake toolchain file for cross-compiling to riscv64-linux-gnu
# Requires: gcc-riscv64-linux-gnu, g++-riscv64-linux-gnu
# On Ubuntu: sudo apt-get install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
#
# Pass -DCMAKE_SYSROOT=/path/to/riscv64-sysroot on the cmake command line.
# CMake will automatically add the sysroot to CMAKE_FIND_ROOT_PATH.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(CMAKE_C_COMPILER riscv64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER riscv64-linux-gnu-g++)
set(CMAKE_STRIP riscv64-linux-gnu-strip)

# Tell CMake to resolve the multiarch library/include paths inside the sysroot
set(CMAKE_LIBRARY_ARCHITECTURE riscv64-linux-gnu)

# Never look in the sysroot for programs — use host tools (moc, rcc, uic, cmake)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Only look inside the sysroot for libraries, headers, and packages
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
