<?php


class StaticFiles extends Route{
    public function __construct(){
        $this->route = '/^\/static\/(?\'path\'[a-zA-Z]+)\/(?\'filename\'[a-zA-Z0-9\-_]+\.[a-zA-Z]+)$/';
    }
    function check_auth($args=[]){
        return TRUE;
    }

    public function get($args=[]){
        $path = $args['path'];
        $filename = $args['filename'];

        if(!in_array(strtolower($path), ['img', 'js', 'css', 'font', 'keys'])){
            $this->response->abort(404);
        }

        if(!file_exists('static/' . $path . '/' . $filename))
            $this->response->abort(404);
            
        $file_content = @file_get_contents('static/' . $path . '/' . $filename);
        $file_extension = @explode('.', $filename, 1)[1];

        switch($file_extension){
            case 'js':
                $file_type = 'application/json';
                break;
            case 'css':
                $file_type = 'text/css';
                break;
            default:
                $file_type = 'text/plain';
        }
        $this->response->setHeader('Content-Type', $file_type);
        $this->response->text($file_content);
    }

    
}