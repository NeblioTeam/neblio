#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import argparse
import multiprocessing as mp
import neblio_ci_libs as nci

nci.setup_travis_or_gh_actions_env_vars()

working_dir = os.getcwd()
build_dir = "build"
deploy_dir = os.path.join(os.environ['BUILD_DIR'],'deploy', '')

parser = argparse.ArgumentParser()
parser.add_argument('--test', '-t', help='Only build and run tests', action='store_true')
args = parser.parse_args()

nci.mkdir_p(deploy_dir)
nci.mkdir_p(build_dir)
os.chdir(build_dir)

# do not auto update homebrew as it is very slow
os.environ['HOMEBREW_NO_AUTO_UPDATE'] = '1'

# remove existing deps that come pre installed
nci.call_with_err_code('brew uninstall --ignore-dependencies ccache || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies qt || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies berkeley-db@4 || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies boost || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies miniupnpc || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies curl || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies openssl || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies openssl@1.1 || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies qrencode || true')
nci.call_with_err_code('brew uninstall --ignore-dependencies libsodium || true')

# Install High Seirra Versions of Depeendencies, due to that being the minimum version we support
#ccache
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=ccache-3.7.6.high_sierra.bottle.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=ccache-3.7.6.high_sierra.bottle.tar.gz')
#qt
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=qt-5.13.2.high_sierra.bottle.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=qt-5.13.2.high_sierra.bottle.tar.gz')
#berkeley-db@4
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=berkeley-db%404-4.8.30.high_sierra.bottle.1.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=berkeley-db%404-4.8.30.high_sierra.bottle.1.tar.gz')
#boost
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=boost-1.71.0.high_sierra.bottle.1.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=boost-1.71.0.high_sierra.bottle.1.tar.gz')
#miniupnpc
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=miniupnpc-2.1.high_sierra.bottle.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=miniupnpc-2.1.high_sierra.bottle.tar.gz')
#curl
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=curl-7.67.0.high_sierra.bottle.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=curl-7.67.0.high_sierra.bottle.tar.gz')
#openssl
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=openssl%401.1-1.1.1d.high_sierra.bottle.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=openssl%401.1-1.1.1d.high_sierra.bottle.tar.gz')
#qrencode
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=qrencode-4.0.2.high_sierra.bottle.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=qrencode-4.0.2.high_sierra.bottle.tar.gz')
#libsodium
nci.call_with_err_code('brew fetch   --retry https://bintray.com/homebrew/bottles/download_file?file_path=libsodium-1.0.18_1.high_sierra.bottle.tar.gz')
nci.call_with_err_code('brew install --force https://bintray.com/homebrew/bottles/download_file?file_path=libsodium-1.0.18_1.high_sierra.bottle.tar.gz')

# force relinking
nci.call_with_err_code('brew unlink qt            && brew link --force --overwrite qt')
nci.call_with_err_code('brew unlink berkeley-db@4 && brew link --force --overwrite berkeley-db@4')
nci.call_with_err_code('brew unlink boost         && brew link --force --overwrite boost')
nci.call_with_err_code('brew unlink miniupnpc     && brew link --force --overwrite miniupnpc')
nci.call_with_err_code('brew unlink curl          && brew link --force --overwrite curl')
nci.call_with_err_code('brew unlink python        && brew link --force --overwrite python')
nci.call_with_err_code('brew unlink openssl@1.1   && brew link --force --overwrite openssl@1.1')
nci.call_with_err_code('brew unlink qrencode      && brew link --force --overwrite qrencode')
nci.call_with_err_code('brew unlink libsodium     && brew link --force --overwrite libsodium')


nci.call_with_err_code('ccache -s')

# prepend ccache to the path, necessary since prior steps prepend things to the path
os.environ['PATH'] = '/usr/local/opt/ccache/libexec:' + os.environ['PATH']

if (args.test):
    nci.call_with_err_code('qmake "QMAKE_CXX=ccache clang++" "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" "NEBLIO_CONFIG += NoWallet" ../neblio-wallet.pro')
    nci.call_with_err_code("make -j" + str(mp.cpu_count()))
    # download test data
    nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/ntp1txids_to_test_testnet.txt.tar.gz -O ./wallet/test/data/ntp1txids_to_test_testnet.txt.tar.gz')
    nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/ntp1txids_to_test.txt.tar.gz -O ./wallet/test/data/ntp1txids_to_test.txt.tar.gz')
    nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/txs_ntp1tests_ntp1_txs_testnet.json.tar.gz -O ./wallet/test/data/txs_ntp1tests_ntp1_txs_testnet.json.tar.gz')
    nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/txs_ntp1tests_ntp1_txs.json.tar.gz -O ./wallet/test/data/txs_ntp1tests_ntp1_txs.json.tar.gz')
    nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/txs_ntp1tests_raw_neblio_txs_testnet.json.tar.gz -O ./wallet/test/data/txs_ntp1tests_raw_neblio_txs_testnet.json.tar.gz')
    nci.call_with_err_code('wget --progress=dot:giga https://files.nebl.io/txs_ntp1tests_raw_neblio_txs.json.tar.gz -O ./wallet/test/data/txs_ntp1tests_raw_neblio_txs.json.tar.gz')
    nci.call_with_err_code('tar -xzvf ./wallet/test/data/ntp1txids_to_test_testnet.txt.tar.gz -C ./wallet/test/data')
    nci.call_with_err_code('tar -xzvf ./wallet/test/data/ntp1txids_to_test.txt.tar.gz -C ./wallet/test/data')
    nci.call_with_err_code('tar -xzvf ./wallet/test/data/txs_ntp1tests_ntp1_txs_testnet.json.tar.gz -C ./wallet/test/data')
    nci.call_with_err_code('tar -xzvf ./wallet/test/data/txs_ntp1tests_ntp1_txs.json.tar.gz -C ./wallet/test/data')
    nci.call_with_err_code('tar -xzvf ./wallet/test/data/txs_ntp1tests_raw_neblio_txs_testnet.json.tar.gz -C ./wallet/test/data')
    nci.call_with_err_code('tar -xzvf ./wallet/test/data/txs_ntp1tests_raw_neblio_txs.json.tar.gz -C ./wallet/test/data')
    nci.call_with_err_code('rm ./wallet/test/data/*.tar.gz')
    # run tests
    nci.call_with_err_code("./wallet/test/neblio-Qt.app/Contents/MacOS/neblio-Qt")
else:
    nci.call_with_err_code('qmake "QMAKE_CXX=ccache clang++" "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" ../neblio-wallet.pro')
    nci.call_with_err_code("make -j" + str(mp.cpu_count()))
    # build our .dmg
    nci.call_with_err_code('npm install -g appdmg')
    os.chdir("wallet")
    nci.call_with_err_code('../../contrib/macdeploy/macdeployqtplus ./neblio-Qt.app -add-qt-tr da,de,es,hu,ru,uk,zh_CN,zh_TW -verbose 1 -rpath /usr/local/opt/qt/lib')
    nci.call_with_err_code('appdmg ../../contrib/macdeploy/appdmg.json ./neblio-Qt.dmg')

    file_name = '$(date +%Y-%m-%d)---' + os.environ['BRANCH'] + '-' + os.environ['COMMIT'][:7] + '---neblio-Qt---macOS.zip'

    nci.call_with_err_code('zip -j ' + file_name + ' ./neblio-Qt.dmg')
    nci.call_with_err_code('mv ' + file_name + ' ' + deploy_dir)
    nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')
    # set the SOURCE_DIR & SOURCE_PATH env vars, these point to the binary that will be uploaded
    nci.call_with_err_code('echo "::set-env name=SOURCE_DIR::'  + deploy_dir + '"')
    nci.call_with_err_code('echo "::set-env name=SOURCE_PATH::' + deploy_dir + file_name + '"')

nci.call_with_err_code('ccache -s')


print("")
print("")
print("Building finished successfully.")
print("")
