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
nci.call_with_err_code('timeout --signal=SIGKILL 2m sudo docker run -e BRANCH=' + os.environ['TRAVIS_BRANCH'] + ' -v ' + deploy_dir + ':/root/deploy -t neblioteam/nebliod-build-ccache-rpi')
nci.call_with_err_code('sleep 15 && sudo docker kill $(sudo docker ps -q);exit 0')

# move .ccache folder back to travis ccache dir
nci.call_with_err_code('mv ' + os.path.join(deploy_dir,'.ccache', '') + ' ' + os.path.join(os.environ['HOME'],'.ccache', ''))

file_name = '$(date +%Y-%m-%d)---' + os.environ['TRAVIS_BRANCH'] + '-' + os.environ['TRAVIS_COMMIT'][:7] + '---RPi-neblio-Qt-nebliod---raspbian-stretch.tar.gz'

# since RPi jobs are run several times we need to be creative in our deployment as only the original job can post to GitHub Releases.
# So, start the original job via the GitHub tag and pass its job ID to every subsequent job. Once the build is complete, the final build job
# will restart the original job, which will handle the deployment to GitHub releases.
deploy_job_id = os.eviron.get('TRAVIS_DEPLOY_JOB_ID', '0')
travis_tag    = os.eviron.get('TRAVIS_TAG', '')
# if travis tag is populated, this is the job ID we want
if (travis_tag != '') {
	deploy_job_id = os.eviron['TRAVIS_JOB_ID']
}
print('Debug: Job ID: ' + deploy_job_id)

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

  nci.call_with_err_code('echo "Binaries neblio-qt and nebliod not found, likely due to a timeout, restarting job..."')
  nci.call_with_err_code('curl -X POST -H "Content-Type: application/json" -H "Travis-API-Version: 3" -H "Accept: application/json" -H "Authorization: token ' + os.environ["TRAVIS_API_TOKEN"] + '" -d \'{"request":{"message":"RPi Build Restart","branch":"' + os.environ["TRAVIS_BRANCH"] + '","config":{"merge_mode":"deep_merge","env":{"global":{"TRAVIS_DEPLOY_JOB_ID":"' + deploy_job_id + '"},"matrix":["target_v=rpi_daemon_wallet"]}}}}\' \'https://api.travis-ci.org/repo/NeblioTeam%2Fneblio/requests\'')