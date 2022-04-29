import os
from shutil import copy

ldlib = os.environ.get("LD_LIBRARY_PATH")
if ldlib:
    copy("laurelang.so", ldlib)

for dir in os.listdir("lib"):
    path = "lib/" + dir
    if os.path.isdir(path):
        os.system("make -C " + path)
