<?php


class Shared extends Route
{
    public function __construct()
    {
        $REGEX_UUID = '[0-9a-fA-F]{8}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{12}';
        $this->route = "/^\/shared\/($REGEX_UUID)$/";
    }

    function check_auth($args = [])
    {

        $documentId = $args[1];
        if (isset($this->request->query['sign']))
            $sign = $this->request->query['sign'];
        else
            return FALSE;

        return verify_signature($documentId, $sign);
    }

    public function get($args = [])
    {

        try {
            $document = DocumentModel::retrieveById($args[1]);
        } catch (ModelNotFoundException $e) {
            $this->response->abort(404);
        }

        $this->response->render_template('shared.html', ['document' => $document]);
    }
}
