PHP_ARG_WITH(opentelemetry, for opentelemetry support,
Make sure that the comment is aligned:
[  --with-opentelemetry             Include opentelemetry support])

if test "$PHP_THREAD_SAFETY" == "yes"; then
  AC_MSG_ERROR([opentelemetry does not support ZTS])
fi

AC_MSG_RESULT([cpu in $host_cpu])

AC_DEFINE([OPENTELEMETRY_DEBUG], 1, [Whether debug is enabled])

AC_LANG_PUSH(C++)
AX_CHECK_COMPILE_FLAG([-std=c++0x], , [AC_MSG_ERROR([compiler not accept c++11])])
AC_LANG_POP()

PHP_REQUIRE_CXX()

CXXFLAGS="-g -O0 $CXXFLAGS -g -O0 -std=c++11 -Wno-maybe-uninitialized -Wno-unused-variable -Wno-sign-compare -Wno-invalid-offsetof -Wall -Wno-unused-function -Wno-deprecated -Wno-deprecated-declarations"

if test "$host_cpu" = "aarch64"; then
    CXXFLAGS="$CXXFLAGS -mno-outline-atomics"
fi


dnl check for php json
AC_MSG_CHECKING([check for php json])
json_inc_path=""
if test -f "$abs_srcdir/include/php/ext/json/php_json.h"; then
  json_inc_path="$abs_srcdir/include/php"
elif test -f "$abs_srcdir/ext/json/php_json.h"; then
  json_inc_path="$abs_srcdir"
elif test -f "$phpilncludedir/ext/json/php_json.h"; then
  json_inc_path="$phpincludedir"
else
  for i in php php4 php5 php6 php7 php8; do
    if test -f "$prefix/include/$i/ext/json/php_json.h"; then
      json_inc_path="$prefix/include/$i"
    fi
  done
fi

if test "$json_inc_path" = ""; then
  AC_MSG_ERROR([Could not fond php_json.h, please reinstall the php-json extension])
else
  AC_MSG_RESULT([found in $json_inc_path])
fi


if test "$PHP_OPENTELEMETRY" != "no"; then


  LIBS="-lpthread $LIBS"
  OPENTELEMETRY_SHARED_LIBADD="-lpthread $OPENTELEMETRY_SHARED_LIBADD"
  PHP_ADD_LIBRARY(pthread)

  PHP_ADD_LIBRARY(protobuf)
  CFLAGS="-Wall -protobuf $CFLAGS"
  LDFLAGS="$LDFLAGS -lprotobuf"

  PHP_ADD_LIBRARY(grpc)
  CFLAGS="-Wall -grpc $CFLAGS"
  LDFLAGS="$LDFLAGS -lgrpc"

  PHP_ADD_LIBRARY(grpc++)
  CFLAGS="-Wall -grpc++ $CFLAGS"
  LDFLAGS="$LDFLAGS -lgrpc++"

  PHP_ADD_LIBRARY(dl,,OPENTELEMETRY_SHARED_LIBADD)
  PHP_ADD_LIBRARY(dl)

  case $host in
    *darwin*)
      PHP_ADD_LIBRARY(c++,1,OPENTELEMETRY_SHARED_LIBADD)
      ;;
    *)
      PHP_ADD_LIBRARY(rt,,OPENTELEMETRY_SHARED_LIBADD)
      PHP_ADD_LIBRARY(rt)
      ;;
  esac

  PHP_SUBST(OPENTELEMETRY_SHARED_LIBADD)

  PHP_ADD_INCLUDE(src)
  PHP_ADD_INCLUDE(include)
  PHP_ADD_INCLUDE(third_party)
  PHP_ADD_INCLUDE(third_party/robin-map/include)
  PHP_ADD_INCLUDE(opentelemetry/proto/collector/trace/v1)
  PHP_ADD_INCLUDE(opentelemetry/proto/common/v1)
  PHP_ADD_INCLUDE(opentelemetry/proto/resource/v1)
  PHP_ADD_INCLUDE(opentelemetry/proto/trace/v1)

  PHP_ADD_LIBRARY(stdc++, 1, OPENTELEMETRY_SHARED_LIBADD)

  PHP_OPENTELEMETRY_SOURCE_FILES="\
        opentelemetry/proto/common/v1/common.pb.cc
        opentelemetry/proto/resource/v1/resource.pb.cc
        opentelemetry/proto/trace/v1/trace.pb.cc
        opentelemetry/proto/trace/v1/trace_config.pb.cc
        opentelemetry/proto/collector/trace/v1/trace_service.pb.cc
        opentelemetry/proto/collector/trace/v1/trace_service_grpc.pb.cc
        src/hex.cc
        src/utils.cc
        src/provider.cc
        src/otel_exporter.cc
        src/core.cc
        src/sdk.cc
        src/Timer.cc
        src/zend_hook.cc
        src/zend_hook_error.cc
        src/zend_hook_exception.cc
        src/zend_hook_curl.cc
        src/zend_hook_yar.cc
        src/zend_hook_memcached.cc
        src/zend_hook_redis.cc
        src/zend_hook_mysqli.cc
        src/zend_hook_pdo.cc
        src/zend_hook_pdo_statement.cc
        opentelemetry.cc \
        "

  PHP_NEW_EXTENSION(opentelemetry, $PHP_OPENTELEMETRY_SOURCE_FILES , $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1, cxx)
fi

if test -r $phpincludedir/ext/mysqli/mysqli_mysqlnd.h; then
    AC_DEFINE([MYSQLI_USE_MYSQLND], 1, [Whether mysqlnd is enabled])
fi
