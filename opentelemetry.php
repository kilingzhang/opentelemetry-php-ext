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

$n = 10;
while($n--){
opentelemetry_start_cli_tracer();
echo $n , "\n";
try{
$redis = new Redis();
$redis->connect('127.0.0.1');
$redis->set("opentelemetry","opentelemetry");
$redis->get("opentelemetry");
}catch(Exception $e){
}
opentelemetry_shutdown_cli_tracer();
}

sleep(5);