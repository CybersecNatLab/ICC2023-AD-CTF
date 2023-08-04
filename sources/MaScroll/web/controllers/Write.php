<?php


class Write extends Route
{
    public function __construct()
    {
        $this->route = '/^\/write$/';
    }

    function check_auth($args = [])
    {
        return $this->request->is_logged;
    }

    public function get($args = [])
    {
        return $this->response->render_template('write.html');
    }

    public function post($args = [])
    {
        $data = $this->request->form;
        if(strlen($data['body']) > 10000 || strlen($data['macro']) > 1000000 ){
            Flash::flash('File too bing');
            $this->response->redirect('/write');
        };
        try{
            $document = DocumentModel::create($data['filename'], $data['body'], $data['macro'], $this->request->user->getUsername());
        } catch (Exception $e) {
            Flash::flash($e->getMessage());
            $this->response->redirect('/write');
        }
        $this->response->redirect('/view/' . $document->getFilename());
    }
}
