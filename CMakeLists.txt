cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

set(PACKAGE_NAME      "Yet-Another-Shadow-Socket")

project(${PACKAGE_NAME} CXX C)

option(WX_CONFIG_PATH "Default path to wx-config" "")

if (APPLE)
  set(WX_CONFIG_PATH
    "/opt/local/Library/Frameworks/wxWidgets.framework/Versions/wxWidgets/3.1/bin"
  )
endif()

if(NOT MSVC)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

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

if (APPLE)
    include_directories(/opt/local/include)
    link_directories(/opt/local/lib)
endif()

#add_definitions(-DBOOST_ASIO_DISABLE_KQUEUE)
#add_definitions(-DBOOST_ASIO_ENABLE_HANDLER_TRACKING)

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
    glog
    gflags
    sodium
    mbedcrypto
    jsoncpp
    boost_thread-mt
    boost_filesystem-mt
    )

find_program(
  WX_CONFIG
  wx-config
  PATHS
  ${WX_CONFIG_PATH}
  )
if (WX_CONFIG)
  add_executable(yass
    src/gui/yass.cpp
    src/gui/yass_frame.cpp
    src/gui/panels.cpp
    src/gui/worker.cpp
    $<TARGET_OBJECTS:yass_base>
    )
  target_include_directories(yass PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_link_libraries(yass PUBLIC
      glog
      gflags
      sodium
      mbedcrypto
      jsoncpp
      boost_thread-mt
      boost_filesystem-mt
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

  target_compile_options(yass PRIVATE ${WX_CONFIG_INCLUDES})

  set_target_properties(yass
    PROPERTIES LINK_FLAGS "${WX_CONFIG_LIBS}"
    )
endif()
