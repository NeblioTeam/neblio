import os
import urllib2
import multiprocessing as mp
import neblio_ci_libs as nci

nci.setup_travis_or_gh_actions_env_vars()

working_dir = os.getcwd()
deploy_dir = os.path.join(os.environ['BUILD_DIR'],'deploy', '')

nci.mkdir_p(deploy_dir)
os.chdir(deploy_dir)

build_target = ''
build_target_alt = ''
if(os.environ['target_v'] == "linux_daemon"):
  build_target = 'nebliod'
  build_target_alt = 'nebliod'
elif(os.environ['target_v'] == "linux_wallet_test"):
  build_target = 'tests-neblio-qt'
  build_target_alt = 'tests-neblio-Qt'
else:
  build_target = 'neblio-qt'
  build_target_alt = 'neblio-Qt'

# Install docker
# nci.call_with_err_code('curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh && rm get-docker.sh')

# move .ccache folder to our deploy directory
nci.mkdir_p(os.path.join(working_dir,'.ccache', ''))
nci.call_with_err_code('mv ' + os.path.join(working_dir,'.ccache', '') + ' ' + os.path.join(deploy_dir,'.ccache', ''))

# Start Docker Container to Build nebliod or neblio-Qt
nci.call_with_err_code('sudo docker run -e BUILD=' + build_target + ' -v ' + os.environ['BUILD_DIR'] + ':/root/vol -t neblioteam/nebliod-build-ccache')
nci.call_with_err_code('sleep 15 && sudo docker kill $(sudo docker ps -q);exit 0')

# move .ccache folder back to ccache dir
nci.call_with_err_code('mv ' + os.path.join(deploy_dir,'.ccache', '') + ' ' + os.path.join(working_dir,'.ccache', ''))

file_name = '$(date +%Y-%m-%d)---' + os.environ['BRANCH'] + '-' + os.environ['COMMIT'][:7] + '---' + build_target_alt + '---ubuntu16.04.tar.gz'

# Check if binary exists before trying to package it.
# If it does not exist we had a build timeout
if(os.path.isfile(build_target)):
  nci.call_with_err_code('tar -zcvf "' + file_name + '" ' + build_target)
  nci.call_with_err_code('rm -f ' + build_target)
  nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')
  # set the SOURCE_DIR & SOURCE_PATH env vars, these point to the binary that will be uploaded
  nci.call_with_err_code('echo "::set-env name=SOURCE_DIR::'  + deploy_dir + '"')
  nci.call_with_err_code('echo "::set-env name=SOURCE_PATH::' + deploy_dir + file_name + '"')

  # if we are just running tests, delete the deploy package
  if(os.environ['target_v'] == "linux_wallet_test"):
    nci.call_with_err_code('rm -f ' + deploy_dir + file_name)