#!/bin/bash -e

# VSmartcard
./.github/setup-vsmartcard.sh

# Javacard SDKs
git clone https://github.com/martinpaljak/oracle_javacard_sdks.git
export JC_HOME=$PWD/oracle_javacard_sdks/jc222_kit
export JC_CLASSIC_HOME=$PWD/oracle_javacard_sdks/jc305u3_kit

# jCardSim
git clone https://github.com/arekinath/jcardsim.git
pushd jcardsim
env | grep -i openjdk
export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64/
mvn initialize && mvn clean install
popd
