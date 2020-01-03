import os
from subprocess import call
import sys
import errno
import urllib


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


def call_with_err_code(cmd):
    err_code = call(cmd, shell=True)
    # Error code 137 is thrown by the timeout command when it timesout, used in RPi building
    if (err_code != 0 and err_code != 137):
        print("")
        print("")
        sys.stderr.write('call \'' + cmd + '\' exited with error code ' + str(err_code) + ' \n')
        print("")
        exit(err_code)


def call_retry_on_fail(cmd):
    err_code = call(cmd, shell=True)
    # Error code 137 is thrown by the timeout command when it timesout, used in RPi building
    if (err_code != 0 and err_code != 137):
        print("")
        print("")
        sys.stderr.write('call \'' + cmd + '\' failed with error code ' + str(err_code) + '. RETRYING...\n')
        print("")
        call_retry_on_fail(cmd)


def install_packages_debian(packages_to_install):
    call_with_err_code('sudo apt-get update')
    if len(packages_to_install) > 0:
        call_with_err_code('sudo apt-get -y install ' + " ".join(packages_to_install))


def install_packages_osx(packages_to_install):
    call_with_err_code('sudo brew update')
    call_with_err_code('sudo brew -y install ' + " ".join(packages_to_install))

def setup_travis_or_gh_actions_env_vars():
	if os.environ.get('TRAVIS_BUILD_DIR') is not None:
		# Travis Detected
		print("Travis CI Detected. Setting Up Environment Variables.")
		os.environ['BUILD_DIR'] = os.environ.get('TRAVIS_BUILD_DIR')
		os.environ['BRANCH'] = os.environ.get('TRAVIS_BRANCH')
		os.environ['COMMIT'] = os.environ.get('TRAVIS_COMMIT')
	elif os.environ.get('GITHUB_ACTIONS') is not None:
		# GitHub Actions Detected
		print("GitHub Actions Detected. Setting Up Environment Variables.")
		os.environ['BUILD_DIR'] = os.environ['GITHUB_WORKSPACE']
		os.environ['BRANCH'] = os.environ['GITHUB_REF'].rsplit('/', 1)[1]
		os.environ['COMMIT'] = os.environ.get('GITHUB_SHA')
		os.environ['CCACHE_DIR'] = os.path.join(os.environ['GITHUB_WORKSPACE'],'.ccache')
		os.environ['CCACHE_COMPRESS'] = "1"
		os.environ['CCACHE_COMPRESSLEVEL'] = "9"
		os.environ['CCACHE_MAXSIZE'] = "150M"
		os.environ['CPATH'] = '/Applications/Xcode_11.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include'


	else:
		print("Neither Travis CI nor GitHub Actions Detected. Aborting...")
		exit(1)

	print("BUILD_DIR: " + os.environ['BUILD_DIR'])
	print("BRANCH: "    + os.environ['BRANCH'])
	print("COMMIT: "    + os.environ['COMMIT'])

