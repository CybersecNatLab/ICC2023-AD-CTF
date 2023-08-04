#!/usr/bin/env python3

import sys
import checklib
import functools
import os
import random
import string
import traceback
import typing

import macro
import utils


SERVICE_NAME = 'MaScroll-2'
CHECKER_SECRET = 'TejbvefzIr4gBUCU'

# Lower 12 bits of ProcessPrng address in bcryptprimitives.dll
PROCESSPRNG_LO12 = 0x340

# Maximum random statements for SLA checks
MAX_RAND_STMTS = 30

# Maximum number of functions to split
MAX_SPLIT_FUNCS = 5


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


class Storage:
    def __init__(self, debug: bool = False):
        self._debug = debug
        if debug:
            self._store: dict[str, str] = {}
        else:
            import redis
            self._db = redis.StrictRedis(
                host=os.environ['REDIS_HOST'],
                port=int(os.environ['REDIS_PORT']),
                db=int(os.environ['REDIS_DB']),
                password=os.environ['REDIS_PASSWORD'],
            )

    def __getitem__(self, key: str) -> str:
        if self._debug:
            return self._store[key]
        value = self._db.get(f'{SERVICE_NAME}:{key}')
        if value is None:
            raise KeyError(key)
        return value.decode()

    def __setitem__(self, key: str, value: str) -> None:
        if self._debug:
            self._store[key] = value
        else:
            self._db.set(f'{SERVICE_NAME}:{key}', value)


def gen_check_macro_random(main_func: macro.GeneratorScope) -> typing.Callable[[bytes], bool]:
    num_checks = random.randint(1, 5)
    skip_vars: set[str] = set()
    prev_output_var = None
    prev_output_node = None
    output = b''
    for _ in range(num_checks):
        main_func.append_random(MAX_RAND_STMTS // num_checks)
        # Pick a random variable with concrete string representation to check.
        avail_vars = [var for var in main_func.vars if var not in skip_vars]
        if not avail_vars:
            continue
        random.shuffle(avail_vars)
        for var in avail_vars:
            value = main_func.vars[var]
            str_node = macro.VarUse(var=macro.VarLoc(name=var), value=value)
            if not isinstance(value, macro.String):
                str_node = macro.BuiltinCallCStr(inputs=(str_node,))
            str_conc = str_node.value.concrete()
            if str_conc is not None:
                skip_vars.add(var)
                break
        else:
            continue
        # Append string representation to a new variable.
        output += str_conc
        output_node = str_node
        if prev_output_var is not None:
            output_node = macro.BinOpCat(inputs=(
                macro.VarUse(var=prev_output_var, value=prev_output_node.value), output_node))
        output_var = main_func.new_var(output_node.value)
        main_func.append_stmt(macro.AssignStmt(var=output_var, inputs=(output_node,)))
        prev_output_var = output_var
        prev_output_node = output_node

    # Return the output.
    if prev_output_var is not None:
        assert prev_output_node is not None
        return_node = macro.VarUse(var=prev_output_var, value=prev_output_node.value)
    else:
        # Just return a random string.
        output = utils.randStr(16, string.ascii_letters + string.digits, 8).encode()
        return_node = macro.Literal(value=macro.String.abstract(output))
    main_func.append_stmt(macro.AssignStmt(var=macro.VarLoc(name='Main'), inputs=(return_node,)))

    return output.__eq__


def gen_check_macro_fastrand(main_func: macro.GeneratorScope) -> typing.Callable[[bytes], bool]:
    use_entropy = random.choice([True, False])

    main_func.append_random(MAX_RAND_STMTS // 6)

    # Create string for random output.
    length = random.randint(128, 256)
    rand_char = random.choice(string.ascii_letters + string.digits).encode()
    str_var = main_func.new_var(value=macro.String())
    main_func.append_stmt(macro.AssignStmt(var=str_var, inputs=(
        macro.BuiltinCallString(inputs=(
            macro.Literal(value=macro.Integer.abstract(length)),
            macro.Literal(value=macro.String.abstract(rand_char)),
        )),
    )))

    main_func.append_random(MAX_RAND_STMTS // 6)

    if use_entropy:
        # Create entropy string.
        entropy_length = random.randint(16, 32)
        entropy_bytes = ''.join(random.choices(string.ascii_letters + string.digits, k=entropy_length)).encode()
        entropy_var = main_func.new_var(value=macro.String())
        main_func.append_stmt(macro.AssignStmt(var=entropy_var, inputs=(
            macro.Literal(value=macro.String.abstract(entropy_bytes)),
        )))

    main_func.append_random(MAX_RAND_STMTS // 6)

    # Call RandSeed.
    seed = random.randint(macro.Integer.INT_MIN, macro.Integer.INT_MAX)
    seed_value = macro.Integer.abstract(seed)
    main_func.append_stmt(macro.FunctionCall(name='RandSeed', inputs=(
        macro.Literal(value=seed_value),
    ), value=seed_value))

    main_func.append_random(MAX_RAND_STMTS // 6)

    # Call FastRand.
    call_inputs = [macro.VarUse(var=str_var, value=macro.String())]
    if use_entropy:
        call_inputs.append(macro.VarUse(var=entropy_var, value=macro.String()))
    main_func.append_stmt(macro.FunctionCall(name='FastRand', inputs=tuple(call_inputs),
                                             value=macro.Integer.abstract(length)))

    main_func.append_random(MAX_RAND_STMTS // 6)

    # Return random string.
    main_func.append_stmt(macro.AssignStmt(var=macro.VarLoc(name='Main'), inputs=(
        macro.VarUse(var=str_var, value=macro.String()),
    )))

    main_func.append_random(MAX_RAND_STMTS // 6)

    # Compute output.
    output_arr = bytearray((seed + 1 + i) & 0xff for i in range(length))
    if use_entropy:
        # Permute output according to entropy.
        for i in range(length):
            j = (i + entropy_bytes[i % entropy_length]) % length
            output_arr[i], output_arr[j] = output_arr[j], output_arr[i]
    output = bytes(output_arr)

    return output.__eq__


def gen_check_macro_leak(main_func: macro.GeneratorScope) -> typing.Callable[[bytes], bool]:
    leak_as_int = random.choice([True, True, False])

    main_func.append_random(MAX_RAND_STMTS // 5)

    # Leak ProcessPrng address.
    describe_node = macro.FunctionCall(name='Describe', inputs=(
        macro.VarUse(var=macro.VarLoc(name='CryptoRand'), value=None),
    ), value=macro.String())
    leak_var = main_func.new_var(describe_node.value)
    main_func.append_stmt(macro.AssignStmt(var=leak_var, inputs=(describe_node,)))

    main_func.append_random(MAX_RAND_STMTS // 5)

    if leak_as_int:
        # Extract hex address.
        len_node = macro.BuiltinCallLen(inputs=(
            macro.VarUse(var=leak_var, value=describe_node.value),
        ))
        # Analysis of Describe is not accurate enough, which will trip checks in Mid.
        len_node.value = macro.Integer(value_min=27)
        hex_node = macro.BuiltinCallMid(inputs=(
            macro.VarUse(var=leak_var, value=describe_node.value),
            macro.Literal(value=macro.Integer.abstract(27)),
            macro.BinOpSub(inputs=(
                len_node,
                macro.Literal(value=macro.Integer.abstract(27)),
            )),
        ))
        hex_var = main_func.new_var(hex_node.value)
        main_func.append_stmt(macro.AssignStmt(var=hex_var, inputs=(hex_node,)))

    main_func.append_random(MAX_RAND_STMTS // 5)

    if leak_as_int:
        # Convert to integer, adding an offset to obfuscate.
        offset = random.randint(-2**48, 2**48)
        if random.choice([True, False]):
            offset_op = macro.BinOpAdd
            additive_offset = offset
        else:
            offset_op = macro.BinOpSub
            additive_offset = -offset
        int_node = offset_op(inputs=(
            macro.BuiltinCallCInt(inputs=(
                macro.VarUse(var=hex_var, value=macro.String()),
            )),
            macro.Literal(value=macro.Integer.abstract(offset)),
        ))
        int_var = main_func.new_var(int_node.value)
        main_func.append_stmt(macro.AssignStmt(var=int_var, inputs=(int_node,)))

    main_func.append_random(MAX_RAND_STMTS // 5)

    # Return leak.
    if leak_as_int:
        output_node = macro.BuiltinCallCStr(inputs=(
            macro.VarUse(var=int_var, value=int_node.value),
        ))
    else:
        output_node = macro.VarUse(var=leak_var, value=describe_node.value)
    main_func.append_stmt(macro.AssignStmt(var=macro.VarLoc(name='Main'), inputs=(output_node,)))

    main_func.append_random(MAX_RAND_STMTS // 5)

    if leak_as_int:
        def check_output(output: bytes) -> bool:
            try:
                value = int(output, 10) - additive_offset
            except ValueError:
                return False
            return value & 0xfff == PROCESSPRNG_LO12
        return check_output
    else:
        return lambda output: \
            output.startswith(b'[RNGFunction CryptoRand @ &H') and \
            output.endswith(f'{PROCESSPRNG_LO12:03x}]'.encode())


@ExceptionContext('SLA check failed', status=checklib.Status.ERROR)
def check_sla(team_ip: str) -> None:
    if 'SLA_CHECK_SEED' in os.environ:
        seed = bytes.fromhex(os.environ['SLA_CHECK_SEED'])
    else:
        seed = random.randbytes(16)
    print(f'SLA check seed: {seed.hex()}', file=sys.stderr)
    random.seed(seed)

    check_gens = [
        gen_check_macro_random,
        gen_check_macro_fastrand,
        gen_check_macro_leak,
    ]
    random.shuffle(check_gens)

    for check_gen in check_gens:
        username = utils.randStr(16, utils.username_dictionary, 4)
        password = utils.randStr(16, utils.username_dictionary, 4)
        filename = utils.randStr(20, utils.username_dictionary, 4) + '.mdx'
        text = utils.randStr(32, string.ascii_letters + string.digits, 8)

        gen = macro.Generator(text.encode())
        check_output = check_gen(gen.funcs['Main'])
        gen.split_functions(MAX_SPLIT_FUNCS)
        code = str(gen)
        # print(code)

        client = utils.Client(team_ip)

        with ExceptionContext('Registration failed'):
            client.register(username, password)
        with ExceptionContext('Failed to write document'):
            client.write(filename, text, code)
        with ExceptionContext('Failed to run macro'):
            resp = client.sess.post(f'{client.base_url}/api/run',json={
                'filename': filename,
            })

        output = resp.content
        if not check_output(output):
            checklib.quit(checklib.Status.DOWN, 'Macro interpreter not working',
                          debug=f'Failed {check_gen.__name__}, output: {output!r}')


@ExceptionContext('Put flag failed', status=checklib.Status.ERROR)
def put_flag(storage: Storage, team_ip: str, flag: str) -> dict[str, str]:
    random.seed(CHECKER_SECRET + flag)
    username = utils.randStr(16, utils.username_dictionary, 4)
    password = utils.randStr(16, utils.username_dictionary, 4)
    filename = utils.randStr(20, utils.username_dictionary, 4) + '.mdx'

    client = utils.Client(team_ip)

    with ExceptionContext('Registration failed'):
        client.register(username, password)

    with ExceptionContext('Failed to write flag'):
        resp = client.write(filename, flag, '')
        if flag not in resp.text:
            checklib.quit(checklib.Status.DOWN, 'Incorrect flag write')

    with ExceptionContext('Failed to share flag'):
        share_link = client.share(filename)
        doc_id = share_link.split('?')[0].split('/')[-1]
        sig = share_link.split('?sign=')[1]

    storage[f'{flag}:doc'] = doc_id
    storage[f'{flag}:sig'] = sig

    return {'document_id': doc_id}


def gen_sign_macro(doc_id: str) -> tuple[str, str]:
    is_text_input = random.choice([True, False])
    if is_text_input:
        text = doc_id
    else:
        text = utils.randStr(32, string.ascii_letters + string.digits, 8)

    gen = macro.Generator(text.encode())
    main_func = gen.funcs['Main']

    main_func.append_random(MAX_RAND_STMTS // 3)

    # Sign document ID.
    if is_text_input:
        sign_input = macro.VarUse(var=macro.VarLoc(
            name='text'), value=macro.String.abstract(text.encode()))
    else:
        sign_input = macro.Literal(macro.String.abstract(doc_id.encode()))
    sign_node = macro.FunctionCall(name='SignToken', inputs=(
        sign_input,
    ), value=macro.String())
    sign_var = main_func.new_var(sign_node.value)
    main_func.append_stmt(macro.AssignStmt(var=sign_var, inputs=(sign_node,)))

    main_func.append_random(MAX_RAND_STMTS // 3)

    # Return signature.
    main_func.append_stmt(macro.AssignStmt(var=macro.VarLoc(name='Main'), inputs=(
        macro.VarUse(var=sign_var, value=sign_node.value),
    )))

    main_func.append_random(MAX_RAND_STMTS // 3)

    gen.split_functions(MAX_SPLIT_FUNCS)
    code = str(gen)

    return text, code


@ExceptionContext('Get flag failed', status=checklib.Status.ERROR)
def get_flag(storage: Storage, team_ip: str, flag: str) -> None:
    random.seed(CHECKER_SECRET + flag)
    username = utils.randStr(16, utils.username_dictionary, 4)
    password = utils.randStr(16, utils.username_dictionary, 4)

    # Reseed from OS random to avoid collisions, as the flag might be checked multiple times.
    seed = os.urandom(16)
    print(f'Get flag seed: {seed.hex()}', file=sys.stderr)
    random.seed(seed)
    sign_filename = utils.randStr(20, utils.username_dictionary, 4) + '.mdx'

    doc_id = storage[f'{flag}:doc']
    sig = storage[f'{flag}:sig']

    idxs = [0, 1]
    random.shuffle(idxs)
    for i in idxs:
        client = utils.Client(team_ip)
        if i == 0:
            check_sig = sig
        elif i == 1:
            with ExceptionContext('Login failed'):
                client.login(username, password)
            text, code = gen_sign_macro(doc_id)
            with ExceptionContext('Failed to write document'):
                client.write(sign_filename, text, code)
            with ExceptionContext('Failed to run macro'):
                resp = client.sess.post(f'{client.base_url}/api/run',json={
                    'filename': sign_filename,
                })
                check_sig = resp.text
            with ExceptionContext('Failed to retrieve public key'):
                pubkey = client.get_pubkey()
            with ExceptionContext('Invalid signature'):
                utils.verify_share_sig(pubkey, check_sig, doc_id)
        with ExceptionContext('Failed to get flag'):
            resp = client.shared(doc_id, check_sig)
            if flag.encode() not in resp.content:
                checklib.quit(checklib.Status.DOWN, 'Flag not found')


def main():
    if len(sys.argv) == 3 and sys.argv[1].lower() == 'debug':
        team_ip, = sys.argv[2:]
        flag = utils.gen_flag()
        action = None
        storage = Storage(debug=True)
    else:
        data = checklib.get_data()
        action = data['action']
        team_ip = f'10.{60+int(data["vulnboxId"])}.{data["teamId"]}.1'
        team_id = data['teamId']
        flag = data['flag']
        with ExceptionContext('Internal error', status=checklib.Status.ERROR):
            storage = Storage()

    if action is None:
        print(f'Flag: {flag}', file=sys.stderr)
        flag_id = put_flag(storage, team_ip, flag)
        print(f'Flag ID: {flag_id}', file=sys.stderr)
        check_sla(team_ip)
        get_flag(storage, team_ip, flag)
    elif action == checklib.Action.CHECK_SLA.value:
        check_sla(team_ip)
    elif action == checklib.Action.PUT_FLAG.value:
        flag_id = put_flag(storage, team_ip, flag)
        checklib.post_flag_id(SERVICE_NAME, team_id, flag_id)
    elif action == checklib.Action.GET_FLAG.value:
        get_flag(storage, team_ip, flag)
    else:
        checklib.quit(checklib.Status.ERROR, 'Internal error',
                      debug=f'Unknown action "{action}"')

    checklib.quit(checklib.Status.OK, 'OK')


if __name__ == '__main__':
    main()
