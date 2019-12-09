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
"libboost-regex-dev",
"libboost-iostreams-dev",
"libboost-random-dev",
"libboost-chrono-dev",
"libboost-atomic-dev",
"libssl-dev",
"libdb++-dev",
"libminiupnpc-dev",
"libqrencode-dev",
"libcurl4-openssl-dev",
"libldap2-dev",
"libidn11-dev",
"librtmp-dev",
"lib32z1",
"libx32z1",
"libx32z1-dev",
"zlib1g",
"zlib1g-dev",
"lib32z1-dev",
"libsodium-dev"
]

nci.install_packages_debian(packages_to_install)

nci.mkdir_p(deploy_dir)
nci.mkdir_p(build_dir)
os.chdir(build_dir)

nci.call_with_err_code('ccache -s')

nci.call_with_err_code('python ../build_scripts/CompileOpenSSL-Linux.py')
nci.call_with_err_code('python ../build_scripts/CompileCurl-Linux.py')
nci.call_with_err_code('python ../build_scripts/CompileQREncode-Linux.py')

pkg_config_path = os.path.join(working_dir, build_dir, 'curl_build/lib/pkgconfig/')
openssl_include_path = os.path.join(working_dir, build_dir, 'openssl_build/include/')
openssl_lib_path = os.path.join(working_dir, build_dir, 'openssl_build/lib/')
qrencode_lib_path = os.path.join(working_dir, build_dir, 'qrencode_build/lib/')
qrencode_include_path = os.path.join(working_dir, build_dir, 'qrencode_build/include/')

os.environ['PKG_CONFIG_PATH'] = pkg_config_path
os.environ['OPENSSL_INCLUDE_PATH'] = openssl_include_path
os.environ['OPENSSL_LIB_PATH'] = openssl_lib_path
os.environ['QRENCODE_INCLUDE_PATH'] = qrencode_include_path
os.environ['QRENCODE_LIB_PATH'] = openssl_lib_path

# prepend ccache to the path, necessary since prior steps prepend things to the path
os.environ['PATH'] = '/usr/lib/ccache:' + os.environ['PATH']

if (args.test):
        nci.call_with_err_code('qmake "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" "OPENSSL_INCLUDE_PATH=' + openssl_include_path + '" "OPENSSL_LIB_PATH=' + openssl_lib_path + '" "QRENCODE_LIB_PATH=' + qrencode_lib_path + '" "QRENCODE_INCLUDE_PATH=' + qrencode_include_path + '" "PKG_CONFIG_PATH=' + pkg_config_path + '" "NEBLIO_CONFIG += NoWallet" ../neblio-wallet.pro')
	nci.call_with_err_code("make -j" + str(mp.cpu_count()))
	# run tests
	nci.call_with_err_code("./wallet/test/neblio-tests")
else:
        nci.call_with_err_code('qmake "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" "OPENSSL_INCLUDE_PATH=' + openssl_include_path + '" "OPENSSL_LIB_PATH=' + openssl_lib_path + '" "QRENCODE_LIB_PATH=' + qrencode_lib_path + '" "QRENCODE_INCLUDE_PATH=' + qrencode_include_path + '" "PKG_CONFIG_PATH=' + pkg_config_path + '" ../neblio-wallet.pro')
	nci.call_with_err_code("make -j" + str(mp.cpu_count()))

	file_name = '$(date +%Y-%m-%d)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---neblio-Qt---ubuntu16.04.tar.gz'

	nci.call_with_err_code('tar -zcvf "' + file_name + '" -C ./wallet neblio-qt')
	nci.call_with_err_code('mv ' + file_name + ' ' + deploy_dir)
	nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')

nci.call_with_err_code('ccache -s')

print("")
print("")
print("Building finished successfully.")
print("")
