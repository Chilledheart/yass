project(zlib C)

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
  # TODO check simd
  inflate.c
  )

add_library(zlib STATIC ${zlib_SRCS})

target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(zlib PRIVATE "-DCPU_NO_SIMD")
if (COMPILER_CLANG)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "-Wno-missing-prototypes -Wno-implicit-fallthrough -Wno-format-nonliteral -Wno-sign-compare -Wno-strict-prototypes")
elseif (COMPILER_GCC)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "-Wno-missing-prototypes -Wno-implicit-fallthrough -Wno-format-nonliteral -Wno-sign-compare -Wno-strict-prototypes -Wno-old-style-definition -Wno-unused-variable")
elseif (COMPILER_MSVC)
  set_target_properties(zlib PROPERTIES
    COMPILE_FLAGS "/wd4131 /wd4255 /wd4245 /wd4244 /wd4100 /wd4702 /wd4127")
endif()
