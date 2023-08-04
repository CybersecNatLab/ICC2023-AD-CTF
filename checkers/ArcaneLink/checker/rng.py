from random import Random
from string import ascii_letters, digits

class RNG(Random):
	def random_msg(self) -> bytes:
		return self.randbytes(self.randint(16, 65)).hex().encode()

	def random_msgid(self) -> int:
		return self.randrange(1, 1 << 31)

	def random_key(self) -> int:
		return self.randrange(0, 1 << 64)

	def random_filename(self, length=8, suffix='') -> str:
		return ''.join(self.choices(ascii_letters + digits, k=length)) + suffix

	def chance(self, n: int, d: int) -> bool:
		'''Return True with probability n/d'''
		return self.randrange(0, d) < n
