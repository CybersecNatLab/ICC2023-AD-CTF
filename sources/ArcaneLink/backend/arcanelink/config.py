import os

from cryptography.hazmat.primitives import serialization

TEAM_ID                = int(os.getenv('TEAM_ID', '-1'))    # only needed in backend
BACKEND_HOST           = os.getenv('BACKEND_HOST', '<unneeded>') # only needed in server
BACKEND_PORT           = int(os.getenv('BACKEND_PORT', '-1'))    # only needed in server
REDIS_HOST             = 'redis'
REDIS_PORT             = 6379
LIBVIRT_DOMAINS        = ['ic3-bookworm-i386', 'ic3-bookworm-arm64']
UID_RANGE              = range(1001, 1128)
MAX_LOGINS_PER_USER    = 1
SESSION_CLEAR_INTERVAL = 30
JWT_PUBKEY             = serialization.load_pem_public_key(b'''\
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEN+yh/NwzaOpSCmmEj39klLJu2r0B
ODbUOjBbyJw/GMKfDs0zSWHYMrC2GnqvLt8grHTMpfj4T7iWAvKh6klfyA==
-----END PUBLIC KEY-----\
''')
