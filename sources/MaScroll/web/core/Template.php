<?php 

class Template {
    protected $template_filename;
    protected $vars;

    function __construct( $filename, $vars ) {
        $this->template_filename = 'templates/' . $filename;
        $this->vars = $vars;
    }
    
    function escape( $str ) {
        return htmlspecialchars( $str );
    }

    function __get( $name ) {
        if( isset( $this->vars[$name] ) ) {
            return $this->vars[$name];
        }
        return null;
    }
    
    function __set( $name, $value ) {
        $this->vars[$name] = $value;
    }

    function __isset($name)
    {
        return isset($this->vars[$name]);
    }
    
    function render( ) {
        
        include( $this->template_filename );
        
    }
}