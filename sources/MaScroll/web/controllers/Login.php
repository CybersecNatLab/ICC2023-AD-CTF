<?php


class Login extends Route
{
    public function __construct()
    {
        $this->route = '/^\/login$/';
    }
    function check_auth($args = [])
    {
        return TRUE;
    }
    public function get($args = [])
    {
        $this->response->render_template('login.html');
    }

    public function post($args = [])
    {
        $data = $this->request->form;

        $user = UserModel::login($data['username'], $data['password']);
        if (!$user) {
            Flash::flash('Wrong username or password');
            $this->response->redirect('/login');
        }

        $_SESSION['user_id'] = $user->getUsername();
        $this->response->redirect('/');
    }
}
