import os
import urllib2
import multiprocessing as mp
import neblio_ci_libs as nci

working_dir = os.getcwd()
deploy_dir = os.path.join(os.environ['TRAVIS_BUILD_DIR'],'deploy', '')
build_cache_name = 'rpi-build-ccache-' + os.environ['TRAVIS_BRANCH'] + '.tar.gz'

nci.mkdir_p(deploy_dir)
os.chdir(deploy_dir)

# Download our ccache file, if it does not exist, skip building
try:
  url = 'https://neblio-build-staging.ams3.digitaloceanspaces.com/' + build_cache_name
  response = urllib2.urlopen(url)
  with open(build_cache_name, 'wb') as f: f.write(response.read())
except urllib2.HTTPError as err:
  if err.code == 404:
    print('ccache file does not exist remotely for this branch, skip building!')
    exit(0)
  else:
    raise

# Install docker
nci.call_with_err_code('curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh && rm get-docker.sh')

# Prepare qemu
nci.call_with_err_code('docker run --rm --privileged multiarch/qemu-user-static:register --reset')

# Extract Build ccache File
nci.call_with_err_code('tar -zxf ' + build_cache_name)

# Start Docker Container to Build nebliod & neblio-Qt
nci.call_with_err_code('sudo docker run -e BRANCH=' + os.environ['TRAVIS_BRANCH'] + ' -v ' + deploy_dir + ':/root/deploy -t neblioteam/nebliod-build-ccache-rpi')

# Package Binaries & Cache
nci.call_with_err_code('tar -zcf ' + deploy_dir + build_cache_name + ' ' + deploy_dir + '.ccache')
nci.call_with_err_code('rm -rf ' + deploy_dir + '.ccache')

file_name = '$(date +%Y-%m-%d)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---RPi-neblio-Qt-nebliod---raspbian-stretch.tar.gz'
nci.call_with_err_code('tar -zcvf "' + file_name + '" neblio-qt nebliod')
nci.call_with_err_code('rm neblio-qt && rm nebliod')
nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')