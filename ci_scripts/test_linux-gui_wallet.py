#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
from subprocess import call
import sys
import re
import multiprocessing as mp
import string
import urllib
import shutil
import errno

neblio_src = "neblio"
build_dir = "build"

CC_path  = "gcc"
CXX_path = "g++"

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
"libssl-dev",
"libdb++-dev",
"libminiupnpc-dev",
"libqrencode-dev"
]

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

def call_with_err_code(cmd):
    err_code = call(cmd, shell=True)
    if err_code != 0:
        print("")
        print("")
        sys.stderr.write('call \'' + cmd + '\' exited with error code ' + str(err_code) +' \n')
        print("")
        exit(err_code)

def install_packages():
    call_with_err_code('sudo apt-get -y install ' + " ".join(packages_to_install))

install_packages()

working_dir = os.getcwd()

os.chdir(working_dir)

mkdir_p(build_dir)
os.chdir(build_dir)
call_with_err_code('qmake "USE_UPNP=1" "USE_QRCODE=0" "RELEASE=1" ../neblio-qt.pro')
call_with_err_code("make -j3")

os.chdir(working_dir)

print("")
print("")
print("Building finished successfully.")
print("")
