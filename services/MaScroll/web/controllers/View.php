<?php

class View extends Route
{
    public function __construct()
    {
        $REGEX_FILENAME = '.*\.mdx'; 
        $this->route = "/^\/view\/($REGEX_FILENAME)$/";
    }

    function check_auth($args = [])
    {
        return $this->request->is_logged;
    }

    public function get($args = [])
    {
        $filename = $args[1];

        try {
            $document = DocumentModel::retrieveByFilename($filename, $this->request->user->getUsername());
        } catch (ModelNotFoundException $e) {

            $this->response->abort(404);;
        }
        return $this->response->render_template('view.html', ['body' => $document->getBody(), 'filename' => $document->getFilename(), 'macro' => $document->getMacro()]);
    }
}
