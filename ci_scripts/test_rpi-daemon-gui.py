import os
import urllib2
import multiprocessing as mp
import neblio_ci_libs as nci

nci.setup_travis_or_gh_actions_env_vars()

working_dir = os.getcwd()
deploy_dir = os.path.join(os.environ['BUILD_DIR'],'deploy', '')

# If this is a Travis PR, bail out instead of just wasting 45 mins running
if (os.environ.get('TRAVIS_BUILD_DIR') is not None and os.environ['TRAVIS_PULL_REQUEST'] != 'false'):
  print('Pull Requests are not built for RPi since ccache cannot be used!')
  exit(0)

nci.mkdir_p(deploy_dir)
os.chdir(deploy_dir)

build_target = ''
build_target_alt = ''
if(os.environ['target_v'] == "rpi_daemon"):
  build_target = 'nebliod'
  build_target_alt = 'nebliod'
elif(os.environ['target_v'] == "rpi_wallet_test"):
  build_target = 'tests-neblio-qt'
  build_target_alt = 'tests-neblio-Qt'
else:
  build_target = 'neblio-qt'
  build_target_alt = 'neblio-Qt'

# Install docker
nci.call_with_err_code('curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh && rm get-docker.sh')

# Prepare qemu
nci.call_with_err_code('docker run --rm --privileged multiarch/qemu-user-static:register --reset')

# move .ccache folder to our deploy directory
nci.call_with_err_code('mv ' + os.path.join(os.environ['HOME'],'.ccache', '') + ' ' + os.path.join(deploy_dir,'.ccache', ''))

# Start Docker Container to Build nebliod or neblio-Qt
if (os.environ.get('TRAVIS_BUILD_DIR') is not None):
  nci.call_with_err_code('timeout --signal=SIGKILL 42m sudo docker run -e BRANCH=' + os.environ['BRANCH'] + ' -e BUILD=' + build_target + ' -v ' + os.environ['BUILD_DIR'] + ':/root/vol -t neblioteam/nebliod-build-ccache-rpi')
else:
  nci.call_with_err_code('sudo docker run -e BRANCH=' + os.environ['BRANCH'] + ' -e BUILD=' + build_target + ' -v ' + os.environ['BUILD_DIR'] + ':/root/vol -t neblioteam/nebliod-build-ccache-rpi')
nci.call_with_err_code('sleep 15 && sudo docker kill $(sudo docker ps -q);exit 0')

# move .ccache folder back to travis ccache dir
nci.call_with_err_code('mv ' + os.path.join(deploy_dir,'.ccache', '') + ' ' + os.path.join(os.environ['HOME'],'.ccache', ''))

file_name = '$(date +%Y-%m-%d)---' + os.environ['BRANCH'] + '-' + os.environ['COMMIT'][:7] + '---' + build_target_alt + '---RPi-raspbian-stretch.tar.gz'

if (os.environ.get('TRAVIS_BUILD_DIR') is not None):
  # since RPi jobs are run several times we need to be creative in our deployment as only the original job can post to GitHub Releases.
  # So, start the original job via the GitHub tag and pass its job ID to every subsequent job. Once the build is complete, the final build job
  # will restart the original job, which will handle the deployment to GitHub releases.
  deploy_job_id = os.environ.get('TRAVIS_DEPLOY_JOB_ID', '0')
  travis_tag    = os.environ.get('TRAVIS_TAG', '')
  # if travis tag is populated, this is the job ID we want
  if (travis_tag != ''):
    deploy_job_id = os.environ['TRAVIS_JOB_ID']

# Check if binary exists before trying to package it.
# If it does not exist we had a build timeout
if(os.path.isfile(build_target)):
  nci.call_with_err_code('tar -zcvf "' + file_name + '" ' + build_target)
  nci.call_with_err_code('rm -f ' + build_target)
  nci.call_with_err_code('echo "Binary package at ' + deploy_dir + file_name + '"')

  # if we are just running tests, delete the deploy package
  if(os.environ['target_v'] == "rpi_wallet_test"):
    nci.call_with_err_code('rm -f ' + deploy_dir + file_name)

  if (os.environ.get('TRAVIS_BUILD_DIR') is not None and travis_tag == '' and deploy_job_id != '0'):
  	nci.call_with_err_code('echo "Restarting our deploy job. Job ID: ' + deploy_job_id + '"')
  	nci.call_with_err_code('curl -X POST -H "Content-Type: application/json" -H "Travis-API-Version: 3" -H "Accept: application/json" -H "Authorization: token ' + os.environ["TRAVIS_API_TOKEN"] + '" -d \'{}\' \'https://api.travis-ci.org/job/' + deploy_job_id + '/restart\'')

elif(os.environ.get('TRAVIS_BUILD_DIR') is not None):
  nci.call_with_err_code('echo "Binary not found, likely due to a timeout, starting new job..."')
  nci.call_with_err_code('curl -X POST -H "Content-Type: application/json" -H "Travis-API-Version: 3" -H "Accept: application/json" -H "Authorization: token ' + os.environ["TRAVIS_API_TOKEN"] + '" -d \'{"request":{"message":"RPi ' + build_target_alt + ' Build Restart","branch":"' + os.environ["TRAVIS_BRANCH"] + '","config":{"merge_mode":"deep_merge","env":{"global":{"TRAVIS_DEPLOY_JOB_ID":"' + deploy_job_id + '"},"matrix":["target_v=' + os.environ["target_v"] + '"]}}}}\' \'https://api.travis-ci.org/repo/NeblioTeam%2Fneblio/requests\'')
