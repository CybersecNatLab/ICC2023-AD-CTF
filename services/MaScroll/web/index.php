<?php
ini_set('display_errors', '1');
ini_set('display_startup_errors', '1');
error_reporting(E_ALL);

require __DIR__ . '/vendor/autoload.php';
include 'core/Init.php';
include 'controllers/Init.php';
include 'models/Init.php';


$app = new Application();

$app->register_route(new Register());
$app->register_route(new Login());
$app->register_route(new RunMacro());
$app->register_route(new ListDocuments());
$app->register_route(new StaticFiles());
$app->register_route(new Logout());
$app->register_route(new View());
$app->register_route(new Shared());
$app->register_route(new Share());
$app->register_route(new Write());
$app->register_route(new CheckOwner());
$app->register_route(new Ban());
$app->register_route(new Index());

$app->start();
