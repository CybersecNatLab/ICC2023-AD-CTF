import os
import re
from datetime import timedelta

from cryptography.hazmat.primitives import serialization


# Vulnbox IPs are 10.<60 + vulnbox_id>.<team_id>.1, assume less than 10
# services and just use 6\d for the second octect.
VULNBOX_IP_EXP   = re.compile(fr'^10\.6\d.(\d+).1$')
TEAM_IP_EXP      = re.compile(r'^10\.8[01]\.(\d+)\.\d+$')
TOKEN_DURATION   = timedelta(minutes=12) # 6 ticks, needs to outlive flags (5 ticks)
CHECKER_PASSWORD = '3aef232c5daeec55a5eaa61abcfab6c4d58e91c9b16ea61e90cf22abead63f09'
JWT_KEY          = serialization.load_pem_private_key(b'''\
-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIIvctiLaN2k/IyZEVWelBgY4+C1i28szyNaLXKfS5HtwoAoGCCqGSM49
AwEHoUQDQgAEN+yh/NwzaOpSCmmEj39klLJu2r0BODbUOjBbyJw/GMKfDs0zSWHY
MrC2GnqvLt8grHTMpfj4T7iWAvKh6klfyA==
-----END EC PRIVATE KEY-----\
''', None)
