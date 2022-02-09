#!/usr/local/bin python3

import os

DEFAULT_LIB_PATH = "/usr/local/opt/laurelang"

def make(
    lib_path: str = DEFAULT_LIB_PATH,
    build_packages: bool = True,
    install: bool = True,
):
    if os.system(f'make LIBFLAG=-Dlib_path=\'\\\"{lib_path}/lib\\\"\'') != 0:
        print("Error building sources :(")
        return
    
    if build_packages:
        r = os.system("make packages")
        if r != 0:
            print("Failed to build packages :(")
    
    if install:
        os.system("make install")
    

def parse_answer(s: str) -> bool:
    if not s or s.lower() == "y":
        return True
    elif s.lower() == "n":
        return False
    return parse_answer(input("Sorry, unable to understand. y/n/quit [y is default]: "))


if __name__ == "__main__":
    print("[laurelang builder] Option in [brackets] is default, leave input empty to use it")
    lib_path = input(f"Store library at [{DEFAULT_LIB_PATH}]/{{custom}}: ") or DEFAULT_LIB_PATH
    build_packages = parse_answer(input("Should packages be built? [y]/n: "))
    install = parse_answer(input("Install laurelang to system? [y]/n: "))
    if parse_answer(input("Ok, library installation path: {}\n    build packages: {}\n    install to system: {}\nIs everything right? [y]/n: ".format(lib_path, build_packages, install))):
        make(lib_path, build_packages, install)
        print("Done")
    else:
        print("Cancelled")
