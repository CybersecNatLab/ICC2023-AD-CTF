#!/usr/bin/env python3

import os
from functools import wraps
from typing import Callable, Any

from pwn import tube
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.serialization import load_pem_public_key, load_pem_parameters, Encoding, PublicFormat
from cryptography.hazmat.primitives.ciphers import Cipher, CipherContext, algorithms, modes

from .config import DH_PARAMS


def wrap_send(ctx: CipherContext) -> Callable[[Callable[[bytes],None]],Callable[[bytes],None]]:
	def decorator(send_raw: Callable) -> Callable[[bytes],None]:
		@wraps(send_raw)
		def wrapper(data: bytes):
			return send_raw(ctx.update(data))
		return wrapper
	return decorator


def wrap_recv(ctx: CipherContext) -> Callable[[Callable[[int],bytes]],Callable[[int],bytes]]:
	def decorator(recv_raw: Callable) -> Callable[[int],bytes]:
		@wraps(recv_raw)
		def wrapper(n: int):
			data = recv_raw(n)
			return ctx.update(data) if data else b''
		return wrapper
	return decorator


def do_not_unrecv(data: Any):
	raise RuntimeError('Tried to unrecv data in the AES stream: ' + repr(data))


def dh_wrap(r: tube, peer_key_pem: bytes):
	dhparams = load_pem_parameters(DH_PARAMS)

	r.clean()
	peer_key = load_pem_public_key(peer_key_pem)
	my_key = dhparams.generate_private_key()
	my_pubkey_pem = my_key.public_key().public_bytes(Encoding.PEM, PublicFormat.SubjectPublicKeyInfo)
	r.write(my_pubkey_pem)

	secret = my_key.exchange(peer_key)
	aes_key = HKDF(SHA256(), 0x20, b'ajeje', b'brazorf').derive(secret)

	peer_iv = r.recvn(0x10)
	my_iv = os.urandom(0x10)
	r.write(my_iv)

	encryptor = Cipher(algorithms.AES256(aes_key), modes.CTR(my_iv)).encryptor()
	decryptor = Cipher(algorithms.AES256(aes_key), modes.CTR(peer_iv)).decryptor()
	r._original_send_raw = r.send_raw
	r._original_recv_raw = r.recv_raw
	r._original_unrecv   = r.unrecv
	r.send_raw = wrap_send(encryptor)(r.send_raw)
	r.recv_raw = wrap_recv(decryptor)(r.recv_raw)
	r.unrecv   = do_not_unrecv


def unwrap(r: tube):
	r.send_raw = r._original_send_raw
	r.recv_raw = r._original_recv_raw
	r.unrecv   = r._original_unrecv
	del r._original_send_raw
	del r._original_recv_raw
	del r._original_unrecv
