<?php
$br = (php_sapi_name() == "cli") ? "" : "<br>";

if (!extension_loaded('opentelemetry')) {
    dl('opentelemetry.' . PHP_SHLIB_SUFFIX);
}
$module = 'opentelemetry';
$functions = get_extension_funcs($module);
echo "Functions available in the test extension:$br\n";
foreach ($functions as $func) {
    echo $func . "$br\n";
}
echo "$br\n";

$n = 5;
while($n--){
echo 'before memory : ', memory_get_usage() / 1024 / 1024, 'M', "\n";
opentelemetry_start_cli_tracer();
try{
    $redis = new Redis();
    $redis->connect('127.0.0.1');
    $redis->set("opentelemetry","opentelemetry");
    $redis->get("opentelemetry");
    unset($redis);
}catch(Exception $e){
}
echo 'ing memory : ', memory_get_usage() / 1024 / 1024, 'M', "\n";
opentelemetry_shutdown_cli_tracer();
echo 'after memory : ', memory_get_usage() / 1024 / 1024, 'M', "\n\n";
}

sleep(5);