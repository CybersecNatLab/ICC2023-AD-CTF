.PHONY: all cryptolib-python

all: cryptolibnode.js cryptolib.js cryptolib-python

cryptolibnode.js: src/* include/* cryptopp
	em++ -lembind libcryptopp-wasm.a -s ALLOW_MEMORY_GROWTH=1 -s ASSERTIONS=0 -s WASM_ASYNC_COMPILATION=0 -s MALLOC=emmalloc -s MODULARIZE=1 -s SINGLE_FILE=1 -o cryptolibnode.js -I include -I . --std=c++17 src/client.cpp src/register.cpp src/utils.cpp src/files.cpp -isystem /usr/include -isystem /usr/include/x86_64-linux-gnu/ -v

cryptolib.js: src/* include/* cryptopp
	em++ -lembind libcryptopp-wasm.a -s ALLOW_MEMORY_GROWTH=1 -s ASSERTIONS=0 -s MALLOC=emmalloc -s SINGLE_FILE=1 -o cryptolib.js -I include -I . --std=c++17 src/client.cpp src/register.cpp src/utils.cpp src/files.cpp -isystem /usr/include -isystem /usr/include/x86_64-linux-gnu/ -v

cryptolib-python: src/* include/* cryptopp
	docker run -v $$(pwd):/app python:3.11-bookworm sh -c "cd app; apt install -y g++; pip3 install pybind11; g++ -shared -std=c++17 -fPIC \$$(python3 -m pybind11 --includes) -D PYBIND -I include -I . src/server.cpp src/utils.cpp libcryptopp.a -o cryptolib\$$(python3-config --extension-suffix); chown $$(id -u):$$(id -g) cryptolib.cpython*"

cryptopp:
	git clone https://github.com/weidai11/cryptopp.git
	cd cryptopp; git checkout a75a7d87ff03df37f6319c45a484b292465cc173
