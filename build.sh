#!/usr/bin/env bash
rm -rf src/.libs src/*.lo* opentelemetry/proto/collector/trace/v1/.libs opentelemetry/proto/collector/trace/v1/*.lo* opentelemetry/proto/trace/v1/.libs opentelemetry/proto/trace/v1/*.lo*
phpize --clean
phpize && ./configure && make && make install
phpize --clean
rm -rf src/.libs src/*.lo* opentelemetry/proto/collector/trace/v1/.libs opentelemetry/proto/collector/trace/v1/*.lo* opentelemetry/proto/trace/v1/.libs opentelemetry/proto/trace/v1/*.lo*
#cp opentelemetry.ini $PHP_INI_SCAN_DIR"docker-php-ext-opentelemetry.ini"
php -m | grep opentelemetry && php -d enable_dl=On opentelemetry.php -debug=on