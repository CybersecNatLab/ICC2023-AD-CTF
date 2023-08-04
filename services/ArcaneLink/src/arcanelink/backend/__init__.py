import os
import asyncio
from typing import Set
from datetime import datetime

from fastapi import FastAPI
from fastapi.responses import JSONResponse

from .setup import init_vms
from .user import get_password
from .db import db_flush
from .session import get_or_create_session, destroy_expired_sessions
from .session import destroy_all_sessions, get_session_count
from ..config import LIBVIRT_DOMAINS, SESSION_CLEAR_INTERVAL, TEAM_ID
from ..utils import eprint, asyncio_set_interval, decode_jwt
from ..virsh import domain_start_and_wait_all


app = FastAPI()
bg_tasks: Set[asyncio.Task] = set()


@app.on_event('startup')
async def init():
	await domain_start_and_wait_all(LIBVIRT_DOMAINS)

	if os.getenv('RESET_SERVICE') == '1':
		eprint('Resetting service')
		await destroy_all_sessions()
		db_flush()
	else:
		await destroy_expired_sessions(log=True)
		n = get_session_count()
		eprint('Restored', n, 'existing session' + 's'[:n^1])

	await init_vms()

	bg_tasks.add(asyncio.ensure_future(asyncio_set_interval(destroy_expired_sessions, SESSION_CLEAR_INTERVAL)))
	eprint('Up and running!')


@app.get('/{raw_token}')
async def handle_connection(raw_token: str):
	token = decode_jwt(raw_token, TEAM_ID)
	if token is None:
		return JSONResponse({'detail': 'Invalid or expired token'}, status_code=400)

	session = await get_or_create_session(raw_token, datetime.utcfromtimestamp(token['exp']))
	if session is None:
		eprint('Oh no, we ran out of UIDs!')
		return JSONResponse({'detail': 'Too busy'}, status_code=503)

	return {
		'vms'      : LIBVIRT_DOMAINS,
		'username' : f'user{session.uid}',
		'password' : get_password(session.uid),
		'time_left': (session.expiry - datetime.utcnow()).total_seconds(),
	}
