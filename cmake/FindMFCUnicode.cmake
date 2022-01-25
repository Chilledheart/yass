# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindMFCUnicode
-------

Find Microsoft Foundation Class Library (MFC) on Windows

Find the native MFC - i.e.  decide if an application can link to the
MFC libraries.

::

  MFC_UNICODE_FOUND - Was MFC support found

You don't need to include anything or link anything to use it.
#]=======================================================================]

# Assume no MFC support
set(MFC_UNICODE_FOUND "NO")

# Only attempt the try_compile call if it has a chance to succeed:
set(MFC_UNICODE_ATTEMPT_TRY_COMPILE 0)
if(WIN32 AND NOT UNIX AND NOT BORLAND AND NOT MINGW)
  set(MFC_UNICODE_ATTEMPT_TRY_COMPILE 1)
endif()

if(MFC_UNICODE_ATTEMPT_TRY_COMPILE)
  if(NOT DEFINED MFC_UNICODE_HAVE_MFC)
    set(CHECK_INCLUDE_FILE_VAR "afxwin.h")
    configure_file(${CMAKE_ROOT}/Modules/CheckIncludeFile.cxx.in
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.cxx)
    message(CHECK_START "Looking for MFC Unicode")

    if (ALLOW_XP)
      set (MFC_UNICODE_ADDITIONS_DEFINTIONS "-D_USING_V110_SDK71_ -D_WIN32_WINNT=0x0501")
    endif()
    # Try both shared and static as the root project may have set the /MT flag
    try_compile(MFC_UNICODE_HAVE_MFC
      ${CMAKE_BINARY_DIR}
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.cxx
      CMAKE_FLAGS
      -DCMAKE_MFC_UNICODE_FLAG:STRING=2
      COMPILE_DEFINITIONS "-D_UNICODE -DUNICODE -D_AFXDLL ${MFC_UNICODE_ADDITIONS_DEFINTIONS}"
      OUTPUT_VARIABLE OUTPUT)
    if(NOT MFC_UNICODE_HAVE_MFC)
      configure_file(${CMAKE_ROOT}/Modules/CheckIncludeFile.cxx.in
        ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.cxx)
      try_compile(MFC_UNICODE_HAVE_MFC
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.cxx
        CMAKE_FLAGS
        -DCMAKE_MFC_UNICODE_FLAG:STRING=1
        COMPILE_DEFINITIONS "-D_UNICODE -DUNICODE ${MFC_UNICODE_ADDITIONS_DEFINTIONS}"
        OUTPUT_VARIABLE OUTPUT)
    endif()
    if(MFC_UNICODE_HAVE_MFC)
      message(CHECK_PASS "found")
      set(MFC_UNICODE_HAVE_MFC 1 CACHE INTERNAL "Have MFC Unicode?")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Determining if MFC Unicode exists passed with the following output:\n"
        "${OUTPUT}\n\n")
    else()
      message(CHECK_FAIL "not found")
      set(MFC_UNICODE_HAVE_MFC 0 CACHE INTERNAL "Have MFC Unicode?")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Determining if MFC Unicode exists failed with the following output:\n"
        "${OUTPUT}\n\n")
    endif()
  endif()

  if(MFC_UNICODE_HAVE_MFC)
    set(MFC_UNICODE_FOUND "YES")
  endif()
endif()
