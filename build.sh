#!/usr/bin/env bash
find opentelemetry -name "*.lo*" | while read id; do rm -rf $id; done
find opentelemetry -name ".libs" | while read id; do rm -rf $id; done
find src -name "*.lo*" | while read id; do rm -rf $id; done
find src -name ".libs" | while read id; do rm -rf $id; done
#开发环境 protoc --version libprotoc 3.15.8
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/logs/v1/logs.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/metrics/experimental/metrics_config_service.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/metrics/v1/metrics.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/resource/v1/resource.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/common/v1/common.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/trace/v1/trace.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/trace/v1/trace_config.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/collector/logs/v1/logs_service.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/collector/metrics/v1/metrics_service.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) opentelemetry/proto/collector/trace/v1/trace_service.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) --plugin=protoc-gen-grpc-cpp=$(which grpc_cpp_plugin) --grpc-cpp_out=$(pwd) opentelemetry/proto/collector/trace/v1/trace_service.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) --plugin=protoc-gen-grpc-cpp=$(which grpc_cpp_plugin) --grpc-cpp_out=$(pwd) opentelemetry/proto/collector/metrics/v1/metrics_service.proto
protoc --proto_path=$(pwd)/third_party/opentelemetry-proto --cpp_out=$(pwd) --plugin=protoc-gen-grpc-cpp=$(which grpc_cpp_plugin) --grpc-cpp_out=$(pwd) opentelemetry/proto/collector/logs/v1/logs_service.proto
find opentelemetry -name "*.grpc.pb.cc" | while read id; do mv $id ${id/.grpc/_grpc}; done
phpize --clean
phpize && ./configure && make && make install
phpize --clean
find opentelemetry -name "*.lo*" | while read id; do rm -rf $id; done
find opentelemetry -name ".libs" | while read id; do rm -rf $id; done
find src -name "*.lo*" | while read id; do rm -rf $id; done
find src -name ".libs" | while read id; do rm -rf $id; done
if [ -d $PHP_INI_DIR"/php.d/" ]; then
  cp opentelemetry.ini $PHP_INI_DIR"/php.d/docker-php-ext-opentelemetry.ini"
fi
if [ -d $PHP_INI_DIR"/conf.d/" ]; then
  cp opentelemetry.ini $PHP_INI_DIR"/conf.d/docker-php-ext-opentelemetry.ini"
fi
php -m | grep opentelemetry
php -d enable_dl=On opentelemetry.php -debug=on
