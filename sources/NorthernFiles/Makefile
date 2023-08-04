# Copying abiondo's idea of having a Makefile

.PHONY: all cryptolib

all: cryptolib
	rm -rf ../../services/$${PWD##*/}
	cp -r . ../../services/$${PWD##*/}
	rm ../../services/$${PWD##*/}/README.md
	rm ../../services/$${PWD##*/}/Makefile
	rm -rf ../../services/$${PWD##*/}/cryptolib

	cp frontend/src/_cli/*.ts ../../checkers/$${PWD##*/}
	cp -r frontend/src/_cli/_api ../../checkers/$${PWD##*/}
	cd ../../checkers/$${PWD##*/} && npm ci && npm run build

cryptolib:
	$(MAKE) -C cryptolib
	mv cryptolib/cryptolib.js frontend/static/cryptolib.js
	mv cryptolib/cryptolibnode.js frontend/src/_cli/_api/cryptolibnode.js
	mv cryptolib/cryptolib* auth/src/