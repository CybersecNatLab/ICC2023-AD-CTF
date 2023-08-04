<?php


class Ban extends Route{

    public function __construct(){
        $this->route = "/^\/api\/ban$/";
    }

    function check_auth($args=[]){
        return $this->request->is_logged;
    }

    public function get($args=[]){
        try{
            $this->request->user->ban();
        }catch(ModelNotFoundException $e){
            $this->response->abort(404);
        }
        
    }
}