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
  nci.call_with_err_code('curl -X POST -H "Content-Type: application/json" -H "Travis-API-Version: 3" -H "Accept: application/json" -H "Authorization: token ' + os.environ["TRAVIS_API_TOKEN"] + '" -d \'{"request":{"message":"RPi Build Restart","branch":"' + os.environ["TRAVIS_BRANCH"] + '","config":{"env":{"global":["CCACHE_MAXSIZE=1G",{"secure":"hi4pDxnjHCeb75FeQUjMQCdE+UwCOO6CqOnkFTGLM8uroBcs9YChW2UJUKqMwEeK+s0kCBDe5GvlFc3joDclX/aEMTBvOECUd8L8Y8+tEknZaNvk7Xl+EI+Rror33gnr4XYpJ4qQbubkM/aMnH5fYJFA6lCDbD/mztizw9Gc1kWZc6+px7liifUOmPrL5CuF9S5tgB7C2erZoZG0pxPSWx16yKyTzDzoMVIUjwubeDf7luMrTT8mkaMFl0ZLq5/+joEMBDjtRMCfS0ap9YnAbbcoY5xpelAZYY5c+6LjUIKng+r8hUtHabCBoFdcboRpeBymG5TUufBjr3lChxfnnVsch02Zu+r0CLkpd9P0oGpohUXn5iZRfSSo/Rb2t2swIZp4Cd8uabie0l0BVL8yTf/bvv6LWZ7BZQsrj22z+dxwIJGCDlsKS4zqmvlSsXzSOIZMLvrfTRUtYlQzC2tLs9UrPzgCS6PzQixmUk4vK3u6m9+i5eI7B6rDzkkEnDgfyr04aI2EE5zakzUYl/mOKYQHCDbq0xE+s29rzTGytA0qq06Fw4HNF1HOxv+27bPAQxBnlo5XCVb+v7XEVHP+dmjkzcx5DusbqkCctCOnyLYMMobPLnRI64XCpfF7zHUkonwYkag8vObf2D3jCxijAvVW3IpvcXbdkH7NXzQxW1A="}],"matrix":["target_v=rpi_daemon_wallet"]}}}}\' \'https://api.travis-ci.org/repo/NeblioTeam%2Fneblio/requests\'')