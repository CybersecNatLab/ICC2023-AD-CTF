<?php


class Logout extends Route{
    public function __construct(){
        $this->route = '/^\/logout$/';
    }

    function check_auth($args=[]){
        return TRUE;
    }
    
    public function get($args=[]){
        session_destroy();
        session_start();
        Flash::flash('Logged out');
        $this->response->redirect('/');
    }  
}