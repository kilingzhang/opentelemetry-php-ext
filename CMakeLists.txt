cmake_minimum_required(VERSION 3.8)
ENABLE_LANGUAGE(ASM)
set(OPENTELEMETRY_VERSION 1.0)
project(opentelemetry VERSION ${OPENTELEMETRY_VERSION})

add_compile_definitions(HAVE_OPENTELEMETRY)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -g -Wdefaulted-function-deleted")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wdefaulted-function-deleted")

include_directories(.)

set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

if (APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
else ()
    list(APPEND OPENTELEMETRY_LINK_LIBRARIES rt crypt)
endif ()

SET(CMAKE_BUILD_TYPE Debug)

file(GLOB_RECURSE SRC_LIST FOLLOW_SYMLINKS src/*.c src/*.cc
        )
file(GLOB_RECURSE HEAD_FILES FOLLOW_SYMLINKS config.h include/*.h)

#add_definitions(-DHAVE_CONFIG_H)
add_definitions(-DZTS)

include_directories(BEFORE ./include ext-src/ ./)

execute_process(COMMAND php-config --includes OUTPUT_VARIABLE PHP_INCLUDES OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND php-config --extension-dir OUTPUT_VARIABLE PHP_EXTENSION_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND php-config --include-dir OUTPUT_VARIABLE PHP_SOURCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PHP_INCLUDES}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PHP_INCLUDES}")

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    execute_process(COMMAND ldconfig -p OUTPUT_VARIABLE LDCONFIG_LIST OUTPUT_STRIP_TRAILING_WHITESPACE)
    #message(STATUS LDCONFIG_LIST)
endif ()

#引入php需要的扩展源码，这里也是根据自己需要的来更改
message(${PHP_SOURCE})
include_directories(${PHP_SOURCE})
include_directories(${PHP_SOURCE}/main)
include_directories(${PHP_SOURCE}/Zend)
include_directories(${PHP_SOURCE}/sapi)
include_directories(${PHP_SOURCE}/pear)
include_directories(${PHP_SOURCE}/TSRM)
include_directories(third_party/robin-map/include)
include_directories(/usr/local/include)

find_package(boost CONFIG REQUIRED)
find_package(grpc CONFIG REQUIRED)

# lib-opentelemetry
link_directories(${LIBRARY_OUTPUT_PATH})
add_library(lib-opentelemetry SHARED ${SRC_LIST})
set_target_properties(lib-opentelemetry PROPERTIES OUTPUT_NAME "opentelemetry" VERSION ${OPENTELEMETRY_VERSION})
target_include_directories(lib-opentelemetry PRIVATE ${OPENTELEMETRY_CPP_INCLUDE_DIRS})
target_link_libraries(lib-opentelemetry PRIVATE ${OPENTELEMETRY_CPP_LIBRARIES})

# ext-opentelemetry
set(ext_src_list
        src/utils.cc
        include/utils.h
        php_opentelemetry.h
        opentelemetry.cc
        src/provider.cc
        include/provider.h
        src/sdk.cc
        include/sdk.h
        src/core.cc
        include/core.h
        src/zend_hook.cc
        include/zend_hook.h
        src/zend_hook_memcached.cc
        include/zend_hook_memcached.h
        src/zend_hook_redis.cc
        include/zend_hook_redis.h
        src/zend_hook_pdo.cc
        include/zend_hook_pdo.h
        src/zend_hook_pdo_statement.cc
        include/zend_hook_pdo_statement.h
        src/zend_hook_mysqli.cc
        include/zend_hook_mysqli.h
        src/zend_hook_curl.cc
        include/zend_hook_curl.h
        src/zend_hook_yar.cc
        include/zend_hook_yar.h
        src/zend_hook_error.cc
        include/zend_hook_error.h
        src/zend_hook_exception.cc
        include/zend_hook_exception.h
        src/otel_exporter.cc
        include/otel_exporter.h
        src/hex.cc
        include/hex.h
        include/Timer.h src/Timer.cc)
add_library(ext-opentelemetry SHARED ${ext_src_list})
set_target_properties(ext-opentelemetry PROPERTIES PREFIX "")
set_target_properties(ext-opentelemetry PROPERTIES OUTPUT_NAME "opentelemetry")
add_dependencies(ext-opentelemetry lib-opentelemetry)
target_include_directories(ext-opentelemetry PRIVATE ${OPENTELEMETRY_CPP_INCLUDE_DIRS})
target_link_libraries(ext-opentelemetry ${OPENTELEMETRY_C_FLAGS} opentelemetry)

add_custom_target(build.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})