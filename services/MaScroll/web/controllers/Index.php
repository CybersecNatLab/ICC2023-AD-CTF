<?php


class Index extends Route{

    public function __construct(){
        $this->route = '/^\/(:?index)?$/';
    }

    function check_auth($args=[]){
        return TRUE;
    }

    public function get($args=[]){
        
        $this->response->render_template('index.html');
    }
}