import os
from subprocess import call
import sys
import re
import multiprocessing as mp
import string
import urllib
import shutil


version = "1.0.18-stable"

def get_libsodium_filename(ver):
    return "libsodium-" + ver + ".tar.gz"

def get_libsodium_link(ver):
    link = "https://download.libsodium.org/libsodium/releases/" + get_libsodium_filename(ver)
#    print(link)
    return link

def download_file(filelink, target):
    try:
        testfile = urllib.URLopener()
        try:
            os.remove(target)
            print("Found file " + target + ", which is now deleted.")
        except:
            pass
        testfile.retrieve(filelink, target)
        return True
    except:
        return False

def download_libsodium():
    libsodium_version_found = False
    filename_ = ""
    if(download_file(get_libsodium_link(version), get_libsodium_filename(version))):
        libsodium_version_found = True
        filename_ = get_libsodium_filename(version)
        print("Found libsodium version to be " + version)
    if libsodium_version_found == False:
        print("Could not find the latest libsodium version. Probably you're not connected to the internet.")
        print("If you have already downloaded libsodium, put the file name in the first argument of the script.")
    return filename_

if len(sys.argv) < 2:
    filename = download_libsodium()
else:
    filename = sys.argv[1]

dirname  = "libsodium-stable"
try:
    shutil.rmtree(dirname)
except:
    pass

working_dir = os.getcwd()


call("tar -xf " + filename, shell=True) #extract the .tar.gz file

dirname_bin = dirname + "_build"
final_dirname = "libsodium_build"

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

call("./configure",shell=True)
call("make -j" + str(mp.cpu_count()) + " && make check", shell=True)
call("sudo make install", shell=True)
print("Compilation complete.")

#Go back to base dir
os.chdir(working_dir)
################

call(r"ln -s " + dirname_bin + " " + final_dirname,shell=True)

print("")
print("libsodium compiled to \"" + os.path.join(working_dir,final_dirname) + "\" with a soft link to \"" + os.path.join(working_dir,dirname_bin) + "\"")
print("")
print("libsodium lib path:     " + os.path.join(working_dir,final_dirname,"lib"))
print("libsodium include path: " + os.path.join(working_dir,final_dirname,"include"))
