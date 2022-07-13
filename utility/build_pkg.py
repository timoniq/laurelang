import os
import sys
from shutil import copy

pkg_names = sys.argv[1:]

ldlib = os.environ.get("LD_LIBRARY_PATH")
if ldlib:
    copy("laurelang.so", ldlib)

if not pkg_names:
    pkg_names = os.listdir("lib/")
    print("Building all library packages")

for d in pkg_names:
    path = "lib/" + d
    if not os.path.exists(path):
        print(f"{d} is not a library package. {path} undefined")
        continue
    if os.path.isdir(path):
        copy("laurelang.so", path)
        os.system("make -C " + path)

print("Completed")