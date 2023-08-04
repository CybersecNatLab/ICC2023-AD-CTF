from errno import ENOMSG
from ctypes import create_string_buffer
from ctypes import CDLL, POINTER, c_void_p, c_size_t, c_long, c_int, c_ssize_t


IPC_NOWAIT  = 0o4000
MSG_NOERROR	= 0o10000
DUMMY_BUF   = create_string_buffer(1)


libc = CDLL('libc.so.6')
libc_msgget = libc.msgget
libc_msgget.argtypes = [c_int, c_int]
libc_msgget.restype = c_int
libc_msgrcv = libc.msgrcv
libc_msgrcv.argtypes = [c_int, c_void_p, c_size_t, c_long, c_int]
libc_msgrcv.restype = c_ssize_t
libc.__errno_location.restype = POINTER(c_int)


def libc_get_errno() -> int:
	return libc.__errno_location().contents.value


def drain_msgqueue(key: int):
	qid = libc_msgget(key, 0o666)
	if qid == -1:
		raise RuntimeError(f'msgget({key}) failed: errno={libc_get_errno()}')

	while 1:
		res = libc_msgrcv(qid, DUMMY_BUF, 0, 0, IPC_NOWAIT|MSG_NOERROR)
		if res == -1:
			err = libc_get_errno()
			if err == ENOMSG:
				break

			raise RuntimeError(f'msgrcv({key}) failed: errno={err}')
