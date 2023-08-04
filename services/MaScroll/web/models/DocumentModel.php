<?php

class DocumentModel
{
    private String $documentId;
    private String $filename;
    private ?String $body;
    private ?String $macro = null;
    private String $owner;

    private function __construct($documentId, $owner, $macro = null)
    {
        $this->documentId = $documentId;
        $this->macro = $macro;
        $this->owner = $owner;
        $this->body = null;
    }

    public static function retrieveById($documentId)
    {
        
        $query = 'SELECT * FROM documents WHERE id = ?';
        $db = Db::retrieve();
        $data = $db->query($query, [$documentId]);

        if (empty($data)) {
            throw new ModelNotFoundException('Document not found');
        }

        return self::hidrate($data[0]);
    }

    public static function retrieveByFilename($filename, $owner)
    {
        $filename = strtolower($filename);
        $query = 'SELECT * FROM documents WHERE filename = ? AND owner = ?';
        $db = Db::retrieve();
        $data = $db->query($query, [$filename, $owner]);

        if (empty($data)) {
            throw new ModelNotFoundException('Document not found');
        }

        return self::hidrate($data[0]);
    }


    public static function hidrate($data)
    {
        $document = new self($data['id'], $data['owner'], $data['macro']);
        $document->setFilename($data['filename']);
        return $document;
    }

    public static function create($filename, $body, $macro, $owner)
    {
        $data['id'] = generate_uuid();
        $data['macro'] = $macro;
        $data['filename'] = $filename;
        $data['owner'] = $owner;

        $document = self::hidrate($data);
        $document->setBody($body);
        $document->save();
        return $document;
    }

    public function getBody()
    {
        if (is_null($this->body)) {
            $path = 'c:\\inetpub\\data\\documents\\';
            $path .= $this->owner . '\\';
            $path .= $this->filename;

            $this->body = file_get_contents($path);
        }
        return $this->body;
    }

    public function setBody($body)
    {
        $this->body = $body;
    }

    public function setFilename($filename)
    {
        $filename = basename($filename);
        $filename = strtolower($filename);
        $len = strlen($filename);

        if ($len > 40 || $len < 8 || !str_ends_with($filename, '.mdx') || preg_match("/[\/\\\\]/", $filename) || !ctype_print($filename))
            throw new Exception('Invalid filename');
        $this->filename = $filename;
    }

    public function getFilename()
    {
        return $this->filename;
    }

    public function getDocumentId()
    {
        return $this->documentId;
    }

    public function getMacro()
    {
        return $this->macro;
    }

    public function setMacro($macro)
    {
        $this->macro = $macro;
    }

    public function getOwner()
    {
        return $this->owner;
    }


    public function save()
    {
        try {
            self::retrieveByFilename($this->filename, $this->owner);
            throw new Exception('Please, choose another name for the document');
        } catch (ModelNotFoundException $e) {
        }
        $query = 'INSERT INTO documents VALUES (?,?,?,?)';
        $db = Db::retrieve();
        $db->query($query, [$this->documentId, $this->filename, $this->macro, $this->owner]);
        $path = '..\data\documents\\';
        $path .= $this->owner . '\\';
        $path .= $this->filename;

        try{
            $user = UserModel::retrieve($this->owner);
        }catch (ModelNotFoundException $e) {
            throw new Exception('Something went wrong');
        }

        if($user->isBanned()){
            throw new Exception('User is banned');
        }

        if (file_exists($path)) {
            print($path);
            throw new Exception('Please, choose another name for the document');
        }
        file_put_contents($path, $this->body);
    }
}
