FROM php:7.1-fpm-alpine

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/usr/local/lib64
ENV LD_RUN_PATH=$LD_RUN_PATH:/usr/local/lib:/usr/local/lib64

RUN set -eux; \
    apk --update add --no-cache tzdata procps bash boost-dev git ca-certificates autoconf automake libtool g++ make cmake file linux-headers file re2c pkgconf openssl openssl-dev curl-dev; \
    cp /usr/share/zoneinfo/Asia/Shanghai /etc/localtime; \
    echo "Asia/Shanghai" > /etc/timezone; \
    apk del tzdata; \
    rm -f /var/cache/apk/*; \
    echo "ulimit -c unlimited " >> ~/.bashrc; \
    echo "alias ll='ls -lha'" >> ~/.bashrc; \
    source ~/.bashrc;

RUN set -eux; \
    git clone --recurse-submodules --recursive -b v1.38.x https://github.com/grpc/grpc.git  /var/grpc ; \
    cd /var/grpc && sed -e 's/add_method/protobuf_add_method/g' -i.bak third_party/protobuf/php/ext/google/protobuf/php-upb.h third_party/protobuf/ruby/ext/google/protobuf_c/ruby-upb.h third_party/protobuf/src/google/protobuf/api.pb.cc third_party/protobuf/src/google/protobuf/api.pb.h third_party/protobuf/src/google/protobuf/compiler/parser.cc third_party/protobuf/src/google/protobuf/descriptor.cc third_party/protobuf/src/google/protobuf/descriptor.pb.cc third_party/protobuf/src/google/protobuf/descriptor.pb.h third_party/protobuf/src/google/protobuf/descriptor_unittest.cc ; \
    mkdir -p cmake/build && cd cmake/build && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE -DBUILD_SHARED_LIBS=ON -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF ../.. ; \
    make -j$(nproc) && make install ; \
    mkdir -p /var/grpc/third_party/abseil-cpp/cmake/build && cd /var/grpc/third_party/abseil-cpp/cmake/build && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE -DBUILD_SHARED_LIBS=ON  ../.. ; \
    make -j$(nproc) && make install ;


# would be kept in the final layer
RUN apk add \
        # memcached
        zlib \
        libmemcached \
        libevent \
        # pdo_pgsql
        libpq \
        #kafka
        librdkafka \
        # mysql
        mysql-client \
        mysql-dev \
        redis \
    ;

RUN pecl update-channels;

RUN set -eux; \
# would be removed later
    apk add --virtual .sinasrv-build-deps \
        $PHPIZE_DEPS \
        # memcached
        libmemcached-dev \
        libevent-dev \
        # pdo_pgsql
        postgresql-dev \
        # swoole
        openssl-dev \
        # yar
        curl-dev \
        # db4
        db-dev \
        # rdkafka
        librdkafka-dev \
        # pdo_mysql
        mysql \
    ;
# prevent docker-php-ext-* from apk get $PHPIZE_DEPS .
# unset would only affect this RUN.
RUN set -eux; \
    unset PHPIZE_DEPS; \
    peclDir="$(pecl config-get temp_dir)"; \
    # redis
    pecl install --onlyreqdeps --nobuild redis-3.1.3; \
    cd "${peclDir}/redis"; \
    phpize; \
    ./configure ; \
    make && make install; \
    # memcached
    pecl install --onlyreqdeps --nobuild memcached; \
    cd "${peclDir}/memcached"; \
    phpize; \
    ./configure ; \
    make && make install; \
    # yar
    pecl install --onlyreqdeps --nobuild yar-2.0.4; \
    cd "${peclDir}/yar"; \
    phpize; \
    ./configure ; \
    make && make install; \
    # swoole
    pecl install --onlyreqdeps --nobuild swoole-4.4.26; \
    cd "${peclDir}/swoole"; \
    phpize; \
    ./configure;\
    make && make install;

RUN set -eux; \
    # configure pdo_mysql pdo_pgsql
    docker-php-ext-configure pdo_mysql --with-pdo-mysql=/usr --with-zlib-dir=/usr; \
    docker-php-ext-configure pdo_pgsql --with-pdo-pgsql=/usr; \
    docker-php-ext-install \
        pdo_mysql \
        pdo_pgsql \
    ; \
    cd /; \
    rm -rf /tmp/pear ~/.pearrc; \
    apk del --no-network .sinasrv-build-deps; \
    rm -f /var/cache/apk/*;

RUN set -eux; \
    echo "extension=redis.so" >> /usr/local/etc/php/conf.d/docker-php-ext-redis.ini; \
    echo "extension=memcached.so" >> /usr/local/etc/php/conf.d/docker-php-ext-memcached.ini; \
    echo "extension=yar.so" >> /usr/local/etc/php/conf.d/docker-php-ext-yar.ini; \
    echo "extension=swoole.so" >> /usr/local/etc/php/conf.d/docker-php-ext-swoole.ini;

ADD . /var/opentelemetry-php-ext
RUN set -eux; \
    cd /var/opentelemetry-php-ext && git submodule update --init --recursive ; \
    ./build.sh ;
