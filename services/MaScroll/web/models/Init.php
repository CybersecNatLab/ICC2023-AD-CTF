<?php

spl_autoload_register(function ($class_name) {
    $filename = "models/$class_name.php";

    if(file_exists($filename)){
        require_once($filename);
    }
});

require_once 'core/utilities.php';