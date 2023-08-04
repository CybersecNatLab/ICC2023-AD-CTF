from enum import Enum


class ReqCommand(Enum):
	PEEK_MSG     = 0x00
	PUSH_MSG     = 0x01
	POP_MSG      = 0x02
	GEN_KEY      = 0x03
	CHK_KEY      = 0x04
	WRAPPER_EXIT = 0xffffffff

	def to_bytes(self) -> bytes:
		return self.value.to_bytes(4, 'big')


class ReqError(Enum):
	INVALID_UID = (0x03, 'Invalid uid')
	INVALID_KEY = (0x04, 'Invalid key')
	MALLOC      = (0x05, 'Device memory error')
	MSGSND      = (0x06, 'Device send error')
	MSGRCV      = (0x07, 'Device receive error')
	UNK_CMD     = (0x08, 'Unknown CMD')
	NOMSG       = (0x09, 'No such message')
	UNKNOWN     = (-1  , 'Unknown error code')
	OK          = (-2  , 'OK')
	BAD_VALUE   = (-3  , None) # only for playground and for internal use


	def __init__(self, code: int, description: str):
		self.code        = code
		self.description = description


	def __str__(self) -> str:
		return self.description


	@staticmethod
	def from_cli(desc: bytes) -> 'ReqError':
		'''Translate error codes from human readable strings returned by
		arcane-cli to ReqError instances. In case of error, arcane-cli prints
		human readable strings of the form "ERROR: Short description".
		'''
		if desc.startswith(b'ERROR:'):
			desc = desc[6:]

		desc = desc.strip().decode()
		for e in ReqError:
			if e.description == desc:
				return e

		return ReqError.UNKNOWN


	@staticmethod
	def from_playground(desc: bytes) -> 'ReqError':
		'''Translate error codes from human readable strings returned by the AES
		playground to ReqError instances. In case of error, the playground
		prints human readable strings of the form "ERROR: 0xXXX".
		'''
		if desc.startswith(b'ERROR:'):
			desc = desc[6:]

		if desc == b'OK':
			return ReqError.OK

		try:
			code = int(desc.strip().decode(), 0)
		except ValueError:
			return ReqError.BAD_VALUE

		for e in ReqError:
			if e.code == code:
				return e

		return ReqError.UNKNOWN
