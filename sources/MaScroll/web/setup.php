<?php

// if(file_exists('c:\inetpub\data\database.sqlite')){
// die();
// }
require __DIR__ . '/vendor/autoload.php';


$db_struct = "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT, salt TEXT);
CREATE TABLE IF NOT EXISTS documents (id TEXT PRIMARY KEY, filename TEXT, macro TEXT, owner TEXT, FOREIGN KEY (owner) REFERENCES users (username));
CREATE TABLE IF NOT EXISTS banned (username TEXT PRIMARY KEY);";

$db = new PDO("sqlite:c:\inetpub\data\database.sqlite"); 

$queries = explode("\n", $db_struct);
$queries = array_filter($queries);

foreach($queries as $query){
    $stm = $db->prepare($query);
    $stm->execute();
}


