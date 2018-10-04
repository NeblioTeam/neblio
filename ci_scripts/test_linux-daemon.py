#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import neblio_ci_libs as nci

build_dir = "wallet"

packages_to_install = \
[
"qt5-default",
"qt5-qmake",
"qtbase5-dev-tools",
"qttools5-dev-tools",
"build-essential",
"libboost-dev",
"libboost-system-dev",
"libboost-filesystem-dev",
"libboost-program-options-dev",
"libboost-thread-dev",
"libboost-regex-dev",
"libssl-dev",
"libdb++-dev",
"libminiupnpc-dev",
"libqrencode-dev",
"libcurl4-openssl-dev"
]

nci.install_packages_debian(packages_to_install)

os.chdir(build_dir)

nci.call_with_err_code('python $TRAVIS_BUILD_DIR/build_scripts/CompileOpenSSL-Linux.py')
os.environ['OPENSSL_INCLUDE_PATH'] = '$TRAVIS_BUILD_DIR/openssl_build/include/'
os.environ['OPENSSL_LIB_PATH'] = '$TRAVIS_BUILD_DIR/openssl_build/lib/'

nci.call_with_err_code('python $TRAVIS_BUILD_DIR/build_scripts/CompileCurl-Linux.py')
os.environ['PKG_CONFIG_PATH'] = '$TRAVIS_BUILD_DIR/curl_build/lib/pkgconfig/'

nci.call_with_err_code('make "STATIC=1" -B -w -f makefile.unix -j3')

print("")
print("")
print("Building finished successfully.")
print("")
