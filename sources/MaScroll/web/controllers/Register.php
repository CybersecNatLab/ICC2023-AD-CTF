<?php


class Register extends Route
{
    public function __construct()
    {
        $this->route = '/^\/register$/';
    }

    function check_auth($args = [])
    {
        return TRUE;
    }

    public function get($args = [])
    {
        $this->response->render_template('register.html');
    }

    public function post($args = [])
    {
        $data = $this->request->form;

        try {
            UserModel::register($data['username'], $data['password']);
        } catch (Exception $e) {
            Flash::flash($e->getMessage());
            $this->response->redirect('/register');
        }
        $_SESSION['user_id'] =  UserModel::login($data['username'], $data['password'])->getUsername();
        $this->response->redirect('/');
    }
}
