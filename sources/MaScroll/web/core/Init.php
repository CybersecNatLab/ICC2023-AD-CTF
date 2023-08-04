<?php

spl_autoload_register(function ($class_name) {
    if(str_ends_with($class_name, 'Exception'))
        $filename = "core/Exceptions/$class_name.php";
    else
        $filename = "core/$class_name.php";

    if(file_exists($filename)){
        require_once($filename);
    }
});

require_once 'core/utilities.php';