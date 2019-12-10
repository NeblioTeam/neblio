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

# set up ccache
nci.call_with_err_code('brew fetch --retry ccache        && brew install ccache --force')

nci.call_with_err_code('brew fetch --retry qt            && brew install qt --force')
nci.call_with_err_code('brew fetch --retry berkeley-db@4 && brew install berkeley-db@4 --force')
nci.call_with_err_code('brew fetch --retry boost@1.60    && brew install boost@1.60 --force')
nci.call_with_err_code('brew fetch --retry miniupnpc     && brew install miniupnpc --force')
nci.call_with_err_code('brew fetch --retry curl          && brew install curl --force')
nci.call_with_err_code('brew fetch --retry openssl       && brew install openssl --force')
nci.call_with_err_code('brew fetch --retry qrencode      && brew install qrencode --force')
nci.call_with_err_code('brew fetch --retry libsodium     && brew install libsodium --force')

nci.call_with_err_code('brew unlink qt            && brew link --force --overwrite qt')
nci.call_with_err_code('brew unlink berkeley-db@4 && brew link --force --overwrite berkeley-db@4')
nci.call_with_err_code('brew unlink boost@1.60    && brew link --force --overwrite boost@1.60')
nci.call_with_err_code('brew unlink miniupnpc     && brew link --force --overwrite miniupnpc')
nci.call_with_err_code('brew unlink curl          && brew link --force --overwrite curl')
nci.call_with_err_code('brew unlink python        && brew link --force --overwrite python')
nci.call_with_err_code('brew unlink openssl       && brew link --force --overwrite openssl')
nci.call_with_err_code('brew unlink qrencode      && brew link --force --overwrite qrencode')
nci.call_with_err_code('brew unlink libsodium     && brew link --force --overwrite libsodium')


nci.call_with_err_code('ccache -s')

# prepend ccache to the path, necessary since prior steps prepend things to the path
os.environ['PATH'] = '/usr/local/opt/ccache/libexec:' + os.environ['PATH']

if (args.test):
    nci.call_with_err_code('qmake "QMAKE_CXX=ccache clang++" "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" "NEBLIO_CONFIG += NoWallet" ../neblio-wallet.pro')
    nci.call_with_err_code("make -j" + str(mp.cpu_count()))
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
