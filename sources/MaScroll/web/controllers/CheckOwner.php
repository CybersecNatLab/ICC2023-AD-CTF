<?php


class CheckOwner extends Route{

    public function __construct(){
        $REGEX_UUID = '[0-9a-fA-F]{8}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{12}';
        $this->route = "/^\/api\/checkowner\/($REGEX_UUID)\/([^\.\\\\\/]+)$/";
    }

    function check_auth($args=[]){
        return TRUE;
    }

    public function get($args=[]){
        try{
            $document = DocumentModel::retrieveById($args[1]);
        }catch(ModelNotFoundException $e){
            $this->response->abort(404);
        }
        if($document->getOwner() !== $args[2]){
            $this->response->abort(403);
        }
    }
}