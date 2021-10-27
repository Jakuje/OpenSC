#!/bin/bash

if [ ! -d "openssl" ]; then
	git clone https://github.com/openssl/openssl
fi

cd openssl && git checkout openssl-3.0.0-beta2 \
	&& ./Configure --prefix=/usr \
	&& make -j && make install

export LD_LIBRARY_PATH=/usr/lib:/usr/lib64