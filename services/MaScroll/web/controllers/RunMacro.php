<?php


class RunMacro extends Route
{
    public function __construct()
    {
        $this->route = '/^\/api\/run$/';
    }

    function check_auth($args = [])
    {
        return $this->request->is_logged;
    }

    public function post($args = [])
    {
        if ($this->request->is_json && isset($this->request->json->filename)) {
            $data = $this->request->json;
        } else {
            $this->response->abort(400);
        }
        try {
            $document = DocumentModel::retrieveByFilename($data->filename, $this->request->user->getUsername());
        } catch (Exception $e) {
            Flash::flash($e->getMessage());
            $this->response->redirect('/');
        }

        $body = bin2hex($this->request->user->getUsername()) . ';';
        $body .= bin2hex($document->getMacro()) . ';';
        $body .= bin2hex($document->getBody());

        $ch = curl_init();

        curl_setopt($ch, CURLOPT_URL, "http://127.0.0.1/api/macro");
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
        curl_setopt($ch, CURLOPT_POST,           1);
        curl_setopt($ch, CURLOPT_POSTFIELDS,     $body);
        curl_setopt($ch, CURLOPT_HTTPHEADER,     array('Content-Type: text/plain'));

        $result = curl_exec($ch);
        $this->response->text($result);
    }
}
