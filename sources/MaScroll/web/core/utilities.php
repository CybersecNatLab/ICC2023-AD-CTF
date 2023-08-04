<?php
use Elliptic\EC;

function format_uuidv4($data)
{
  assert(strlen($data) == 16);

  $data[6] = chr(ord($data[6]) & 0x0f | 0x40); // set version to 0100
  $data[8] = chr(ord($data[8]) & 0x3f | 0x80); // set bits 6-7 to 10
    
  return vsprintf('%s%s-%s-%s-%s-%s%s%s', str_split(bin2hex($data), 4));
}

function generate_uuid(){
    $data = openssl_random_pseudo_bytes(16);
    return format_uuidv4($data);
}

function verify_signature($data, $signature){
  $pub = get_public_key();
  
  $ec = new EC('p256');

  $key = $ec->keyFromPublic($pub);
  
  $r = substr($signature,0,64);
  $s = substr($signature, 64);
  $data_hash = hash('sha256', $data);

  return $key->verify($data_hash, ['r'=>$r,'s'=>$s]);
  
}

function sign($data){
  $ec = new EC('p256');

  $priv = get_private_key();
  $key = $ec->keyFromPrivate($priv);

  $msg = hash('sha256', $data);
  
  $signature = $key->sign($msg);
  $sign_r = $signature->r->toString(16, 32);
  $sign_s = $signature->s->toString(16, 32);

  return $sign_r . $sign_s;
}

function get_private_key(){
  $key = file_get_contents('c:\inetpub\keys\private.pem');
  $key = str_replace("-----BEGIN EC PRIVATE KEY-----", '', $key);
  $key = str_replace("-----END EC PRIVATE KEY-----", '', $key);
  $key = base64_decode($key);
  $private = substr($key, 7, 32);

  return bin2hex($private);
}

function get_public_key(){
  $key = file_get_contents('c:\inetpub\keys\public.pem');
  $key = str_replace("-----BEGIN PUBLIC KEY-----", '', $key);
  $key = str_replace("-----END PUBLIC KEY-----", '', $key);
  $key = base64_decode($key);
  $public = substr($key, 26, 66);

  return $public;
}