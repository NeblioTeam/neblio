#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import argparse
import multiprocessing as mp
import neblio_ci_libs as nci

working_dir = os.getcwd()
build_dir = "build"
deploy_dir = os.path.join(os.environ['TRAVIS_BUILD_DIR'],'deploy', '')

parser = argparse.ArgumentParser()
parser.add_argument('--test', '-t', help='Only build and run tests', action='store_true')
args = parser.parse_args()

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
"libcurl4-openssl-dev",
"libldap2-dev",
"libidn11-dev",
"librtmp-dev"
]

nci.install_packages_debian(packages_to_install)

nci.mkdir_p(deploy_dir)
nci.mkdir_p(build_dir)
os.chdir(build_dir)

nci.call_with_err_code('python $TRAVIS_BUILD_DIR/build_scripts/CompileOpenSSL-Linux.py')
nci.call_with_err_code('python $TRAVIS_BUILD_DIR/build_scripts/CompileCurl-Linux.py')
nci.call_with_err_code('python $TRAVIS_BUILD_DIR/build_scripts/CompileQREncode-Linux.py')

os.environ['PKG_CONFIG_PATH'] = os.path.join(working_dir, build_dir, 'curl_build/lib/pkgconfig/')
os.environ['OPENSSL_INCLUDE_PATH'] = os.path.join(working_dir, build_dir, 'openssl_build/include/')
os.environ['OPENSSL_LIB_PATH'] = os.path.join(working_dir, build_dir, 'openssl_build/lib/')

if (args.test):
	nci.call_with_err_code('qmake "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" "NEBLIO_CONFIG += NoWallet" ../neblio-wallet.pro')
	nci.call_with_err_code("make -j" + str(mp.cpu_count()))
	# run tests
	nci.call_with_err_code("./wallet/test/neblio-tests")
else:
	nci.call_with_err_code('qmake "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" ../neblio-wallet.pro')
	nci.call_with_err_code("make -j" + str(mp.cpu_count()))

	file_name = '$(date +%Y-%m-%d-%T)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---neblio-Qt---ubuntu16.04.tar.gz'

	nci.call_with_err_code('tar -zcvf "' + file_name + '" -C ./wallet neblio-qt')
	nci.call_with_err_code('mv ' + file_name + ' ' + deploy_dir)
	nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')

print("")
print("")
print("Building finished successfully.")
print("")
