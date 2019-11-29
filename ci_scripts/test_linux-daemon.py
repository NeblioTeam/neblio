#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import multiprocessing as mp
import neblio_ci_libs as nci

working_dir = os.getcwd()
build_dir = "wallet"
deploy_dir = os.path.join(os.environ['TRAVIS_BUILD_DIR'],'deploy', '')

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
"lib32z1-dev"
]

nci.install_packages_debian(packages_to_install)

nci.mkdir_p(deploy_dir)
os.chdir(build_dir)

nci.call_with_err_code('ccache -s')

nci.call_with_err_code('python ../build_scripts/CompileOpenSSL-Linux.py')
nci.call_with_err_code('python ../build_scripts/CompileCurl-Linux.py')
nci.call_with_err_code('python ../build_scripts/CompileLibsodium-Linux.py')

os.environ['PKG_CONFIG_PATH'] = os.path.join(working_dir, build_dir, 'curl_build/lib/pkgconfig/')
os.environ['OPENSSL_INCLUDE_PATH'] = os.path.join(working_dir, build_dir, 'openssl_build/include/')
os.environ['OPENSSL_LIB_PATH'] = os.path.join(working_dir, build_dir, 'openssl_build/lib/')

nci.call_with_err_code('ccache -s')

# prepend ccache to the path, necessary since prior steps prepend things to the path
os.environ['PATH'] = '/usr/lib/ccache:' + os.environ['PATH']

nci.call_with_err_code('make "CXX=ccache g++" "STATIC=1" -B -w -f makefile.unix -j' + str(mp.cpu_count()))
nci.call_with_err_code('strip ./nebliod')

file_name = '$(date +%Y-%m-%d)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---nebliod---ubuntu16.04.tar.gz'

nci.call_with_err_code('tar -zcvf "' + file_name + '" ./nebliod')
nci.call_with_err_code('mv ' + file_name + ' ' + deploy_dir)
nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')

nci.call_with_err_code('ccache -s')

print("")
print("")
print("Building finished successfully.")
print("")
