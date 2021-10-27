#!/bin/bash

set -ex -o xtrace

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig;

if [ "$GITHUB_EVENT_NAME" == "pull_request" ]; then
	PR_NUMBER=$(echo $GITHUB_REF | awk 'BEGIN { FS = "/" } ; { print $3 }')
	if [ "$GITHUB_BASE_REF" == "master" ]; then
		./bootstrap.ci -s "-pr$PR_NUMBER"
	else
		./bootstrap.ci -s "$GITHUB_BASE_REF-pr$PR_NUMBER"
	fi
else
	BRANCH=$(echo $GITHUB_REF | awk 'BEGIN { FS = "/" } ; { print $3 }')
	if [ "$BRANCH" == "master" ]; then
		./bootstrap
	else
		./bootstrap.ci -s "$BRANCH"
	fi
fi

./configure  --disable-dependency-tracking --disable-strict
make -j 2 V=1

make distcheck
make dist

sudo make install
