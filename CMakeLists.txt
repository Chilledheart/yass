cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

set(PACKAGE_NAME      "Yet-Another-Shadow-Socket")

project(${PACKAGE_NAME} CXX C)

option(WX_CONFIG_PATH "Default path to wx-config" "")

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

if (UNIX AND ${HAS_CLANG} AND
    CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fcolor-diagnostics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
endif()

if (NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g3 -fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

#add_definitions(-DBOOST_ASIO_DISABLE_KQUEUE)
#add_definitions(-DBOOST_ASIO_ENABLE_HANDLER_TRACKING)

# *****************************************************************************************
#           Support Libraries
# *****************************************************************************************

if (APPLE)
    set(WX_CONFIG_PATH
      "/opt/local/Library/Frameworks/wxWidgets.framework/Versions/wxWidgets/3.1/bin"
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
find_library(BOOST_THREAD_LIB boost_thread-mt)
find_library(BOOST_FILESYSTEM_LIB boost_filesystem-mt)

set(SUPPORT_LIBS
    ${JEMALLOC_LIB}
    ${GLOG_LIB}
    ${GFLAGS_LIB}
    ${SODIUM_LIB}
    ${CRYPTO_LIB}
    ${JSONCPP_LIB}
    ${BOOST_THREAD_LIB}
    ${BOOST_FILESYSTEM_LIB}
    )

# *****************************************************************************************
#           Source code
# *****************************************************************************************

set(files
    src/base64.c
    src/cipher.cpp
    src/config.cpp
    src/socks5_connection.cpp
    src/socks5_request_parser.cpp
    src/iobuf.cpp
    )

set(hfils
    src/base64.h
    )

add_library(yass_base OBJECT
    ${files} ${hfiles}
    )

set_target_properties(yass_base PROPERTIES POSITION_INDEPENDENT_CODE True)

add_executable(yass_cli
    src/local.cpp
    $<TARGET_OBJECTS:yass_base>
    )

target_link_libraries(yass_cli PUBLIC
    ${SUPPORT_LIBS}
    )

find_program(
    WX_CONFIG
    wx-config
    PATHS
    ${WX_CONFIG_PATH}
    )

# *****************************************************************************************
#           Source code: GUI part
# *****************************************************************************************

if (WX_CONFIG)
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
        )

    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_link_libraries(${APP_NAME} PUBLIC
        ${SUPPORT_LIBS}
        )

    # "-framework wxWidgets.framework" fix by wx-config
    execute_process(COMMAND ${WX_CONFIG} --cxxflags
        OUTPUT_VARIABLE WX_CONFIG_INCLUDES
      )
    string(REGEX REPLACE "\n$" "" WX_CONFIG_INCLUDES "${WX_CONFIG_INCLUDES}")
    separate_arguments(WX_CONFIG_INCLUDES)

    execute_process(COMMAND ${WX_CONFIG} --libs
        OUTPUT_VARIABLE WX_CONFIG_LIBS
      )

    string(REGEX REPLACE "\n$" "" WX_CONFIG_LIBS "${WX_CONFIG_LIBS}")

    target_compile_options(${APP_NAME} PRIVATE ${WX_CONFIG_INCLUDES})

    set_target_properties(${APP_NAME}
        PROPERTIES LINK_FLAGS "${WX_CONFIG_LIBS}"
      )

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
