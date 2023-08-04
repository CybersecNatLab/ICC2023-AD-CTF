<?php

class Response{

    public Int $status=200;
    public Array $headers=[];
    public Array $new_cookies=[];
    public String $body = '';
    public $user=null;

    public function setHeader($header_name, $header_value){
        $this->headers[$header_name] = $header_value;
    }

    public function setCookies($cookie_name, $cookie_value){
        $this->new_cookies[$cookie_name] = $cookie_value;
    }

    public function write($text){
        $this->body .= $text;
    }

    public function status($status){
        $this->status = $status;
    }

    public function send_file($filename){
        $this->body .= file_get_contents($filename);
    }

    public function text($text){
        $this->body .= $text;
    }
    public function render_template($template_name, $vars=[]){
        ob_clean(); 
        $vars['user'] = $this->user;
        $view = new Template($template_name, $vars);
        $view->render();
        $this->body .= ob_get_contents();
        ob_clean();

        return $this;
    }

    public function redirect($url, $status=302){
        $this->setHeader('Location', $url);
        $this->status($status);
        
        
        $this->body = '';
        $this->render_response();
        die();
    }   
    public function abort($status){
        ob_end_clean();
        $this->status = $status;
        http_response_code($this->status);
        die();
        return $this;

    }

    public function render_response(){
        ob_end_clean();
        http_response_code($this->status);

        foreach($this->headers as $header_name=>$header_value){
            header("$header_name: $header_value");
        }

        foreach($this->new_cookies as $cookie_name=>$cookie_value){
            setcookie($cookie_name, $cookie_value);
        }

        print($this->body);
    }

    public function json($data){
        $this->body = json_encode($data);
        $this->setHeader('Content-Type', 'Application/json');
    }

    public function debug($var){
        ob_end_clean();
        var_dump($var);
        $this->body .= ob_get_contents();
        ob_end_clean();
    }

    public function setUser($user){
        $this->user=$user;
    }

}