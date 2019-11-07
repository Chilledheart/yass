cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

set(PACKAGE_NAME      "Yet-Another-Shadow-Socket")

project(${PACKAGE_NAME} CXX C)

# *****************************************************************************************
#           Baisc Setups
# *****************************************************************************************

if(NOT MSVC)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

if(NOT MSVC)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
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
if (UNIX AND NOT APPLE)
    include_directories(/usr/include/jsoncpp)
endif()
find_library(BOOST_FILESYSTEM_LIB boost_filesystem)
if (NOT BOOST_FILESYSTEM_LIB)
    find_library(BOOST_FILESYSTEM_LIB boost_filesystem-mt)
endif()
find_library(BOOST_SYSTEM_LIB boost_system)
if (NOT BOOST_SYSTEM_LIB)
    find_library(BOOST_SYSTEM_LIB boost_system-mt)
endif()
if (UNIX AND NOT APPLE)
    find_library(PTHREAD_LIB pthread)
else()
    set(PTHREAD_LIB)
endif()

set(SUPPORT_LIBS
    ${GLOG_LIB}
    ${GFLAGS_LIB}
    ${SODIUM_LIB}
    ${CRYPTO_LIB}
    ${JSONCPP_LIB}
    ${BOOST_FILESYSTEM_LIB}
    ${BOOST_SYSTEM_LIB}
    ${PTHREAD_LIB}
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
    src/core/iobuf.cpp
    src/core/socks4_request_parser.cpp
    src/core/socks5_request_parser.cpp
    src/core/ss_request_parser.cpp
    src/socks5_connection.cpp
    src/ss_connection.cpp
    )

set(hfiles
    src/config.hpp
    src/core/base64.h
    src/core/cipher.hpp
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
    src/socks5_connection.hpp
    src/ss_connection.hpp
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
    $<TARGET_OBJECTS:yass_base>
    )

target_include_directories(yass_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

target_link_libraries(yass_cli PUBLIC
    ${SUPPORT_LIBS}
    )

add_executable(yass_server
    src/server/server.cpp
    $<TARGET_OBJECTS:yass_base>
    )

target_include_directories(yass_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

target_link_libraries(yass_server PUBLIC
    ${SUPPORT_LIBS}
    )

# *****************************************************************************************
#           Source code: GUI part
# *****************************************************************************************

find_package(wxWidgets COMPONENTS core base)
if (wxWidgets_FOUND)
    set(APP_NAME yass)
    set(SRC_FILES
        src/gui/yass.cpp
        src/gui/yass_frame.cpp
        src/gui/panels.cpp
        src/gui/worker.cpp
        $<TARGET_OBJECTS:yass_base>
        )

    if (APPLE)
        list(APPEND SRC_FILES wxmac.icns)
    endif()

    add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE
        ${SRC_FILES}
        ${wxWidgets_USE_FILE}
        )

    target_include_directories(${APP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
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
