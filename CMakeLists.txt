cmake_minimum_required(VERSION 2.8.9)

project(avrouter)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
set(CMAKE_COMPILER_IS_CLANGXX 1)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
if(UNIX)
set(CMAKE_COMPILER_IS_UNIX_ICC 1)
else()
# CMAKE_COMPILER_IS_WIN_ICC 的 c++11 模式命令行参数不一样
set(CMAKE_COMPILER_IS_WIN_ICC 1)
endif()
endif()

if(MSVC)
	set (Boost_USE_MULTITHREADED TRUE)
	set (Boost_USE_STATIC_LIBS TRUE)
endif()

find_package(Threads REQUIRED)
find_package(Boost 1.54 COMPONENTS thread system coroutine context regex date_time chrono filesystem REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
#find_package(PkgConfig REQUIRED)

#pkg_check_modules(PKCS11 REQUIRED libpkcs11-helper-1 )

include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
#include_directories(${PKCS11_INCLUDE_DIRS})
include_directories(include)
include_directories(proto_doc)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/proto_doc)

link_directories(${Boost_LIB_DIRS})

link_libraries(${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${PKCS11_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

find_library(SOCI_LIBRARY NAMES soci_core)
find_library(SOCI_POSTGRES_LIBRARY NAMES soci_postgresql)

if(NOT SOCI_LIBRARY)
message(FATAL_ERROR "SOCI library NOT FOUND!" )
endif()

if(NOT SOCI_POSTGRES_LIBRARY)
message(FATAL_ERROR "SOCI do not have postgresql backend!" )
endif()


link_libraries(${SOCI_LIBRARY})

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_UNIX_ICC)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

endif()

add_subdirectory(proto_doc)
add_subdirectory(src)

