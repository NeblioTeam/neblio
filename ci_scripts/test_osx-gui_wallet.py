#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import multiprocessing as mp
import neblio_ci_libs as nci

working_dir = os.getcwd()
build_dir = "build"

nci.mkdir_p(build_dir)
os.chdir(build_dir)

nci.call_with_err_code('brew update')
nci.call_with_err_code('brew install qt && brew link --force qt')
nci.call_with_err_code('brew install berkeley-db@4 && brew link --force berkeley-db@4')
nci.call_with_err_code('brew install boost@1.60 && brew link --force boost@1.60')

nci.call_with_err_code('qmake "USE_UPNP=1" "USE_QRCODE=0" "RELEASE=1" "NEBLIO_CONFIG += Tests" ../neblio-wallet.pro')
nci.call_with_err_code("make -j" + str(mp.cpu_count()))

# run tests
nci.call_with_err_code("./wallet/test/neblio-tests")

print("")
print("")
print("Building finished successfully.")
print("")
