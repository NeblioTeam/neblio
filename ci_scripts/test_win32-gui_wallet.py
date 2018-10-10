#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import multiprocessing as mp
import neblio_ci_libs as nci


build_dir = "build"

packages_to_install = \
[
"wget",
"make",
"binutils"
]

nci.install_packages_debian(packages_to_install)

working_dir = os.getcwd()
deploy_dir = os.path.join(os.environ['TRAVIS_BUILD_DIR'],'deploy', '')
nci.mkdir_p(deploy_dir)

mxe_path = "/mxe/mxe/"
# download the toolchain for windows
nci.call_with_err_code("wget https://neblio-files.ams3.digitaloceanspaces.com/dependencies/mxe.tar.gz")
# extract it
nci.call_with_err_code("tar -xf mxe.tar.gz")
# move it to /mxe, where it was built the first time
nci.call_with_err_code("sudo mv mxe /")

# add mxe to PATH
mxe_bin_path = os.path.join(mxe_path, "usr/bin/")
os.environ["PATH"] += (":" + mxe_bin_path)

#build leveldb
print("Building leveldb...")
CC_path = os.path.join(mxe_bin_path, "i686-w64-mingw32.static-gcc")
CXX_path = os.path.join(mxe_bin_path, "i686-w64-mingw32.static-g++")
os.chdir("wallet/leveldb")
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
nci.call_with_err_code('i686-w64-mingw32.static-qmake-qt5 "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" ../neblio-wallet.pro')
nci.call_with_err_code("make -j" + str(mp.cpu_count()))

file_name = '$(date +%Y-%m-%d-%T)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---neblio-Qt---windows.zip'

nci.call_with_err_code('zip -j ' + file_name + ' ./wallet/release/neblio-qt.exe')
nci.call_with_err_code('mv ' + file_name + ' ' + deploy_dir)
nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')

################

#back to working dir
os.chdir(working_dir)

print("")
print("")
print("Building finished. Find the executable in " + os.path.join(working_dir, build_dir, "release"))
print("")

