<?php

abstract class Route{

    protected ?Route $next=null;
    protected $route;
    protected $interceptor;

    protected Request $request;
    protected Response $response;

    abstract function __construct();

    final public function _set_next($next){
        if(is_null($this->next))
            $this->next = $next;
        else
            $this->next->_set_next($next);
    
    }

    final public function _call_next(){
        if($this->next){
            return $this->next->call($this->request, $this->response);
        };

        throw new NotFoundException();  
    }

    final public function call($request, $response){
        
        $this->request = $request;
        $this->response = $response;

       
        $path = $this->request->path;
        $matches = [];

        if(!preg_match($this->route, $path, $matches)){
            return $this->_call_next();
        }

        $method = $this->request->method;
        $method = strtolower($method);

        if(!$this->check_auth($matches)){
            Flash::flash('You need to an account to to do this!');
            $this->response->redirect('/');
        }

        if(in_array($method, ['get', 'post', 'head', 'put', 'delete', 'options'])){
            return $this->$method($matches);
        }
        else{
            return $this->response->status(400);
        }

    }

    abstract function check_auth($args=[]);

    protected function get($args=[]){
        return $this->response->status(405);
    }

    protected function post($args=[]){
        return $this->response->status(405);
    }

    protected function put($args=[]){
        return $this->response->status(405);
    }

    protected function delete($args=[]){
        return $this->response->status(405);
    }

    protected function head($args=[]){
        return $this->response->status(200);
    }

    protected function options($args=[]){
        return $this->response->status(400);
    }

}