import os
from subprocess import call
import sys
import re
import multiprocessing as mp
import string
import urllib
import shutil


version = "3.4.4"

def get_qrencode_filename(ver):
    return "qrencode-" + ver + ".tar.bz2"

def get_qrencode_link(ver):
    link = "https://fukuchi.org/works/qrencode/" + get_qrencode_filename(ver)
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

def download_qrencode():
    qrencode_version_found = False
    filename_ = ""
    for ver_suffix in list(reversed(string.ascii_lowercase))+[""]:
        version_str = version + ver_suffix
        if(download_file(get_qrencode_link(version_str), get_qrencode_filename(version_str))):
            qrencode_version_found = True
            filename_ = get_qrencode_filename(version_str)
            print("Found qrencode version to be " + version_str)
            break
    if qrencode_version_found == False:
        print("Could not find the latest qrencode version. Probably you're not connected to the internet.")
        print("If you have already downloaded qrencode, put the file name in the first argument of the script.")
    return filename_

if len(sys.argv) < 2:
    filename = download_qrencode()
else:
    filename = sys.argv[1]

dirname  = filename.replace(".tar.bz2","")
try:
    shutil.rmtree(dirname)
except:
    pass

working_dir = os.getcwd()


call("tar -xf " + filename, shell=True) #extract the .tar.bz2 file

dirname_bin = dirname + "_build"
final_dirname = "qrencode_build"

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

call("./configure --enable-static --disable-shared --without-tools --disable-dependency-tracking",shell=True)
call(r"make -j" + str(mp.cpu_count()), shell=True)
call(r"sudo make install", shell=True)
print("Compilation complete.")

#Go back to base dir
os.chdir(working_dir)
################

call(r"ln -s " + dirname_bin + " " + final_dirname,shell=True)

print("")
print("QREncodecompiled to \"" + os.path.join(working_dir,final_dirname) + "\" with a soft link to \"" + os.path.join(working_dir,dirname_bin) + "\"")
print("")
print("QREncode lib path:     " + os.path.join(working_dir,final_dirname,"lib"))
print("QREncode include path: " + os.path.join(working_dir,final_dirname,"include"))
