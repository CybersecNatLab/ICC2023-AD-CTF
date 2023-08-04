from pathlib import Path

from pwn import args


TOKEN_SERVER_URL      = 'http://10.10.0.5'
TOKEN_SERVER_PASSWORD = '3aef232c5daeec55a5eaa61abcfab6c4d58e91c9b16ea61e90cf22abead63f09'
SERVICE_NAME          = 'ArcaneLink'
SERVICE_PORT          = 1337
PLAYGROUND_DIR        = Path('playground')
DH_PARAMS             = b'''\
-----BEGIN DH PARAMETERS-----
MIIBCAKCAQEA3noIn2jYmm7yIsXA9hyZMYxUtOC6T3u9t3k3IYkxSvXL1w6NPR1n
cPf8DzNHrkF3FSycJyYuDopWt9cipVPjI7IQLrDwVvLvbefxoAiC/oFBbxG41sjG
/05dH9KHi+oCPInaBJZ8nr5xOi/9djjxGccws4osBOi6Zej1OwotoZuwTbeoYZfi
FmyJElxU4rkVqSMDhDzKrsiRYj6B8OG07QNqbrtSlb4GPbcModvtwb7un/gSRuIz
maAUH0+SgwTmDFtLAyPVC5Xl0qTgfTj1VA/nLEgFd7gWZoQ9KXZF5QxCswu7kSTf
os6D0zfuXQzsHB5N0MdMYY4017NF6Y1EcwIBAg==
-----END DH PARAMETERS-----
'''


def in_dev_mode() -> bool:
	return bool(args.DEV)


def get_token_server() -> str:
	if args.TOKENSERVER:
		return args.TOKENSERVER
	return TOKEN_SERVER_URL
