import os
from subprocess import call
import sys
import re
import multiprocessing as mp
import string
import urllib
import shutil

configure_flags = "no-shared"
cflags = "-fPIC"

base_openssl_version = "1.1.1"

def get_openssl_filename(ver):
    return "openssl-" + ver + ".tar.gz"

def get_openssl_link(ver):
    link = "https://www.openssl.org/source/" + get_openssl_filename(ver)
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

def download_openssl():
    openssl_version_found = False
    filename_ = ""
    for ver_suffix in list(reversed(string.ascii_lowercase))+[""]:
        version_str = base_openssl_version + ver_suffix
        if(download_file(get_openssl_link(version_str), get_openssl_filename(version_str))):
            openssl_version_found = True
            filename_ = get_openssl_filename(version_str)
            print("Found latest OpenSSL version to be " + version_str)
            break
    if openssl_version_found == False:
        print("Could not find the latest OpenSSL version. Probably you're not connected to the internet.")
        print("If you have already downloaded OpenSSL, put the file name in the first argument of the script.")
    return filename_

if len(sys.argv) < 2:
    filename = download_openssl()
else:
    filename = sys.argv[1]

dirname  = filename.replace(".tar.gz","")
try:
    shutil.rmtree(dirname)
except:
    pass

working_dir = os.getcwd()

if not bool(re.match("(openssl-){1}(\d)+(.)(\d)+(.)(\d)+(\w)*(.tar.gz)",filename)):
    print("The file given '" + filename + "' doesn't seem to be an openssl source file. It must be in the form: openssl-x.y.zw.tar.gz")
    exit(1)


call("tar -xf " + filename, shell=True) #extract the .tar.gz file

dirname_bin = dirname + "_build"
final_dirname = "openssl_build"

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

call("CFLAGS=" + cflags + " ./config " + configure_flags + " --prefix=" + os.path.join(working_dir,dirname_bin) + " " + configure_flags,shell=True)
call(r"make -j" + str(mp.cpu_count()), shell=True)
call(r"make install", shell=True)
print("Compilation complete.")

#Go back to base dir
os.chdir(working_dir)
################

call(r"ln -s " + dirname_bin + " " + final_dirname,shell=True)

print("")
print("OpenSSL compiled to \"" + os.path.join(working_dir,final_dirname) + "\" with a soft link to \"" + os.path.join(working_dir,dirname_bin) + "\"")
print("")
print("OpenSSL lib path:     " + os.path.join(working_dir,final_dirname,"lib"))
print("OpenSSL include path: " + os.path.join(working_dir,final_dirname,"include"))
