<?php

class Request{
    public String $method;
    public string $server_name;
    public String $path;

    public ?Array $query;
    public ?Array $form;

    public bool $is_json;
    public ?stdClass $json;
    public ?String $raw_body;

    public Array $headers;
    public ?Array $cookies;
    public ?Array $session;

    public $is_logged=FALSE;
    public ?UserModel $user=null;


    public function __construct(){
        session_start();
        $this->method = $_SERVER['REQUEST_METHOD'];
        $this->server_name = $_SERVER['SERVER_NAME'];

        $full_path = 'http://' . $_SERVER['SERVER_NAME'] . $_SERVER['REQUEST_URI']; 
        $parsed_path = parse_url($full_path);

        $this->path = $parsed_path['path'];
        if(isset($parsed_path['query']))
            parse_str($parsed_path['query'], $this->query);

        $this->headers = getallheaders();
        $this->raw_body = file_get_contents('php://input');

        if(isset($this->headers['Content-Type']) && strtolower($this->headers['Content-Type']) === 'application/json'){
            try{
                $this->json = json_decode($this->raw_body);
                $this->is_json = true;
            }
            catch(JsonException $e){
                $this->is_json = false;
            }
        }else{
            $this->is_json = false;
        }

        $this->form = $_POST;
        $this->cookies = $_COOKIE;
        
        $this->session = $_SESSION;
        $this->retrieveUserSession();
    }

    private function retrieveUserSession(){
        $this->is_logged = FALSE;
        $this->user = null;

        if(isset($_SESSION['user_id'])){
            $user_id = $_SESSION['user_id'];
            try{
                $this->user = UserModel::retrieve($user_id);
                $this->is_logged = TRUE;
            }catch(ModelNotFoundException $e){
                $this->user=null;
            }
        }
    }
    
}