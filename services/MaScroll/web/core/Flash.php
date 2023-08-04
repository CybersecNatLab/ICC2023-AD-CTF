<?php

class Flash{
    private static $instance=FALSE;

    private $messages = [];


    private function __construct(){
        if(isset($_SESSION['FLASH_MESSAGES']))
            $this->messages = $_SESSION['FLASH_MESSAGES'];
    }

    function __destruct(){
        $_SESSION['FLASH_MESSAGES'] = $this->messages;
    }

    public function push($msg){
        array_push($this->messages, $msg);
    }

    public function pop(){
        return array_pop($this->messages);
    }

    public function get_messages(){
        return $this->messages;
    }

    public function set_messages($msgs){
        $this->messages = $msgs;
    }

    private static function retrieve(){
        if(!self::$instance)
            self::$instance = new self();

        return self::$instance;
    }

    public static function flash($msg){
        $flash = self::retrieve();
        $flash->push($msg);
    }

    public static function get_flashed_messages(){
        $flash = self::retrieve();
        $msg = $flash->get_messages();
        $flash ->set_messages([]);
        return $msg;
    }

    public static function is_flashing(){
        $flash = self::retrieve();
        return count($flash->get_messages())>0;
    }


}