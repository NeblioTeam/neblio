#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import neblio_ci_libs as nci


build_dir = "build"

working_dir = os.getcwd()

mxe_path = "/mxe/mxe/"
# download the toolchain for windows
nci.call_with_err_code("wget https://download.afach.de/mxe-trusty.tar.gz")
# extract it
nci.call_with_err_code("tar -xvf mxe-trusty.tar.gz")
# move it to /mxe, where it was built the first time
nci.call_with_err_code("sudo mv mxe /")

# add mxe to PATH
mxe_bin_path = os.path.join(mxe_path, "usr/bin/")
os.environ["PATH"] += (":" + mxe_bin_path)

#build leveldb
print("Building leveldb...")
CC_path = os.path.join(mxe_bin_path, "i686-w64-mingw32.static-gcc")
CXX_path = os.path.join(mxe_bin_path, "i686-w64-mingw32.static-g++")
os.chdir("src/leveldb")
print("Cleaning leveldb...")
nci.call_with_err_code("make clean")
print("Done cleaning.")
print("Starting build process...")
nci.call_with_err_code("TARGET_OS=NATIVE_WINDOWS make libleveldb.a libmemenv.a CC=" + CC_path + " CXX=" + CXX_path)
print("Done.")

os.chdir(working_dir)

#Go to build dir and build
nci.mkdir_p(build_dir)
os.chdir(build_dir)
nci.call_with_err_code('i686-w64-mingw32.static-qmake-qt5 "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" ../neblio-qt.pro')
nci.call_with_err_code("make -j6")
################

#back to working dir
os.chdir(working_dir)

print("")
print("")
print("Building finished. Find the executable in " + os.path.join(working_dir, build_dir, "release"))
print("")

