<?php

class UserModel
{
    private ?String $username;
    private ?String $password;
    private ?String $salt = null;

    private function __construct($username = null, $password = null, $salt = null)
    {
        $this->username = $username;
        $this->password = $password;
        $this->salt = $salt;
    }

    public static function retrieve($user)
    {
        $query = 'SELECT * FROM users WHERE username = ?';
        $db = Db::retrieve();
        $data = $db->query($query, [$user]);

        if (empty($data)) {
            throw new ModelNotFoundException('User not found');
        }
        $data = $data[0];
        $user = new self($data['username'], $data['password'], $data['salt']);
        return $user;
    }

    public function setPassword($password)
    {
        $this->salt = openssl_random_pseudo_bytes(6);
        $this->password = md5($this->salt . $password);
    }
    public function getUsername()
    {
        return $this->username;
    }

    public function setUsername($username)
    {
        $len = strlen($username);
        if ($len < 8 || $len > 40) {
            throw new Exception("Username too short or too long");
        }
        if (!ctype_print($username) || preg_match('/[\.\/\\\\]/', $username)) {
            throw new Exception("Username contains invalid characters");
        }
        $this->username = $username;
    }


    public function verifyPassword($password)
    {
        if (is_null($this->salt)) {
            throw new Exception("verifyPassword called on non-initialized UserModel");
        }
        $pwd_hash = md5($this->salt . $password);
        return $pwd_hash === $this->password;
    }

    public static function login($username, $password)
    {
        try {
            $user = self::retrieve($username);
        } catch (ModelNotFoundException $e) {
            return FALSE;
        }

        if ($user->verifyPassword($password))
            return $user;

        return FALSE;
    }

    public static function register($username, $password)
    {
        try {
            $user = self::retrieve($username);
            throw new Exception('User already registered!');
        } catch (ModelNotFoundException $e) {
            $user = new self();
        }

        $user->setUsername($username);
        $user->setPassword($password);

        if (!is_dir('c:/inetpub/data/documents/' . $username)) {
            mkdir('c:/inetpub/data/documents/' . $username, 0777, true);
        }

        $user->commit();
        return TRUE;
    }

    public function listDocuments()
    {
        $query = 'SELECT * FROM documents WHERE owner = ?';
        $db = Db::retrieve();
        $documents = $db->query($query, [$this->getUsername()]);
        $to_return = [];
        foreach ($documents as $document) {
            $to_return += [DocumentModel::hidrate($document)];
        }

        return $to_return;
    }

    public function commit()
    {
        $query = 'INSERT INTO users (username, password, salt) VALUES (?,?,?);';
        $db = Db::retrieve();
        $db->query($query, [$this->username, $this->password, $this->salt]);
    }

    public function ban()
    {
        $query = 'INSERT INTO banned (username) VALUES (?);';
        $db = Db::retrieve();
        $db->query($query, [$this->username]);
    }

    public function isBanned(){
        
        $query = 'SELECT * FROM banned WHERE username LIKE ?';
        $db = Db::retrieve();
        return !empty($db->query($query, [$this->username]));

    }
}
