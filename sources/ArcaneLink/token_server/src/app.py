from typing import Optional
from datetime import datetime

import jwt
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from redis import Redis

from .config import VULNBOX_IP_EXP, TEAM_IP_EXP, TOKEN_DURATION, JWT_KEY, CHECKER_PASSWORD


app = FastAPI()
REDIS_DB = Redis(host='redis', port=6379, db=0)


def create_jwt(target: int, issue: datetime, expiry: datetime):
	return jwt.encode({
		'aud': str(target),
		'nbf': issue,
		'exp': expiry
	}, JWT_KEY, 'ES256')


def team_id_from_vulnbox_ip(ip: str) -> Optional[int]:
	m = VULNBOX_IP_EXP.match(ip)
	if m is not None:
		return int(m.group(1))


def team_id_from_team_ip(ip: str) -> Optional[int]:
	m = TEAM_IP_EXP.match(ip)
	if m is not None:
		return int(m.group(1))


def team_id_from_team_or_vulnbox_ip(ip: str) -> Optional[int]:
	tid = team_id_from_team_ip(ip)
	if tid is None:
		return team_id_from_vulnbox_ip(ip)
	return tid


@app.get('/')
async def readme():
	return JSONResponse({'detail': 'Nothing to see here... use '
		'/token?target=<target_team_id> to get a token to authenticate with the '
		'service of a team'})


@app.get('/token')
async def token_get(target: int, req: Request):
	source = team_id_from_team_or_vulnbox_ip(req.client.host)
	if source is None:
		return JSONResponse({'detail': 'You are not allowed to use the service'}, status_code=403)

	if not (0 <= target < 255):
		return JSONResponse({'detail': 'Invalid target team ID'}, status_code=400)

	key = f'{source}:{target}'
	token = REDIS_DB.get(key)
	if token is not None:
		return token

	issue  = datetime.utcnow()
	expiry = issue + TOKEN_DURATION
	token  = create_jwt(target, issue, expiry)

	REDIS_DB.set(key, token, ex=int(TOKEN_DURATION.total_seconds()))
	return token


@app.get('/checker_token')
async def arbitrary_token(req: Request):
	password = req.query_params.get('password', None)
	target   = req.query_params.get('target', None)

	# Hide this endpoint behind a secret password as it is essentially a backdoor
	if password != CHECKER_PASSWORD or target is None:
		return JSONResponse({'detail': 'Not found'}, status_code=404)

	issue  = datetime.utcnow()
	expiry = issue + TOKEN_DURATION
	return create_jwt(target, issue, expiry)
