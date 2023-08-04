<?php

class Application{
    
    private ?Route $routesChain=null;
    private Request $request;
    private Response $response;

    public function __construct()
    {
       ob_start();
    }

    public function register_route($route){
        if($this->routesChain === null)
            $this->routesChain = $route;
        else
            $this->routesChain->_set_next($route);
    }

    public function find_route(){
        $this->request = new Request();
        $this->response = new Response();
        $this->response->setUser($this->request->user);

        if(!$this->routesChain){
            $this->response->abort(500);
        }
        
        try{
            $this->routesChain->call($this->request, $this->response);
        }catch(NotFoundException $e){
            $this->response->abort(404);
        }
        
        $this->render_response();
    }

    public function start(){
        $this->find_route();
    }


    public function render_response(){
        $this->response->render_response();
    }
}