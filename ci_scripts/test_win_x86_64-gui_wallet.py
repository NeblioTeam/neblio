#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import multiprocessing as mp
import neblio_ci_libs as nci

nci.setup_travis_or_gh_actions_env_vars()

build_dir = "build"

packages_to_install = \
[
"ccache",
"wget",
"make",
"binutils"
]

nci.install_packages_debian(packages_to_install)

working_dir = os.getcwd()
deploy_dir = os.path.join(os.environ['BUILD_DIR'],'deploy', '')
nci.mkdir_p(deploy_dir)

mxe_path = "/mxe/mxe/"
# download the toolchain for windows
nci.call_with_err_code("wget --progress=dot:giga https://assets.nebl.io/dependencies/mxe_x86_64.tar.gz")
# extract it
nci.call_with_err_code("tar -xf mxe_x86_64.tar.gz")
# move it to /mxe, where it was built the first time
nci.call_with_err_code("sudo mv mxe /")

# add mxe to PATH
mxe_bin_path = os.path.join(mxe_path, "usr/bin/")
os.environ["PATH"] += (":" + mxe_bin_path)

os.chdir(working_dir)

# prepend ccache to the path, necessary since prior steps prepend things to the path
os.environ['PATH'] = '/usr/lib/ccache:' + os.environ['PATH']

#Go to build dir and build
nci.mkdir_p(build_dir)
os.chdir(build_dir)
nci.call_with_err_code('x86_64-w64-mingw32.static-qmake-qt5 "QMAKE_CXX=ccache x86_64-w64-mingw32.static-g++" "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" ../neblio-wallet.pro')
nci.call_with_err_code("make -j" + str(mp.cpu_count()))

file_name = '$(date +%Y-%m-%d)---' + os.environ['BRANCH'] + '-' + os.environ['COMMIT'][:7] + '---BETA-neblio-Qt---windows-x86_64.zip'

nci.call_with_err_code('zip -j ' + file_name + ' ./wallet/release/neblio-qt.exe')
nci.call_with_err_code('mv ' + file_name + ' ' + deploy_dir)
nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')
# set the SOURCE_DIR & SOURCE_PATH env vars, these point to the binary that will be uploaded
nci.call_with_err_code('echo "SOURCE_DIR='  + deploy_dir + '" >> $GITHUB_ENV')
nci.call_with_err_code('echo "SOURCE_PATH=' + deploy_dir + file_name + '" >> $GITHUB_ENV')

nci.call_with_err_code('ccache -s')

################

#back to working dir
os.chdir(working_dir)

print("")
print("")
print("Building finished. Find the executable in " + os.path.join(working_dir, build_dir, "release"))
print("")

