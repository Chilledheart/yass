add_library(snappy STATIC
  src/snappy-internal.h
  src/snappy-sinksource.cc
  src/snappy-sinksource.h
  src/snappy-stubs-internal.cc
  src/snappy-stubs-internal.h
  src/snappy.cc
  src/snappy.h
)

if (WIN32)
  target_sources(snappy PRIVATE win32/snappy-stubs-public.h)
  target_include_directories(snappy SYSTEM PUBLIC win32)
elseif(APPLE)
  target_sources(snappy PRIVATE mac/snappy-stubs-public.h)
  target_include_directories(snappy SYSTEM PUBLIC mac)
else()
  target_sources(snappy PRIVATE linux/snappy-stubs-public.h)
  target_include_directories(snappy SYSTEM PUBLIC linux)
endif()

target_compile_definitions(snappy PRIVATE HAVE_CONFIG_H)

if (MSVC)
  target_compile_options(snappy PRIVATE /wd4018)
endif()
