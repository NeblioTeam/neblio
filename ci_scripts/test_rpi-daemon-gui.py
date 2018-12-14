import os
import urllib2
import multiprocessing as mp
import neblio_ci_libs as nci

working_dir = os.getcwd()
deploy_dir = os.path.join(os.environ['TRAVIS_BUILD_DIR'],'deploy', '')

# If this is a PR, bail out instead of just wasting 45 mins running
if (os.environ['TRAVIS_PULL_REQUEST'] != 'false'):
  print('Pull Requests are not built for RPi since ccache cannot be used!')
  exit(0)

nci.mkdir_p(deploy_dir)
os.chdir(deploy_dir)

# Install docker
nci.call_with_err_code('curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh && rm get-docker.sh')

# Prepare qemu
nci.call_with_err_code('docker run --rm --privileged multiarch/qemu-user-static:register --reset')

# move .ccache folder to our deploy directory
nci.call_with_err_code('mv ' + os.path.join(os.environ['HOME'],'.ccache', '') + ' ' + os.path.join(deploy_dir,'.ccache', ''))

# Start Docker Container to Build nebliod & neblio-Qt
nci.call_with_err_code('timeout --signal=SIGKILL 42m sudo docker run -e BRANCH=' + os.environ['TRAVIS_BRANCH'] + ' -v ' + deploy_dir + ':/root/deploy -t neblioteam/nebliod-build-ccache-rpi')
nci.call_with_err_code('sleep 15 && sudo docker kill $(sudo docker ps -q);exit 0')

# move .ccache folder back to travis ccache dir
nci.call_with_err_code('mv ' + os.path.join(deploy_dir,'.ccache', '') + ' ' + os.path.join(os.environ['HOME'],'.ccache', ''))

file_name = '$(date +%Y-%m-%d)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---RPi-neblio-Qt-nebliod---raspbian-stretch.tar.gz'

# Check if BOTH binaries exist before trying to package them.
# If both binaries do not exist, delete any that do exist, as we had a build timeout
if(os.path.isfile('neblio-qt') and os.path.isfile('nebliod')):
  nci.call_with_err_code('tar -zcvf "' + file_name + '" neblio-qt nebliod')
  nci.call_with_err_code('sudo rm -f neblio-qt && sudo rm -f nebliod')
  nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')
else:
  if(os.path.isfile('neblio-qt')):
    nci.call_with_err_code('sudo rm -f neblio-qt')

  if(os.path.isfile('nebliod')):
    nci.call_with_err_code('sudo rm -f nebliod')

  nci.call_with_err_code('echo "Binaries neblio-qt and nebliod not found, likely due to a timeout, this job should be retried"')