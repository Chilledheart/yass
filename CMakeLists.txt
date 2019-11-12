cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

set(PACKAGE_NAME      "Yet-Another-Shadow-Socket")

project(${PACKAGE_NAME} CXX C)

# *****************************************************************************************
#           Baisc Setups
# *****************************************************************************************

if (NOT MSVC)
    ## warnings
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter")

    ## c/c++ standard
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

    ## optimization flags
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")

    ## can gc eliminate any unused functions and data items
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fdata-sections -ffunction-sections")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -fdata-sections -ffunction-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fdata-sections -ffunction-sections")
    if (APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-dead_strip")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -Wl,-dead_strip")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
    else()
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -Wl,--gc-sections")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    endif()

    ## adopt LTO
    if (APPLE)
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -flto -fwhole-program-vtables")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -fwhole-program-vtables")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto -fwhole-program-vtables")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -flto -fwhole-program-vtables")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto -fwhole-program-vtables")
    endif()

    ## definitions
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_definitions(-D__STDC_CONSTANT_MACROS)
    add_definitions(-D__STDC_FORMAT_MACROS)
    if (NOT APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -Wl,--as-needed")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
    endif()

    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -pthread -pipe")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -pipe")

    # default stack frame
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
else()
    ## warnings
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /W4 /WX /wd4099 /wd4100 /wd4127 /wd4244 /wd4267 /wd4505 /wd4838 /wd4996")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /wd4099 /wd4100 /wd4127 /wd4244 /wd4267 /wd4505 /wd4838 /wd4996")

    ## c/c++ standard, disabled rtti
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 /GR-")

    ## optimization flags
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zc:inline /DNDEBUG")

    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /OPT:REF")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF")

    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /GL")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

    ## definitions
    add_definitions(/DNOMINMAX)
    add_definitions(/DUNICODE)
    add_definitions(/DWIN32_LEAN_AND_MEAN)
    add_definitions(/DWINVER=0x0601)
    add_definitions(/D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(/D_SCL_SECURE_NO_DEPRECATE)
    add_definitions(/D_UNICODE)
    add_definitions(/D_WIN32_WINNT=0x0601)

    # default stack frame
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /Oy-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Oy-")
endif()

# *****************************************************************************************
#           Debug facilities
# *****************************************************************************************

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(HAS_CLANG TRUE)
else()
    set(HAS_CLANG FALSE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(HAS_GCC TRUE)
else()
    set(HAS_GCC FALSE)
endif()

if (UNIX AND ${HAS_CLANG} AND
    CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fcolor-diagnostics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
endif()

if (UNIX AND ${HAS_GCC} AND
    CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

if (NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND HAS_CLANG)
    set(APPLY_ASAN TRUE)
else()
    set(APPLY_ASAN FALSE)
endif()

#add_definitions(-DBOOST_ASIO_DISABLE_KQUEUE)
#add_definitions(-DBOOST_ASIO_ENABLE_HANDLER_TRACKING)
# add macro GLOG_NO_ABBREVIATED_SEVERITIES
add_definitions(-DGLOG_NO_ABBREVIATED_SEVERITIES)

# *****************************************************************************************
#           Support Libraries
# *****************************************************************************************

# change wxWidgets_CONFIG_EXECUTABLE
if (APPLE)
    set(wxWidgets_CONFIG_EXECUTABLE
      "/opt/local/Library/Frameworks/wxWidgets.framework/Versions/wxWidgets/3.1/bin/wx-config"
    )
endif()

if (APPLE)
    include_directories(/opt/local/include)
    link_directories(/opt/local/lib)
endif()

if (UNIX AND NOT APPLE)
    include_directories(/usr/include/jsoncpp)
endif()

if (WIN32)
    option(VCPKG_ROOT_DIR "" "vcpkg root path")
    set(VCPKG_INCLUDE_DIRS
      ${VCPKG_ROOT_DIR}/installed/x86-windows-static/include
      ${VCPKG_ROOT_DIR}/installed/x86-windows/include)
    set(VCPKG_BIN_PATH ${VCPKG_ROOT_DIR}/installed/x86-windows/bin)
    set(VCPKG_LIB_PATH ${VCPKG_ROOT_DIR}/installed/x86-windows/lib)
    set(VCPKG_STATIC_LIB_PATH ${VCPKG_ROOT_DIR}/installed/x86-windows-static/lib)
    set(VCPKG_LIB_DEBUG_PATH ${VCPKG_ROOT_DIR}/installed/x86-windows/debug/lib)
    set(VCPKG_STATIC_LIB_DEBUG_PATH ${VCPKG_ROOT_DIR}/installed/x86-windows-static/debug/lib)

    add_definitions(-DGLOG_STATIC)
    add_definitions(-DGFLAGS_DLL_DECLARE_FLAG=)
    add_definitions(-DGFLAGS_DLL_DEFINE_FLAG=)
    add_definitions(-DGOOGLE_GLOG_DLL_DECL=)
    include_directories(${VCPKG_INCLUDE_DIRS})

    find_library(JEMALLOC_RELEASE_LIB jemalloc
      PATHS ${VCPKG_LIB_PATH} NO_DEFAULT_PATH)
    find_library(JEMALLOC_DEBUG_LIB jemalloc
      PATHS ${VCPKG_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(JEMALLOC_LIB
      debug ${JEMALLOC_DEBUG_LIB}
      optimized ${JEMALLOC_RELEASE_LIB}
      )

    find_library(GLOG_RELEASE_LIB glog
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(GLOG_DEBUG_LIB glog
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(GLOG_LIB
      debug ${GLOG_DEBUG_LIB}
      optimized ${GLOG_RELEASE_LIB}
      )

    find_library(GFLAGS_RELEASE_LIB gflags_static
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(GFLAGS_DEBUG_LIB gflags_static_debug
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(GFLAGS_LIB
      debug ${GFLAGS_DEBUG_LIB}
      optimized ${GFLAGS_RELEASE_LIB}
      )

    find_library(SODIUM_RELEASE_LIB libsodium
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(SODIUM_DEBUG_LIB libsodium
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(SODIUM_LIB
      debug ${SODIUM_DEBUG_LIB}
      optimized ${SODIUM_RELEASE_LIB}
      )

    find_library(JSONCPP_RELEASE_LIB jsoncpp
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(JSONCPP_DEBUG_LIB jsoncpp
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(JSONCPP_LIB
      debug ${JSONCPP_DEBUG_LIB}
      optimized ${JSONCPP_RELEASE_LIB}
      )

    find_library(CRYPTO_RELEASE_LIB libeay32
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(CRYPTO_DEBUG_LIB libeay32
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(CRYPTO_LIB
      debug ${CRYPTO_DEBUG_LIB}
      optimized ${CRYPTO_RELEASE_LIB}
      )

    find_library(BOOST_FILESYSTEM_RELEASE_LIB boost_filesystem-vc140-mt
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(BOOST_FILESYSTEM_DEBUG_LIB boost_filesystem-vc140-mt-gd
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(BOOST_FILESYSTEM_LIB
      debug ${BOOST_FILESYSTEM_DEBUG_LIB}
      optimized ${BOOST_FILESYSTEM_RELEASE_LIB}
      )

    find_library(BOOST_SYSTEM_RELEASE_LIB boost_system-vc140-mt
      PATHS ${VCPKG_STATIC_LIB_PATH} NO_DEFAULT_PATH)
    find_library(BOOST_SYSTEM_DEBUG_LIB boost_system-vc140-mt-gd
      PATHS ${VCPKG_STATIC_LIB_DEBUG_PATH} NO_DEFAULT_PATH)
    set(BOOST_SYSTEM_LIB
      debug ${BOOST_SYSTEM_DEBUG_LIB}
      optimized ${BOOST_SYSTEM_RELEASE_LIB}
      )

    find_library(ShLwApi_LIB ShLwApi)
    set(SYSTEM_LIBS ${ShLwApi_LIB})

    # replace /MD with /MT
    set(CompilerFlags
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_C_FLAGS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELEASE
      )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
else()
    find_library(JEMALLOC_LIB jemalloc)
    find_library(GLOG_LIB glog)
    find_library(GFLAGS_LIB gflags)
    find_library(SODIUM_LIB sodium)
    if (APPLE)
        find_library(CRYPTO_LIB crypto PATHS /usr/lib NO_DEFAULT_PATH)
    else()
        find_library(CRYPTO_LIB crypto)
    endif()
    find_library(JSONCPP_LIB jsoncpp)

    find_library(BOOST_FILESYSTEM_LIB boost_filesystem)
    if (NOT BOOST_FILESYSTEM_LIB)
        find_library(BOOST_FILESYSTEM_LIB boost_filesystem-mt)
    endif()
    find_library(BOOST_SYSTEM_LIB boost_system)
    if (NOT BOOST_SYSTEM_LIB)
        find_library(BOOST_SYSTEM_LIB boost_system-mt)
    endif()
endif()

set(SUPPORT_LIBS
    ${GLOG_LIB}
    ${GFLAGS_LIB}
    ${SODIUM_LIB}
    ${CRYPTO_LIB}
    ${JSONCPP_LIB}
    ${BOOST_FILESYSTEM_LIB}
    ${BOOST_SYSTEM_LIB}
    ${SYSTEM_LIBS}
    )

if (APPLY_ASAN)
    add_compile_options(-g3 -fsanitize=address)
    add_link_options(-fsanitize=address)
else()
    set(SUPPORT_LIBS
        ${JEMALLOC_LIB}
        ${SUPPORT_LIBS})
endif()

# *****************************************************************************************
#           Source code
# *****************************************************************************************

set(files
    src/config.cpp
    src/core/base64.c
    src/core/cipher.cpp
    src/core/http_parser.c
    src/core/iobuf.cpp
    src/core/socks4_request_parser.cpp
    src/core/socks5_request_parser.cpp
    src/core/ss_request_parser.cpp
    )

set(hfiles
    src/config.hpp
    src/core/base64.h
    src/core/cipher.hpp
    src/core/http_parser.h
    src/core/iobuf.hpp
    src/core/socks4.hpp
    src/core/socks4_request.hpp
    src/core/socks4_request_parser.hpp
    src/core/socks5.hpp
    src/core/socks5_request.hpp
    src/core/socks5_request_parser.hpp
    src/core/ss.hpp
    src/core/ss_request.hpp
    src/core/ss_request_parser.hpp
    )

add_library(yass_base OBJECT
    ${files} ${hfiles}
    )

set_target_properties(yass_base PROPERTIES POSITION_INDEPENDENT_CODE True)
target_include_directories(yass_base PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

add_executable(yass_cli
    src/cli/cli.cpp
    src/cli/socks5_connection.cpp
    src/cli/socks5_connection.hpp
    $<TARGET_OBJECTS:yass_base>
    )

target_include_directories(yass_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli)

target_link_libraries(yass_cli PUBLIC
    ${SUPPORT_LIBS}
    )

add_executable(yass_server
    src/server/server.cpp
    src/server/ss_connection.cpp
    src/server/ss_connection.hpp
    $<TARGET_OBJECTS:yass_base>
    )

target_include_directories(yass_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
    )

target_link_libraries(yass_server PUBLIC
    ${SUPPORT_LIBS}
    )

# *****************************************************************************************
#           Source code: GUI part
# *****************************************************************************************


if (WIN32)
  set(wxWidgets_ROOT_DIR ${VCPKG_ROOT_DIR}/installed/x86-windows-static)
  set(wxWidgets_LIB_DIR ${VCPKG_STATIC_LIB_PATH})
  set(wxWidgets_LIB_DEBUG_DIR ${VCPKG_STATIC_LIB_DEBUG_PATH})
  set(wxWidgets_INCLUDE_DIRS
      ${wxWidgets_ROOT_DIR}/include
      ${wxWidgets_ROOT_DIR}/include/msw)

  # find jpeg library
  find_library(JPEG_RELEASE_LIB jpeg
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(JPEG_DEBUG_LIB jpegd
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(JPEG_LIB
    debug ${JPEG_DEBUG_LIB}
    optimized ${JPEG_RELEASE_LIB}
    )

  # find libpng16 library
  find_library(PNG16_RELEASE_LIB libpng16
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(PNG16_DEBUG_LIB libpng16d
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(PNG16_LIB
    debug ${PNG16_DEBUG_LIB}
    optimized ${PNG16_RELEASE_LIB}
    )

  # find lzma library
  find_library(LZMA_RELEASE_LIB lzma
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(LZMA_DEBUG_LIB lzmad
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(LZMA_LIB
    debug ${LZMA_DEBUG_LIB}
    optimized ${LZMA_RELEASE_LIB}
    )

  # find tiff library
  find_library(TIFF_RELEASE_LIB tiff
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(TIFF_DEBUG_LIB tiffd
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(TIFF_LIB
    debug ${TIFF_DEBUG_LIB}
    optimized ${TIFF_RELEASE_LIB}
    )

  # find tiffxx library
  find_library(TIFFXX_RELEASE_LIB tiffxx
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(TIFFXX_DEBUG_LIB tiffxxd
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(TIFFXX_LIB
    debug ${TIFFXX_DEBUG_LIB}
    optimized ${TIFFXX_RELEASE_LIB}
    )

  # find turbojpeg library
  find_library(TURBOJPEG_RELEASE_LIB turbojpeg
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(TURBOJPEG_DEBUG_LIB turbojpegd
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(TURBOJPEG_LIB
    debug ${TURBOJPEG_DEBUG_LIB}
    optimized ${TURBOJPEG_RELEASE_LIB}
    )

  # find zlib library
  find_library(ZLIB_RELEASE_LIB zlib
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(ZLIB_DEBUG_LIB zlibd
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(ZLIB_LIB
    debug ${ZLIB_DEBUG_LIB}
    optimized ${ZLIB_RELEASE_LIB}
    )

  # find wxbase library
  find_library(WX_BASE_RELEASE_LIB wxbase31u
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(WX_BASE_DEBUG_LIB wxbase31ud
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(WX_BASE_LIB
    debug ${WX_BASE_DEBUG_LIB}
    optimized ${WX_BASE_RELEASE_LIB}
    )

  # find wxcore library
  find_library(WX_CORE_RELEASE_LIB wxmsw31u_core
    PATHS ${wxWidgets_LIB_DIR} NO_DEFAULT_PATH)
  find_library(WX_CORE_DEBUG_LIB wxmsw31ud_core
    PATHS ${wxWidgets_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  set(WX_CORE_LIB
    debug ${WX_CORE_DEBUG_LIB}
    optimized ${WX_CORE_RELEASE_LIB}
    )

  find_library(COMCTL32_LIB COMCTL32)
  find_library(RPCRT4_LIB RPCRT4)
  find_library(UUID_LIB UUID)

  set(wxWidgets_LIBRARIES
      ${JPEG_LIB}
      ${PNG16_LIB}
      ${LZMA_LIB}
      ${TIFF_LIB}
      ${TIFFXX_LIB}
      ${TURBOJPEG_LIB}
      ${ZLIB_LIB}
      ${COMCTL32_LIB}
      ${RPCRT4_LIB}
      ${UUID_LIB}
      ${WX_BASE_LIB}
      ${WX_CORE_LIB})

  set(wxWidgets_DEFINITIONS
      wxUSE_GUI
      wxUSE_UNICODE
      __WXMSW__
      )
  set(wxWidgets_FOUND TRUE)
else()
  find_package(wxWidgets COMPONENTS core base)
endif()
if (wxWidgets_FOUND)
    set(APP_NAME yass)
    set(SRC_FILES
        src/gui/yass.cpp
        src/gui/yass_frame.cpp
        src/gui/panels.cpp
        src/gui/worker.cpp
        src/cli/socks5_connection.cpp
        src/cli/socks5_connection.hpp
        $<TARGET_OBJECTS:yass_base>
        )

    if (WIN32)
        list(APPEND SRC_FILES yass.rc)
    endif()

    if (APPLE)
        list(APPEND SRC_FILES wxmac.icns)
    endif()

    add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE
        ${SRC_FILES}
        ${wxWidgets_USE_FILE}
        )

    target_include_directories(${APP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
        ${wxWidgets_INCLUDE_DIRS}
        )

    target_link_libraries(${APP_NAME} PUBLIC
        ${SUPPORT_LIBS}
        ${wxWidgets_LIBRARIES}
        )

    target_compile_options(${APP_NAME} PRIVATE ${wxWidgets_CXX_FLAGS})
    foreach(DEF IN LISTS wxWidgets_DEFINITIONS)
        target_compile_definitions(${APP_NAME} PRIVATE ${DEF})
    endforeach()

    target_link_directories(${APP_NAME} PRIVATE ${wxWidgets_LIBRARY_DIRS})

    if (APPLE)
        set_target_properties(${APP_NAME} PROPERTIES
            RESOURCE "wxmac.icns"
            MACOSX_BUNDLE_ICON_FILE wxmac.icns
            MACOSX_BUNDLE_COPYRIGHT "Copyright ${PACKAGE_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "it.yass.gui"
            MACOSX_BUNDLE_BUNDLE_NAME ${PACKAGE_NAME}
          )
    endif()
endif()
