option(SQLITE_BUILD_UTILS "Build with sqlite shell" OFF)

add_library(sqlite STATIC
  sqlite3_shim.c
  sqlite3_shim_fixups.h
  src/amalgamation/rename_exports.h
  src/amalgamation/sqlite3.h
)

# sqlite_common_configuration_flags
target_compile_definitions(sqlite PUBLIC
  SQLITE_ENABLE_FTS3

  # New unicode61 tokenizer with built-in tables.
  SQLITE_DISABLE_FTS3_UNICODE

  # Chrome does not enable fts4 disable extra code.
  SQLITE_DISABLE_FTS4_DEFERRED
  # TODO: enable ICU
  #SQLITE_ENABLE_ICU

  # Defaults the secure_delete pragma to 1.
  #
  # This causes SQLite to overwrite all deleted information with zeroes
  # trading additional I/O for better privacy guarantees.
  SQLITE_SECURE_DELETE

  # TODO(pwnall): SQLite adds mutexes to protect structures which cross
  # threads. In theory Chrome should be able to turn this to 2 which
  # should give a slight speed boost. 2 is safe as long as a single
  # connection is not used by more than one thread at a time.
  SQLITE_THREADSAFE=1

  # SQLite can spawn threads to sort in parallel if configured
  # appropriately.  Chrome doesn't configure SQLite for that and would
  # prefer to control distribution to worker threads.
  SQLITE_MAX_WORKER_THREADS=0

  # Allow 256MB mmap footprint per connection.  Should not be too open-ended
  # as that could cause memory fragmentation.  50MB encompasses the 99th
  # percentile of Chrome databases in the wild.
  # TODO(pwnall): A 64-bit-specific value could be 1G or more.
  # TODO(pwnall): Figure out if exceeding this is costly.
  SQLITE_MAX_MMAP_SIZE=268435456

  # The default POSIX permissions for a newly created SQLite database.
  #
  # If unspecified this defaults to 0644. All the data stored by Chrome is
  # private so our databases use stricter settings.
  SQLITE_DEFAULT_FILE_PERMISSIONS=0600

  # Databases are opened in EXCLUSIVE mode by default.
  #
  # NORMAL mode where a database can be used by multiple processes
  # simultaneously can be enabled by executing PRAGMA locking_mode=0.
  #
  # https://www.sqlite.org/compile.html#default_locking_mode
  # https://www.sqlite.org/pragma.html#pragma_locking_mode
  SQLITE_DEFAULT_LOCKING_MODE=1

  # Needed by the SQL MemoryDumpProvider.
  #
  # Setting this to 1 is needed to collect the information reported by
  # sqlite3_status64(SQLITE_STATUS_MEMORY_USED). Without this setting the API
  # still exists but does not work as promised.
  SQLITE_DEFAULT_MEMSTATUS=1

  # Must match sql::Database::kDefaultPageSize.
  SQLITE_DEFAULT_PAGE_SIZE=4096

  # By default SQLite pre-allocates 100 pages of pcache data which will not
  # be released until the handle is closed.  This is contrary to Chrome's
  # memory-usage goals.
  SQLITE_DEFAULT_PCACHE_INITSZ=0

  # The flags below are recommended in the SQLite documentation.
  SQLITE_LIKE_DOESNT_MATCH_BLOBS
  SQLITE_OMIT_DEPRECATED
  SQLITE_OMIT_PROGRESS_CALLBACK
  SQLITE_OMIT_SHARED_CACHE
  SQLITE_USE_ALLOCA

  # Chrome does not use sqlite3_column_decltype().
  SQLITE_OMIT_DECLTYPE

  # Chrome does not use SQLite's JSON support.
  SQLITE_OMIT_JSON

  # Chrome does not use sqlite3_{enable_}load_extension().
  # Asides from giving us fairly minor code savings this option disables code
  # that breaks our method for renaming SQLite's exported symbols. Last
  # there's a tiny security benefit to knowing that WebSQL can't possibly
  # reach extension loading code.
  SQLITE_OMIT_LOAD_EXTENSION

  # Uses isnan() in the C99 standard library.
  SQLITE_HAVE_ISNAN

  # Chrome uses SQLite's built-in corruption recovery module.
  # See https://www.sqlite.org/recovery.html
  #
  # Use of this module requires compiling a separate sqlite3r.c target which
  # generates sqlite3r.c and sqlite3r.h versions of the amalgamation files
  # that include the recover extension. We then map this back to sqlite3.h
  # when generating amalgamations to hide this detail from the rest of the
  # platform. If this changes or if we ever remove this flag we would need to
  # update the mapping in scripts/generate_amalgamation.py.
  SQLITE_HAVE_SQLITE3R

  # See https://www.sqlite.org/dbpage.html
  #
  # This extension is required to use the built-in corruption recovery module
  # (see https://sqlite.org/src/info/c1f2a1d55c180fb5) which needs read access
  # to the sqlite_dbpage table. This table should not be accessed anywhere else
  # in Chrome or by untrusted third-parties (i.e. WebSQL).
  SQLITE_ENABLE_DBPAGE_VTAB
)

target_compile_definitions(sqlite PUBLIC
  SQLITE_ENABLE_BATCH_ATOMIC_WRITE
  SQLITE_TEMP_STORE=3
)

if (APPLE)
  target_compile_definitions(sqlite PUBLIC SQLITE_ENABLE_LOCKING_STYLE=1)
else()
  target_compile_definitions(sqlite PUBLIC SQLITE_ENABLE_LOCKING_STYLE=0)
endif()

if (ASAN OR TSAN OR UBSAN OR MSAN)
  # Limit max length of data blobs and queries to 128 MB for fuzzing builds.
  target_compile_definitions(sqlite PUBLIC SQLITE_MAX_LENGTH=128000000)
  target_compile_definitions(sqlite PUBLIC SQLITE_MAX_SQL_LENGTH=128000000)
  target_compile_definitions(sqlite PUBLIC SQLITE_PRINTF_PRECISION_LIMIT=1280000)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_definitions(sqlite PUBLIC SQLITE_ENABLE_API_ARMOR)
endif()

# warnings
if (COMPILER_CLANG)
  target_compile_options(sqlite PRIVATE -Wno-shadow)
  target_compile_options(sqlite PRIVATE -Wno-unused-function)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(sqlite PRIVATE -Wno-string-conversion)
  endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR COMPILER_CLANG OR COMPILER_GCC)
  target_compile_options(sqlite PRIVATE -Wno-int-to-pointer-cast)
  target_compile_options(sqlite PRIVATE -Wno-pointer-to-int-cast)
  if (COMPILER_CLANG)
    target_compile_options(sqlite PRIVATE -Wno-int-conversion)
  endif()
endif()

if (COMPILER_MSVC)
  target_compile_options(sqlite PRIVATE /wd4101)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
  target_compile_definitions(sqlite PUBLIC fdatasync=fdatasync)
endif()

if (UNIX)
  # Allow xSleep() call on Unix to use usleep() rather than sleep(), so it
  # will have microsecond precision.  Should only affect contended
  # databases via the busy callback.  Browser profile databases are mostly
  # exclusive, but renderer databases may allow for contention.
  target_compile_definitions(sqlite PUBLIC HAVE_USLEEP=1)
  # Use pread/pwrite directly rather than emulating them.
  target_compile_definitions(sqlite PUBLIC USE_PREAD=1)
endif()

target_include_directories(sqlite PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}  # sqlite3.h here must override the one in src/amalgamation/.
  src/amalgamation)

target_include_directories(sqlite PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# By default, we disable all SQLite features that can be disabled by adding a
# SQLITE_OMIT_ define, or by leaving out a SQLITE_ENABLE_ define. We only enable
# features that have compelling use cases in the codebase.
#
# Each SQLite feature carries a cost to our users (binary size, pressure on
# caches and branch predictors, extra surface area for security
# vulnerabilities), as well as a maintenance cost (more surface area to be
# covered by our fuzzers, more logic to reason through when debugging a crash
# or a security vulnerability report). Enabled features must provide benefits
# that outweigh these costs.
target_compile_definitions(sqlite PUBLIC
  # Chrome doesn't use the ANALYZE SQLite extension.
  #
  # ANALYZE [1] is non-standard and currently performs a table scan to
  # update statistics used by the query planner. Chrome uses straightforward
  # database schemas which do not require the level of fine tuning provided
  # by ANALYZE and we generally cannot afford the I/O cost of the required
  # table scans.
  #
  # [1] https://www.sqlite.org/lang_analyze.html
  SQLITE_OMIT_ANALYZE

  # Chrome initializes SQLite manually in //sql/connection.cc.
  SQLITE_OMIT_AUTOINIT

  # Chrome should not use queries with the pathologic performance cases
  # mitigated by automatic indexes.
  #
  # Disabling automatic indexing exposes the pathological performance problems
  # instead of having SQLite paper over them. This helps us catch the prbolems
  # in early testing such as pinpoint and Finch.
  #
  # As a bonus disabling automatic indexing simplifies the mental model for
  # SQLite's optimizer which makes a bit it easier to reason about SQL
  # statement performance.
  #
  # See https://www.sqlite.org/optoverview.html#autoindex
  SQLITE_OMIT_AUTOMATIC_INDEX

  # Chrome calls sqlite3_reset() correctly to reset prepared statements.
  SQLITE_OMIT_AUTORESET

  # Chromium does not use sqlite3_{getfree}_table().
  # Chrome doesn't use sqlite3_compileoption_{usedget}().
  SQLITE_OMIT_COMPILEOPTION_DIAGS

  # EXPLAIN's output is not stable across releases [1] so it should not be
  # used in Chrome. Skipping the EXPLAIN machinery also results in
  # non-trivial binary savings.
  #
  # [1] https://www.sqlite.org/lang_explain.html
  SQLITE_OMIT_EXPLAIN

  # Chrome does not use sqlite3_{getfree}_table().
  SQLITE_OMIT_GET_TABLE

  # Chrome does not use PRAGMA {functionmodulepragma}_list.
  SQLITE_OMIT_INTROSPECTION_PRAGMAS

  # Chrome already depends on malloc being very efficient so we disable
  # SQLite's arena allocator.
  SQLITE_DEFAULT_LOOKASIDE=0,0
  SQLITE_OMIT_LOOKASIDE

  # Chrome doesn't use TCL variables.
  SQLITE_OMIT_TCL_VARIABLE

  # The REINDEX statemnt is only useful if a collation sequence's definition
  # changes [1]. Chrome never defines its own collation sequences [2 3] so
  # it never needs to call REINDEX.
  #
  # [1] https://www.sqlite.org/lang_reindex.html
  # [2] https://www.sqlite.org/datatype3.html#collating_sequences
  # [3] https://www.sqlite.org/c3ref/create_collation.html
  SQLITE_OMIT_REINDEX

  # Chrome doesn't use sqlite3_{profiletrace}().
  SQLITE_OMIT_TRACE

  # Chrome doesn't use UPSERT.
  SQLITE_OMIT_UPSERT

  # Chrome doesn't use window functions in SQL.
  SQLITE_OMIT_WINDOWFUNC
)

if (APPLE)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -framework CoreFoundation -framework CoreServices")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework CoreServices")
endif()

if (ANDROID)
  target_compile_definitions(sqlite PUBLIC SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT=1048576)
  target_compile_definitions(sqlite PUBLIC SQLITE_DEFAULT_AUTOVACUUM=1)
endif()

if (SQLITE_BUILD_UTILS)
  add_executable(sqlite3
    sqlite_shell_shim.c
  )
  # SQLite's shell.c contains an '#include "sqlite3.h", which we want to be
  # resolved to //third_party/sqlite/sqlite3.h.
  target_include_directories(sqlite3 PRIVATE .)
  target_link_libraries(sqlite3 PRIVATE sqlite)
  if (USE_TCMALLOC)
    target_link_libraries(sqlite3 PRIVATE tcmalloc)
  endif()
endif()
