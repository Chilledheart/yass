set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ${GCC_SYSTEM_PROCESSOR})

set(CMAKE_C_COMPILER_TARGET ${GCC_TARGET} CACHE STRING "")
set(CMAKE_CXX_COMPILER_TARGET ${GCC_TARGET} CACHE STRING "")
set(CMAKE_ASM_COMPILER_TARGET ${GCC_TARGET} CACHE STRING "")

set(CMAKE_ASM_FLAGS "--start-no-unused-arguments -rtlib=compiler-rt -fuse-ld=lld --end-no-unused-arguments")
set(CMAKE_C_FLAGS "--start-no-unused-arguments -rtlib=compiler-rt -fuse-ld=lld --end-no-unused-arguments")
set(CMAKE_CXX_FLAGS "--start-no-unused-arguments -rtlib=compiler-rt -stdlib=libc++ -fuse-ld=lld --end-no-unused-arguments")
set(CMAKE_SHARED_LINKER_FLAGS "-rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld")
set(CMAKE_EXE_LINKER_FLAGS "-rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld")

# cross compilers to use for C and C++
set(CMAKE_C_COMPILER "${LLVM_SYSROOT}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_SYSROOT}/bin/clang++")
set(CMAKE_AR "${LLVM_SYSROOT}/bin/llvm-ar")
set(CMAKE_ASM_COMPILER_AR "${LLVM_SYSROOT}/bin/llvm-ar")
set(CMAKE_C_COMPILER_AR "${LLVM_SYSROOT}/bin/llvm-ar")
set(CMAKE_CXX_COMPILER_AR "${LLVM_SYSROOT}/bin/llvm-ar")
set(CMAKE_RANLIB "${LLVM_SYSROOT}/bin/llvm-ranlib")
set(CMAKE_ASM_COMPILER_RANLIB "${LLVM_SYSROOT}/bin/llvm-ranlib")
set(CMAKE_C_COMPILER_RANLIB "${LLVM_SYSROOT}/bin/llvm-ranlib")
set(CMAKE_CXX_COMPILER_RANLIB "${LLVM_SYSROOT}/bin/llvm-ranlib")
# disabled because it is missing from binary
# set(CMAKE_RC_COMPILER "${LLVM_SYSROOT}/bin/llvm-rc")
set(CMAKE_RC_COMPILER "${LLVM_SYSROOT}/bin/${GCC_TARGET}-windres")

set(CMAKE_SYSROOT "${LLVM_SYSROOT}/${GCC_TARGET}" CACHE STRING "")

# target environment on the build host system
set(CMAKE_FIND_ROOT_PATH ${LLVM_SYSROOT}/${GCC_TARGET})

# modify default behavior of FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
