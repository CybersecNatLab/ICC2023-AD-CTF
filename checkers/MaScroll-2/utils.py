import ecdsa
import hashlib
import random
import string
import requests
import checklib

username_dictionary =  string.ascii_lowercase + string.digits + '()\'!_-'
document_dictionary = username_dictionary
flag_dictionary = string.ascii_uppercase + string.digits

def randStr(n=8, d=string.ascii_letters, random_lenth=False):
    if random_lenth and random_lenth < n:
        n += random.randint(random_lenth*-1,random_lenth)
    return ''.join(random.choices(d, k=n))

def gen_flag():
    return randStr(31, flag_dictionary) + '='

class Client:
    
    def __init__(self, ip) -> None:
        self.sess = requests.Session()
        self.sess.headers['User-Agent'] = 'checker'
        self.base_url = f'http://{ip}'

    def register(self, username, password):
        url = self.base_url + '/register'
        data = {
            'username': username,
            'password': password
        }
        resp = self.sess.post(url, data=data)
        if username not in resp.text:
            checklib.quit(checklib.Status.DOWN, 'Cannot register user', f'Username invalid: {username}')
        return resp
        

    def login(self, username, password):
        url = self.base_url + '/login'
        data = {
            'username': username,
            'password': password
        }

        resp = self.sess.post(url, data=data)
        if username not in resp.text:
            checklib.quit(checklib.Status.DOWN, 'Login not working', f'Username invalid: {username}')
        return resp

    def write(self, filename, body, macro):
        url = self.base_url + '/write'
        data = {
            'filename': filename,
            'body': body,
            'macro': macro
        }

        resp = self.sess.post(url, data=data)
        checks_filename = filename not in resp.text 
        checks_body = body not in resp.text
        checks_macro = macro not in resp.text
        if checks_filename or checks_body or checks_macro :
            checklib.quit(checklib.Status.DOWN, 'Cannot write document')
        
        return resp

    def view(self, filename):
        url = self.base_url + f'/view/{filename}'
        resp = self.sess.get(url)
        return resp

    def share(self, filename):
        url = self.base_url + f'/api/share/{filename}'
        resp = self.sess.get(url)
        try:
            resp = resp.json()
            assert len(resp['link']) > 0
            assert '/shared/' in resp['link'] # just to be sure that the link is kinda valid
        except:
            checklib.quit(checklib.Status.DOWN, 'Share not working')
    
        return resp['link']


    def shared(self, uuid, sign):
        url = self.base_url + f'/shared/{uuid}'
        resp = self.sess.get(url, params={'sign': sign})
        return resp
        
    def list_documents(self):
        url = self.base_url + '/list'
        resp = self.sess.get(url)
        return resp
    
    def ban(self):
        url = self.base_url + '/api/ban'
        resp = self.sess.get(url)
        return resp

    def get_pubkey(self) -> ecdsa.VerifyingKey:
        resp = self.sess.get(self.base_url + '/static/keys/public.pem')
        return ecdsa.VerifyingKey.from_pem(resp.text, hashfunc=hashlib.sha256)


def verify_share_sig(pubkey: ecdsa.VerifyingKey, sig_hex: str, doc_id: str) -> None:
    sig = bytes.fromhex(sig_hex)
    pubkey.verify(sig, doc_id.encode())
