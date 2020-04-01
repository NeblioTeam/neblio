#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import multiprocessing as mp
import neblio_ci_libs as nci

nci.setup_travis_or_gh_actions_env_vars()

working_dir = os.getcwd()
build_dir = "wallet"
deploy_dir = os.path.join(os.environ['BUILD_DIR'],'deploy', '')

packages_to_install = \
[
"ccache",
"qt5-default",
"qt5-qmake",
"qtbase5-dev-tools",
"qttools5-dev-tools",
"qttools5-dev",
"build-essential",
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
"libsodium-dev",
"libboost-all-dev",
"libdbus-glib-1-dev",
"gdb",
"python3",
"python3-pip",
"python3-setuptools"
]

nci.install_packages_debian(packages_to_install)

nci.mkdir_p(deploy_dir)
os.chdir(build_dir)

nci.call_with_err_code('ccache -s')

# prepend ccache to the path, necessary since prior steps prepend things to the path
os.environ['PATH'] = '/usr/lib/ccache:' + os.environ['PATH']


nci.call_with_err_code('cmake -DNEBLIO_CMAKE=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEBLIO_FORCE_DISABLE_PREMADE_DATA_DOWNLOAD=ON ..')
nci.call_with_err_code("make -j" + str(mp.cpu_count()))

nci.call_with_err_code('ccache -s')

# download test data
nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/test_data_mainnet_tab.tar.xz -O ../wallet/test/data/test_data_mainnet_tab.tar.xz')
nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/test_data_testnet_tab.tar.xz -O ../wallet/test/data/test_data_testnet_tab.tar.xz')
nci.call_with_err_code('tar -xJvf ../wallet/test/data/test_data_mainnet_tab.tar.xz -C ../wallet/test/data')
nci.call_with_err_code('tar -xJvf ../wallet/test/data/test_data_testnet_tab.tar.xz -C ../wallet/test/data')
nci.call_with_err_code('rm ../wallet/test/data/*.tar.xz')

# run unit tests
nci.call_with_err_code('./wallet/test/neblio-tests')

# run system tests
nci.call_with_err_code('pip3 install litecoin_scrypt')
nci.call_with_err_code('python3 -u ../test/functional/test_runner.py feature_block.py')
nci.call_with_err_code('python3 -u ../test/functional/test_runner.py wallet_accounts.py')
nci.call_with_err_code('python3 -u ../test/functional/test_runner.py rpc_listtransactions.py')
nci.call_with_err_code('python3 -u ../test/functional/test_runner.py rpc_rawtransaction.py')
nci.call_with_err_code('python3 -u ../test/functional/test_runner.py rpc_blockchain.py')
nci.call_with_err_code('python3 -u ../test/functional/test_runner.py p2p_invalid_block.py')
nci.call_with_err_code('python3 -u ../test/functional/test_runner.py p2p_invalid_tx.py')

print("")
print("")
print("Building finished successfully.")
print("")
