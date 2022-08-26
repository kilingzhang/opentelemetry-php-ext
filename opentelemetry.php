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
    $redis->connect('127.0.0.1',22036);
    $redis->set("opentelemetry","opentelemetry", 300);
    $redis->get("opentelemetry");
    unset($redis);
    $mc = new Memcached();
    $mc->addServers(array(
        array('127.0.0.1',15013),
        array('127.0.0.1',15013),
    ));
    $mc->set("opentelemetry","opentelemetry",time() + 300);
    $mc->set("opentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetryopentelemetry","opentelemetry",time() + 300);
    $mc->get("opentelemetry");
    var_dump($mc->getServerByKey("opentelemetry"));
    unset($mc);
    $ch = curl_init("https://api.github.com/repos");
    curl_exec($ch);
}catch(Throwable $e){
    echo $e->getMessage() , "\n";
}
echo opentelemetry_get_traceparent() , "\n";
echo 'ing memory : ', memory_get_usage() / 1024 / 1024, 'M', "\n";
opentelemetry_shutdown_cli_tracer();
echo 'after memory : ', memory_get_usage() / 1024 / 1024, 'M', "\n\n";
}

sleep(5);