set (zlib_SRCS
  adler32.c
  chromeconf.h
  compress.c
  contrib/optimizations/insert_string.h
  cpu_features.c
  cpu_features.h
  crc32.c
  crc32.h
  deflate.c
  deflate.h
  gzclose.c
  gzguts.h
  gzlib.c
  gzread.c
  gzwrite.c
  infback.c
  inffast.c
  inffast.h
  inffixed.h
  inflate.h
  inftrees.c
  inftrees.h
  trees.c
  trees.h
  uncompr.c
  zconf.h
  zlib.h
  zutil.c
  zutil.h
  )

add_library(zlib STATIC ${zlib_SRCS})

target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if (COMPILER_CLANG)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "-Wno-missing-prototypes -Wno-implicit-fallthrough -Wno-format-nonliteral -Wno-sign-compare -Wno-strict-prototypes")
elseif (COMPILER_GCC)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "-Wno-missing-prototypes -Wno-implicit-fallthrough -Wno-format-nonliteral -Wno-sign-compare -Wno-strict-prototypes -Wno-old-style-definition -Wno-unused-variable")
elseif (COMPILER_MSVC)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "/wd4131 /wd4255 /wd4245 /wd4244 /wd4100 /wd4702 /wd4127 /wd4267")
endif()

# use_x86_x64_optimizations
# xp: missing InitOnceExecuteOnce implementation
if ((OS_X86 OR OS_X64) AND NOT IOS)
  message(STATUS "zlib: x86_64 simd optimization")
  set(ZLIB_USE_X86_X64_OPTIMIZATIONS TRUE)
endif()

# TODO move it to upper folder
if (ANDROID)
  add_library(cpu_features STATIC
    ${ANDROID_NDK_ROOT}/sources/android/cpufeatures/cpu-features.h
    ${ANDROID_NDK_ROOT}/sources/android/cpufeatures/cpu-features.c
  )
  target_include_directories(cpu_features PUBLIC
    ${ANDROID_NDK_ROOT}/sources/android/cpufeatures
  )
  target_link_libraries(zlib PRIVATE cpu_features)
endif()

if ((OS_ARM AND ANDROID) OR OS_AARCH64)
  message(STATUS "zlib: neon simd optimization")
  set(ZLIB_USE_ARM_NEON TRUE)
endif()

if (OS_RISCV64)
  message(STATUS "zlib: use riscv64 rvv optimization")
  set(ZLIB_USE_RISCV64_OPTIMIZATIONS TRUE)
endif()

if (ZLIB_USE_X86_X64_OPTIMIZATIONS)
  # zlib_adler32_simd
  target_compile_definitions(zlib PUBLIC "ADLER32_SIMD_SSSE3")
  target_sources(zlib PRIVATE
    adler32_simd.c
    adler32_simd.h
  )
  if (WIN32)
    target_compile_definitions(zlib PUBLIC "X86_WINDOWS")
  else()
    target_compile_definitions(zlib PUBLIC "X86_NOT_WINDOWS")
  endif()
  if (COMPILER_CLANG OR COMPILER_GCC)
    target_compile_options(zlib PRIVATE "-mssse3")
  endif()
  # zlib_inflate_chunk_simd
  target_compile_definitions(zlib PUBLIC "INFLATE_CHUNK_SIMD_SSE2")
  if (OS_X64)
    target_compile_definitions(zlib PUBLIC "INFLATE_CHUNK_READ_64LE")
  endif()
  target_sources(zlib PRIVATE
    contrib/optimizations/chunkcopy.h
    contrib/optimizations/inffast_chunk.c
    contrib/optimizations/inffast_chunk.h
    contrib/optimizations/inflate.c
  )
  # zlib_slide_hash_simd
  target_compile_definitions(zlib PUBLIC "DEFLATE_SLIDE_HASH_SSE2")
  target_sources(zlib PRIVATE
    slide_hash_simd.h
  )
  # zlib_crc32_simd
  target_compile_definitions(zlib PUBLIC "CRC32_SIMD_SSE42_PCLMUL")
  target_sources(zlib PRIVATE
    crc32_simd.c
    crc32_simd.h
    crc_folding.c
  )
  if (COMPILER_CLANG OR COMPILER_GCC)
    target_compile_options(zlib PRIVATE -msse4.2 -mpclmul)
  endif()
elseif(ZLIB_USE_ARM_NEON)
  # zlib_adler32_simd
  target_compile_definitions(zlib PUBLIC "ADLER32_SIMD_NEON")
  target_sources(zlib PRIVATE
    adler32_simd.c
    adler32_simd.h
  )

  target_compile_definitions(zlib PUBLIC "CRC32_ARMV8_CRC32")
  if (ANDROID)
    target_compile_definitions(zlib PUBLIC "ARMV8_OS_ANDROID")
  elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(zlib PUBLIC "ARMV8_OS_LINUX")
  elseif (APPLE AND NOT IOS)
    target_compile_definitions(zlib PUBLIC "ARMV8_OS_MACOS")
  elseif (IOS)
    target_compile_definitions(zlib PUBLIC "ARMV8_OS_IOS")
  elseif (WIN32)
    target_compile_definitions(zlib PUBLIC "ARMV8_OS_WINDOWS")
  endif()

  # zlib_inflate_chunk_simd
  target_compile_definitions(zlib PUBLIC "INFLATE_CHUNK_SIMD_NEON")
  if (OS_AARCH64)
    target_compile_definitions(zlib PUBLIC "INFLATE_CHUNK_READ_64LE")
  endif()
  target_sources(zlib PRIVATE
    contrib/optimizations/chunkcopy.h
    contrib/optimizations/inffast_chunk.c
    contrib/optimizations/inffast_chunk.h
    contrib/optimizations/inflate.c
  )
  # zlib_slide_hash_simd
  target_compile_definitions(zlib PUBLIC "DEFLATE_SLIDE_HASH_NEON")
  target_sources(zlib PRIVATE
    slide_hash_simd.h
  )
  # zlib_arm_crc32
  target_sources(zlib PRIVATE
    crc32_simd.c
    crc32_simd.h
  )
elseif (ZLIB_USE_RISCV64_OPTIMIZATIONS)
  target_compile_definitions(zlib PUBLIC "RISCV_RVV")
  target_compile_definitions(zlib PUBLIC "DEFLATE_SLIDE_HASH_RVV")
  target_compile_definitions(zlib PUBLIC "ADLER32_SIMD_RVV")
  target_compile_options(zlib PRIVATE "-march=rv64gcv")
  target_sources(zlib PRIVATE inflate.c)
else()
  target_compile_definitions(zlib PUBLIC "CPU_NO_SIMD")
  target_sources(zlib PRIVATE inflate.c)
endif()
