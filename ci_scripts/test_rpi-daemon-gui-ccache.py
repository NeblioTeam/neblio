import os
import httplib
import multiprocessing as mp
import neblio_ci_libs as nci

working_dir = os.getcwd()
deploy_dir = os.path.join(os.environ['TRAVIS_BUILD_DIR'],'deploy', '')
build_cache_name = 'rpi-build-ccache-' + os.environ['TRAVIS_BRANCH'] + 'tar.gz'

# First make sure we have a ccache file for this branch
c = httplib.HTTPConnection('https://neblio-build-staging.ams3.digitaloceanspaces.com/' + build_cache_name)
c.request("HEAD", '')
if c.getresponse().status != 200:
  print('ccache file does not exist remotely for this branch, skip building & testing')
  exit(0)

# Install docker
nci.call_with_err_code('curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh')

os.chdir(deploy_dir)

# Download Build Cache & Extract
nci.call_with_err_code('wget https://neblio-build-staging.ams3.digitaloceanspaces.com/' + build_cache_name)
nci.call_with_err_code('tar -zxf ' + build_cache_name)

# Start Docker Container to Build nebliod & neblio-Qt
nci.call_with_err_code('sudo docker run -e BRANCH=' + os.environ['TRAVIS_BRANCH'] + '-v ' + deploy_dir + ':/root/deploy -t neblioteam/nebliod-build-ccache-rpi')

# Package Binaries & Cache
nci.call_with_err_code('tar -zcf ' + deploy_dir + build_cache_name + ' ' + deploy_dir + '.ccache')
nci.call_with_err_code('rm -rf ' + deploy_dir + '.ccache')

file_name = '$(date +%Y-%m-%d)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---neblio-Qt-nebliod---raspbian-stretch.tar.gz'
nci.call_with_err_code('tar -zcvf "' + file_name + '" neblio-qt nebliod')
nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')