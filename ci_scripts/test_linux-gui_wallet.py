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
neblio_target = "neblio-build"

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

def install_packages():
    call('sudo apt-get -y install ' + " ".join(packages_to_install), shell=True)

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

install_packages()

working_dir = os.getcwd()

CC_path  = "gcc"
CXX_path = "g++"
os.chdir(working_dir)

#Go to build dir
build_dir = "build"
mkdir_p(build_dir)
os.chdir(build_dir)
call('qmake "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" ../neblio-qt.pro', shell=True)
call("make -j3", shell=True)

#back to working dir
os.chdir(working_dir)

print("")
print("")
print("Building finished. Find the executable in " + build_dir)
print("")
