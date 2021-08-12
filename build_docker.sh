#!/usr/bin/env bash
cp docker/Dockerfile Dockerfile
docker build -t kilingzhang/opentelemetry-php-fpm-alpine:7.1 .
docker push kilingzhang/opentelemetry-php-fpm-alpine:7.1
rm -rf Dockerfile
