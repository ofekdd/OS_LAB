#!/usr/bin/python


from __future__ import division
import shutil
import getopt
import sys
import os
from hwgrader import *


TEMP_FILES_FOLDER = os.path.expanduser('~/temp_make')
CUSTOM_KERNEL_PATH = '/usr/src/linux-2.4.18-14custom'
CUSTOM_KERNEL_BAKCUP_PATH = os.path.join(TEMP_FILES_FOLDER, os.path.split(CUSTOM_KERNEL_PATH)[-1])


def usage():
    """Print usage details"""
    
    usage_doc = """
Usage: """ + os.path.basename(sys.argv[0]) + """ [options] path_to_modified_sources_folder

Compile a modified kernel based on modified sources.
Options:
  -h, --help        show this help message and exit
  -r, --reset       reset the custom kernel files
  -s, --skip        skip backup of the custom kernel (useful when previous compilation failed)
  -n, --no_modules  skip compilation of kernel modules (useful when no header files changed)
  -e, --extra-mods  include extra kernel modifications in /etc (slows down compilation)
"""
    print usage_doc
    

def reset_kernel():
    
    #
    # Clean up
    #
    print 'performing cleanup'
    if os.path.exists(CUSTOM_KERNEL_BAKCUP_PATH):
        if os.path.exists(CUSTOM_KERNEL_PATH):
            shutil.rmtree(CUSTOM_KERNEL_PATH)
        shutil.os.rename(CUSTOM_KERNEL_BAKCUP_PATH, CUSTOM_KERNEL_PATH)

    if os.path.exists(TEMP_FILES_FOLDER):
        shutil.rmtree(TEMP_FILES_FOLDER)

    
def main():
    """main"""

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hrsne", ["help", "reset", "skip", "no_modules", "extra_mods"])
    except getopt.GetoptError:
        # print help information and exit:
        usage()
        sys.exit(2)

    skip = False
    no_modules = False
    extra_mods = False
    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        if o in ("-r", "--reset"):
            reset_kernel()
            return
        if o in ("-s", "--skip"):
            skip = True
        if o in ("-n", "--no_modules"):
            no_modules = True
        if o in ("-e", "--extra_mods"):
            extra_mods = True

    if not args:
        usage()
        sys.exit(2)
    src_path = os.path.abspath(args[0])

    if not skip:
        #
        # Make a copy of the custom kernel
        #
        print 'Making a backup of the custom kernel'
        if not os.path.exists(TEMP_FILES_FOLDER):
            os.makedirs(TEMP_FILES_FOLDER)
            shutil.os.rename(CUSTOM_KERNEL_PATH, CUSTOM_KERNEL_BAKCUP_PATH)
    
        print 'Making a clean copy of the custom kernel'
        if os.path.exists(CUSTOM_KERNEL_PATH):
            shutil.rmtree(CUSTOM_KERNEL_PATH)
        shutil.copytree(CUSTOM_KERNEL_BAKCUP_PATH, CUSTOM_KERNEL_PATH)

    if extra_mods:
        #
        # Handle the compiling and loading of the new kernel
        #
        # Copy custom modificaions of the grader (memory tracking)
        # This slows down compilation dramatically and is rarely useful, so we
        # don't do it by default
        os.system('cp -rf %s %s' % (os.path.join(BASE_INSTALL_PATH, KERNEL_MODIFICATIONS_FOLDER, '*'), CUSTOM_KERNEL_PATH))

    print 'Start of compilation'
    if no_modules:
        status = os.system(BUILD_NM_SCRIPT + " " + src_path)        
    else:
        status = os.system(BUILD_SCRIPT + " " + src_path)

    if status:
        raise Exception('Failed building the submission, exit with status: %d' % (status/256))


if __name__ == '__main__':
    main()
    
