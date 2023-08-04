<?php

class Db{
    private static $instance=FALSE;
    private $db;

    private function __construct(){
        $this->db = new PDO("sqlite:c:\inetpub\data\database.sqlite" ,'','',[PDO::ATTR_PERSISTENT => true]);   
        $this->query('PRAGMA journal_mode = wal;', []);
    }

    function __destruct(){
    }

    public static function retrieve(){
        if(!self::$instance)
            self::$instance = new self();

        return self::$instance;
    }

    public function query($query, $vars){
        $stm = $this->db->prepare($query);
        $stm->execute($vars);

        return $stm->fetchAll();
    }

}