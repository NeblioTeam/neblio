import os
from subprocess import call
import sys
import re
import multiprocessing as mp
import string
import shutil
import requests


version = "1_65_1"

def get_boost_filename(ver):
    return "boost_" + ver + ".tar.gz"

def get_boost_link(ver):
    dot_ver = ver.replace('_', '.')
    link = "https://dl.bintray.com/boostorg/release/" + dot_ver + "/source/" + get_boost_filename(ver)
    print(link)
    return link

def download_file(filelink, target):
    try:
        testfile = requests.get(filelink, allow_redirects=True)
        try:
            os.remove(target)
            print("Found file " + target + ", which is now deleted.")
        except:
            pass
        open(target, 'wb').write(testfile.content)
        return True
    except:
        return False

def download_boost():
    boost_version_found = False
    filename_ = ""
    if(download_file(get_boost_link(version), get_boost_filename(version))):
        boost_version_found = True
        filename_ = get_boost_filename(version)
        print("Found boost version to be " + version)
    if boost_version_found == False:
        print("Could not find the latest boost version. Probably you're not connected to the internet.")
        print("If you have already downloaded boost, put the file name in the first argument of the script.")
    return filename_

if len(sys.argv) < 2:
    filename = download_boost()
else:
    filename = sys.argv[1]

dirname  = "boost_" + version
try:
    shutil.rmtree(dirname)
except:
    pass

working_dir = os.getcwd()


call("tar -xf " + filename, shell=True) #extract the .tar.gz file

dirname_bin = dirname + "_build"
final_dirname = "boost_build"

try:
    shutil.rmtree(dirname_bin)
except:
    pass

try:
    shutil.rmtree(final_dirname)
except:
    pass

#Go back to base dir
os.chdir(working_dir)
################

os.chdir(dirname)

# prepend ccache to the path, necessary since prior steps prepend things to the path
os.environ['PATH'] = '/usr/lib/ccache:' + os.environ['PATH']

call("./bootstrap.sh",shell=True)
call("./b2 -j" + str(mp.cpu_count()), shell=True)
call("sudo ./b2 -j" + str(mp.cpu_count()) + " install", shell=True)
print("Compilation complete.")

#Go back to base dir
os.chdir(working_dir)
################

call(r"ln -s " + dirname_bin + " " + final_dirname,shell=True)

print("")
print("boost compiled to \"" + os.path.join(working_dir,final_dirname) + "\" with a soft link to \"" + os.path.join(working_dir,dirname_bin) + "\"")
print("")
print("boost lib path:     " + os.path.join(working_dir,final_dirname,"lib"))
print("boost include path: " + os.path.join(working_dir,final_dirname,"include"))
