#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import neblio_ci_libs as nci

build_dir = "build"

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

nci.mkdir_p(build_dir)
os.chdir(build_dir)
nci.call_with_err_code('qmake "USE_UPNP=1" "USE_QRCODE=0" "RELEASE=1" ../neblio-wallet.pro')
nci.call_with_err_code("make -j3")


print("")
print("")
print("Building finished successfully.")
print("")
