#!/usr/bin/python

"""
Copy changes of a repository (workcopy to root of repo)

Author: Amit Aides
"""
from __future__ import division
import argparse
import shutil
import glob
import re
import os


def main(submissions_folder):
    """main"""

    arch_list = glob.glob(os.path.join(submissions_folder, '*.zip'))

    for path in arch_list:
        file_name = os.path.split(path)[1]
        m = re.search(r'.*?_([0-9]+_assignsubmission_file_.*)', file_name)
        if m is not None:
            new_path = os.path.join(submissions_folder, m.group(1))
            os.rename(path, new_path)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Rename moodle submissions to remove hebrew letters.')
    parser.add_argument(
        'submissions_folder',
        type=str,
        help='path to submissions folder'
        )
    
    args = parser.parse_args()

    main(args.submissions_folder)    
