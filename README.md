# opentelemetry-php-ext

## 简介

opentelemetry php 客户端。php扩展、底层无侵入埋点。无接入成本。尤其追求性能及可用性优化。

## 支持

- [x] Trace
    - [x] CURL
    - [x] PDO
    - [x] Mysqli
    - [x] Yar Client
    - [x] Yar Server
    - [x] Redis Extension
    - [x] Memcache Extension
    - [ ] GRPC Client
    - [ ] Swoole
- [ ] Metric
- [ ] Logging

## SDK（提供了一些必要的sdk、方便主动埋点上报）

[opentelemetry-php](https://github.com/kilingzhang/opentelemetry-php)

## 快速开始

```
git clone https://github.com/kilingzhang/opentelemetry-php-ext.git
```

```
cd opentelemetry-php-ext
```

```
docker run -itd \
--name=otel_container \
--net=host \
--restart=always \
-v "${PWD}/examples/otel-config.yaml":/otel-local-config.yaml  \
 otel/opentelemetry-collector --config otel-local-config.yaml 
```

```
docker run  -itd  \
--name=opentelemetry_php_container \
--net=host \
kilingzhang/opentelemetry-php-fpm-alpine:7.1
```

## 编译镜像

```
./build_docker.sh
```

## 配置

```
[opentelemetry]
extension = opentelemetry.so
; 是否开启扩展 默认 1  
opentelemetry.enable = 1
; cli模式下是否开启 默认 0 
opentelemetry.cli_enable = 1
; 是开启debug模式 默认 0 开启后会在日志目录下生成debug日志
opentelemetry.debug = 1
; 是否收集 exception 默认 1
opentelemetry.enable_exception = 1
; 是否收集 error 默认 1
opentelemetry.enable_error = 1
; 是否收集 curl 默认 1
opentelemetry.enable_curl = 1
; 是否收集 memcached 默认 1
opentelemetry.enable_memcached = 1
; 是否收集 redis 默认 1
opentelemetry.enable_redis = 1
; 是否收集 mysql (pdo、mysqli) 默认 1
opentelemetry.enable_mysql = 1
; 是否收集 yar 默认 1
opentelemetry.enable_yar = 1
; 数据收集后是否上报、主要用于性能测试 忽略就好 默认 1 
opentelemetry.enable_collect = 1
; 抽样比例 1/1 1/2 1/10 默认 1
opentelemetry.sample_ratio_based = 1
; 接口耗时 > 500ms 时强制收集 默认 1000ms
opentelemetry.max_time_consuming = 500
; trace php error 收集等级 E_ALL E_WARNING E_ERROR E_NONE 默认 E_ERROR
opentelemetry.error_level = "E_ALL"
; 服务名称 可以在不同项目下的 php-fpm.conf 下设置 （优先推荐） 也可以通 opentelemetry.service_name_key  读取环境变量自动获取
opentelemetry.service_name = "opentelemetry"
; 如果设置了此key值。 会判断 opentelemetry.service_name 是否等于默认值opentelemetry 。如果为默认值则会从读取 $_SERVER["SINASRV_SERVER_NAME"]  
opentelemetry.service_name_key = "SINASRV_SERVER_NAME"
; 日志目录
opentelemetry.log_path = "/data1/opentelemetry/logs"
; otlp grpc上报地址 
opentelemetry.grpc_endpoint = "127.0.0.1:4317"
; grpc 超时时间
opentelemetry.grpc_timeout_milliseconds = 100
; 限制grpc上报
; max_message_size*max_queue_length*consumer_nums 根据机器配置实际情况设置
opentelemetry.max_message_size = 256000
; 上报堆积队列长度
opentelemetry.max_queue_length = 1024
; 队列消费者线程数 默认 1
opentelemetry.consumer_nums = 10
;  环境 staging production 默认 staging
opentelemetry.environment = "staging"
```

## 压测

```
*************************  结果 stat  ****************************
处理协程数量: 100
请求总数（并发数*请求数 -c * -n）: 100000 总请求时间: 67.084 秒 successNum: 100000 failureNum: 0
*************************  结果 end   ****************************

*************************  结果 stat  ****************************
处理协程数量: 100
请求总数（并发数*请求数 -c * -n）: 100000 总请求时间: 74.834 秒 successNum: 100000 failureNum: 0
*************************  结果 end   ****************************

up -> 10%-15%
```

## [otlp udp receiver](https://github.com/kilingzhang/otlpudpreceiver)

## 感谢

感谢 [SkyAPM-php-sdk](https://github.com/SkyAPM/SkyAPM-php-sdk)

通过 [SkyAPM-php-sdk](https://github.com/SkyAPM/SkyAPM-php-sdk) 的使用、源码阅读、开发，学习了很多关于```C++```、```PHP扩展```的知识，受益匪浅。