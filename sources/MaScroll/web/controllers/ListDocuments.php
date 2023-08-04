<?php


class ListDocuments extends Route
{
    public function __construct()
    {
        $this->route = '/^\/list$/';
    }

    public function check_auth($args = [])
    {
        return  $this->request->is_logged;
    }

    public function get($args = [])
    {
        $documents = $this->request->user->listDocuments();
        $this->response->render_template('list.html', ['documents' => $documents]);
    }
}
