#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import neblio_ci_libs as nci

build_dir = "src"

packages_to_install = \
[
"qt5-default",
"qt5-qmake",
"qtbase5-dev-tools",
"qttools5-dev-tools",
"build-essential",
"libboost-all-dev",
"libssl-dev",
"libdb++-dev",
"libminiupnpc-dev",
"libqrencode-dev",
"libcurl4-openssl-dev"
]

nci.install_packages_debian(packages_to_install)

nci.mkdir_p("src/obj/zerocoin")
os.chdir(build_dir)
nci.call_with_err_code('make "STATIC=1" -B -w -f makefile.unix -j3')

print("")
print("")
print("Building finished successfully.")
print("")
