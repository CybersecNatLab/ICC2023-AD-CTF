<?php

class Share extends Route
{
    public function __construct()
    {
        $REGEX_FILENAME = '.*\.mdx';
        $this->route = "/^\/api\/share\/($REGEX_FILENAME)$/";
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
            $this->response->abort(404);
        }
        $documentId = $document->getDocumentId();
        $signature = sign($documentId);
        $url = 'http://' . $this->request->server_name . '/shared/' . $documentId  . '?sign=' . urlencode($signature);

        $this->response->json(['link' => $url]);
    }
}
