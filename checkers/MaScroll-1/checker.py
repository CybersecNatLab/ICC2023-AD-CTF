# Do not make modification to checklib.py (except for debug), it can be replaced at any time
import checklib
import sys
from utils import *
import random
import typing
import traceback
import functools

SERVICE_NAME = 'MaScroll-1'


if __name__ == '__main__':
    if 'debug' in sys.argv:
        try:
            team_ip = sys.argv[2]
        except IndexError:
            print('python.py checker.py debug <ip>')
            quit()
        flag = gen_flag()
        action = None # No action in debug
    else:
        data = checklib.get_data()
        action = data['action']
        team_ip = f'10.{60+int(data["vulnboxId"])}.{data["teamId"]}.1'
        team_id = data['teamId']
        flag = data['flag']

class ExceptionContext:
    def __init__(self, comment: str, status: checklib.Status = checklib.Status.DOWN):
        self._comment = comment
        self._status = status

    def __call__(self, func: typing.Callable) -> typing.Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except Exception:
                checklib.quit(self._status, comment=self._comment,
                              debug=traceback.format_exc())
        return wrapper

    def __enter__(self) -> None:
        pass

    def __exit__(self, exc_type, exc_value, exc_tb) -> None:
        if exc_value is not None:
            if isinstance(exc_value, SystemExit):
                raise exc_value from None
            debug = ''.join(traceback.format_exception(
                exc_type, exc_value, exc_tb))
            checklib.quit(self._status, comment=self._comment, debug=debug)

@ExceptionContext('SLA check failed', status=checklib.Status.ERROR)
def check_sla():
    username = randStr(16, username_dictionary, 4)
    password = randStr(16, username_dictionary, 4)
    client = Client(team_ip)

    # Check register/login
    with ExceptionContext('Registration failed'):
        client.register(username, password)
        client.login(username, password)

    document_body = randStr(30, random_lenth=15)
    document_macro = randStr(30, random_lenth=8)
    document_filename = randStr(20, username_dictionary ,random_lenth=6) + '.mdx'

    # Check write
    with ExceptionContext('Write failed'):
        resp = client.write(document_filename, document_body, document_macro).text
    
    if document_body not in resp or document_filename not in resp or document_macro not in resp:
        checklib.quit(checklib.Status.DOWN, 'Cannot write document', f'filename: {document_filename}')
    
    # Check list
    with ExceptionContext('Listing failed'):
        resp = client.list_documents().text
    
    if document_filename not in resp:
        checklib.quit(checklib.Status.DOWN, 'Listing not working', f'filename: {document_filename}')

    # check share
    with ExceptionContext('Sharing failed'):
        link = client.share(document_filename)
        link = link.replace(client.base_url + '/shared/', '') 
        parts = link.split('?')
        documentId = parts[0]
        sign = parts[1].replace('sign=','')

    with ExceptionContext('Failed to retrieve public key'):
        pubkey = client.get_pubkey()
    with ExceptionContext('Invalid share link'):
        verify_share_sig(pubkey, sign, documentId)

    with ExceptionContext('Sharing failed'):
        document = client.shared(documentId, sign).text

    if document_body not in document:
        checklib.quit(checklib.Status.DOWN, 'Sharing not working', f'filename: {document_filename}')
    # Check this from a clean session
    with ExceptionContext('Sharing failed'):
        document = Client(team_ip).shared(documentId, sign).text
    if document_body not in document:
        checklib.quit(checklib.Status.DOWN, 'Sharing not working', f'filename: {document_filename}')
        
    with ExceptionContext('View document failed'):
        resp = client.view(document_filename).text

    if document_body not in resp or document_filename not in resp or document_macro not in resp:
        checklib.quit(checklib.Status.DOWN, 'Cannot view document', f'filename: {document_filename}')
    



# Returns the flag id. This will pushed to the infra outside this function
# FLAG STORE 1: returns the username of the user that has the flag
@ExceptionContext('Put flag failed', status=checklib.Status.ERROR)
def put_flag():
    random.seed(flag)
    username = randStr(16, username_dictionary, 4)
    password = randStr(16, username_dictionary, 4)

    client = Client(team_ip)

    with ExceptionContext('Registration not working'):
        client.register(username, password)
    filename = randStr(20, username_dictionary, 4) + '.mdx'

    with ExceptionContext('Write document failed'):
        resp = client.write(filename, flag, '').text
    
    if flag not in resp:
         checklib.quit(checklib.Status.DOWN, 'Cannot write flag')
        
    client.ban()

    return {'username': username}

@ExceptionContext('Get flag failed', status=checklib.Status.ERROR)
def get_flag():
    random.seed(flag)
    username = randStr(16, username_dictionary, 4)
    password = randStr(16, username_dictionary, 4)

    filename = randStr(20, username_dictionary, 4) + '.mdx'
    client = Client(team_ip)

    with ExceptionContext('Login failed'):
        client.login(username, password)

    with ExceptionContext('Cannot retrieve flag'):
        resp = client.view(filename).text

    if flag not in resp:
        checklib.quit(checklib.Status.DOWN, 'Flag not found')
    

if action == checklib.Action.CHECK_SLA.value:
    check_sla()
    checklib.quit(checklib.Status.OK, 'OK')
elif action == checklib.Action.PUT_FLAG.value:
    flag_id = put_flag()
    checklib.post_flag_id(SERVICE_NAME, team_id, flag_id)

    # If OK
    checklib.quit(checklib.Status.OK, 'OK')
elif action == checklib.Action.GET_FLAG.value:

    # Check if the flag still exists, use the flag as the seed for random stuff as for put flag
    get_flag()
    # If OK
    checklib.quit(checklib.Status.OK, 'OK')

elif action is None:
    check_sla()
    user_id = put_flag()
    get_flag()
    print(user_id)
    checklib.quit(checklib.quit(checklib.Status.OK, 'OK'))
