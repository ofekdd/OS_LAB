#!/usr/bin/env python
"""
To run the script call:

> diff_submissions.py [--kernel_folder <path to original kernel>] <path to archived submissions>

The --kernel_folder is optional. If not used the script will default
to the global variable REPO_PATH below.

Should create a folder 'html_diff' inside the path to submissions. The
folder contains an index.htm that should be open using a browser.

Requirements:
    The script requires a large bunch of python pacakges. As a student
    the easiest way to get them is to request an free academic license
    from:
    http://www.enthought.com/products/edudownload.php
    or download:
    http://code.google.com/p/pythonxy/

Author: Amit Aides
        amitibo@tx.technion.ac.il
"""

from __future__ import division
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import difflib
import filecmp
import argparse 
import os
import os.path
import shutil
import zipfile
import tarfile
import glob
from jinja2 import Template
import tempfile
from pygments import highlight
from pygments.lexers import CLexer
from pygments.formatters import HtmlFormatter
import re
import subprocess as sbp
import traceback
import sys
import time


REPO_PATH = r'D:\amit\teaching\linux_kernel\files\linux-2.4.18-14custom_base'
SIMILARITY_FIGURE = 'similarity.png'
KERNEL_ARCHIVE_NAME = 'kernel.tar.gz'

#
# HTML stuff
#
REPORT_FOLDER = 'html_diff'
INDEX_TEMPLATE = """
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html lang="en">
<head>
    <title>Submissions</title>
</head>
<body>
    <h1>Submissions</h1>
    <ol id="navigation" type="1">
    {% for item in navigation %}
        <li><a href="{{ item.href }}">{{ item.caption }}</a></li>
    {% endfor %}
    </ol>
    <h1>Submissions Similarity</h1>
    <table border="1">
    <tr>
        <th></th>
        {% for i in similarity_table %}
        <th>{{ loop.index }}</th>
        {% endfor %}
    </tr>
    {% for row in similarity_table %}
    <tr>
        <td>{{ loop.index }}</td>
        {% for item in row %}
        <td bgcolor="#{{ item.bgcolor }}"><a href="{{ item.href }}">{{ item.score }}</a></td>
        {% endfor %}
    </tr>
    {% endfor %}    
    </table>
</body>
</html>
"""

SUBMISSION_TEMPLATE = """
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html lang="en">
<head>
    <title>{{ submission }}</title>
</head>
<body>
    <h1>{{ submission }}</h1>
    <h2>{{ submitters }}</h2>
    <h3>New Files</h3>
    <ul id="navigation">
    {% for item in navigation_new %}
        <li><a href="{{ item.href }}">{{ item.caption }}</a></li>
    {% endfor %}
    </ul>
    <h3>Changed Files</h3>
    <ul id="navigation">
    {% for item in navigation_diff %}
        <li><a href="{{ item.href }}">{{ item.caption }}</a></li>
    {% endfor %}
    </ul>
</body>
</html>
"""

NEWFILE_TEMPLATE = """
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html lang="en">
<head>
    <title>{{ file_name }}</title>
    <link rel="stylesheet" type="text/css" href="report.css" />
</head>
<body>
    {{ code }}
</body>
</html>
"""

COMP_TEMPLATE = """
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html lang="en">
<head>
    <title>{{ title }}</title>
</head>
<body>
    {{ content }}
</body>
</html>
"""

COMP_REPORT = re.compile(r'(.*) consists for (\d+) \% of (.*) material')


def folderSize(folder):
    size_sum = 0
    for f in os.listdir(folder):
        abs_path = os.path.join(folder, f)
        if os.path.isfile(abs_path):
            size_sum += os.path.getsize(abs_path)
            
    return size_sum


def folderSimilarity(temp_folder1, temp_folder2, report_path):
    p = sbp.Popen(["sim_c", '-e', '-p', '-R', temp_folder1, temp_folder2], shell=True, stdin=sbp.PIPE, stdout=sbp.PIPE)
    report_lines = p.stdout.readlines()
    
    size_sum = folderSize(temp_folder1) + folderSize(temp_folder2)
    
    percent = 0
    filtered_lines = []
    for line in report_lines:
        res = COMP_REPORT.search(line.strip())
        if not res:
            continue
        filtered_lines.append(line)
        
        file_size = os.path.getsize(res.group(1))
        percent += int(res.group(2)) * file_size
    
    with tmpHtmlFile(report_path) as report_f:
        html_name = report_f.name
        tpl = Template(COMP_TEMPLATE)
        try:
            content = '<br>'.join(filtered_lines).decode('utf8').encode('ascii', 'ignore')
        except:
            content = '<br>'.join(filtered_lines).decode('latin').encode('ascii', 'ignore')
            
        report_f.write(
            tpl.render(
                title='comparision',
                content=content
                )
            )
        
    return (percent / size_sum) / 100, html_name


def calcSimilarity(filtered_folders, zip_names, report_path):

    #
    # Grade the similarity of each submission to the rest.
    #
    similarity_table = []
    for i in range(len(filtered_folders)):
        similarity_row = [
            {
                'score': 0,
                'href': '',
                'bgcolor': 'FFFFFF'
            } for j in range(len(filtered_folders))]

        for j in range(i+1, len(filtered_folders)):
            score, page_name = folderSimilarity(filtered_folders[i], filtered_folders[j], report_path)
            similarity_row[j] = {
                'score': round(score, 1),
                'href': makeLocalPath(page_name),
                'bgcolor': ('0'+hex(int((1-score)*255))[2:])[-2:]*3
            }
            
        similarity_table.append(similarity_row)
    
    return similarity_table


def tmpHtmlFile(dir_path):
    """
    Create a .htm file with random name
    """

    #
    # Create a temporary file for logging the folder
    #
    fd, filename = tempfile.mkstemp(suffix='.htm', dir=dir_path)
    os.close(fd)
    f = open(filename, 'w')
    return f


def catFile(file_name, base_path, report_path):
    """
    Create an HTML page for a source file.
    """
    
    file_path = os.path.join(base_path, file_name)
    with open(file_path, 'r') as f:
        lines = f.readlines()

    with tmpHtmlFile(report_path) as report_f:
        html_name = report_f.name
        tpl = Template(NEWFILE_TEMPLATE)
        try:
            text = ''.join(lines).decode('utf8').encode('ascii', 'ignore')
        except:
            text = ''.join(lines).decode('latin').encode('ascii', 'ignore')
            
        report_f.write(
            tpl.render(
                file_name=file_name,
                code=highlight(text, CLexer(), HtmlFormatter())
                )
            )

    return html_name


def filterDiff(line):
    """Filter the unified diff output from unwanted lines
    """
    
    return not line.startswith('-') and not line.startswith('---') and not line.startswith('+++') and not line.startswith('@@')

    
def catDiff(file_name, left_path, right_path, report_path):
    """
    Create an HTML diff page.
    """

    left_file_path = os.path.join(left_path, file_name)
    with open(left_file_path, 'r') as f:
        left_lines = f.readlines()

    right_file_path = os.path.join(right_path, file_name)
    with open(right_file_path, 'r') as f:
        right_lines = f.readlines()

    with tmpHtmlFile(report_path) as report_f:
        html_name = report_f.name
        report_f.write(difflib.HtmlDiff().make_file(right_lines, left_lines, right_file_path, left_file_path))

    diff_lines = difflib.unified_diff(right_lines, left_lines, n=0, lineterm='\n')
    filtered_lines = [line[1:] for line in diff_lines if filterDiff(line)]
                  
    return html_name, ''.join(filtered_lines)


def parseZip(zip_path, temp_path):
    """
    Parse the zip file for basic data like names etc.
    """

    glob_result = glob.glob(os.path.join(temp_path, '[sS]ubmitters.txt'))
    
    if not glob_result:
        submitters = 'Missing submitters file.'
    else:
        with open(glob_result[0], 'rb') as sub_f:
            sub_data = sub_f.readlines()

        submitters = [line.strip() for line in sub_data]
        submitters = ' & '.join([line for line in submitters if line != ''])

    try:
        submitters = submitters.decode('utf8').encode('ascii', 'ignore')
    except:
        submitters = submitters.decode('latin').encode('ascii', 'ignore')

    return submitters


def makeLocalPath(html_path):
    """Leave only the local name of the html page"""

    return os.path.join('.', os.path.split(html_path)[1])
    
    
def diffFolders(diff_obj, filtered_folder, report_path, filtered_files):
    """Traverse folders recursively diffing their content
    """

    new_files = []
    diff_files = []
    
    #
    # Recursively call into COMMON sub folders
    #
    for sub_diff_obj in diff_obj.subdirs.values():
        new_files_t, diff_files_t = diffFolders(sub_diff_obj, filtered_folder, report_path, filtered_files)
        new_files += new_files_t
        diff_files += diff_files_t

    for lo_file_name in diff_obj.left_only:
        if lo_file_name.lower().endswith('txt') or lo_file_name in filtered_files:
            continue
        
        if os.path.isdir(lo_file_name):
            new_files_t, diff_files_t = diffFolders(sub_diff_obj, filtered_folder, report_path, filtered_files)
            new_files += new_files_t
            diff_files += diff_files_t
        else:
            shutil.copy(os.path.join(os.path.abspath(diff_obj.left), lo_file_name), filtered_folder)
            
            page_name = catFile(lo_file_name, diff_obj.left, report_path)
            new_files.append(
                {
                    'href': makeLocalPath(page_name),
                    'caption': lo_file_name,
                }
            )
        
    for common_file_name in diff_obj.common_files:
        if common_file_name in filtered_files:
            continue

        with open(os.path.join(filtered_folder, common_file_name), 'w') as f:
            page_name, diff_text = catDiff(common_file_name, diff_obj.left, diff_obj.right, report_path)
            f.write(diff_text)

        diff_files.append(
            {
                'href': makeLocalPath(page_name),
                'caption': common_file_name,
            }
        )

    return new_files, diff_files


def main(subs_path, original_kernel_path, filtered_files):
    """entry point"""

    SUBMISSIONS_PATH = os.path.abspath(subs_path)
    TEMP_PATH = os.path.join(SUBMISSIONS_PATH, 'temp_diff')
    REPORT_PATH = os.path.join(SUBMISSIONS_PATH, REPORT_FOLDER)
    
    if os.path.exists(REPORT_PATH):
        shutil.rmtree(REPORT_PATH)

    time.sleep(1)
    os.mkdir(REPORT_PATH)

    subs_links = []
    filtered_folders = []
    zip_names = []
    
    for zip_name in os.listdir(SUBMISSIONS_PATH):
        if not zip_name[-3:] in ('zip', 'ZIP'):
            print 'Ignoring %s, not a zip file' % zip_name
            continue

        print 'processing submission:', zip_name

        #
        # Remove the temp folder.
        #
        if os.path.exists(TEMP_PATH):
            shutil.rmtree(TEMP_PATH, ignore_errors=True)

        #
        # Extract the zip file.
        #
        try:
            zip_path = os.path.join(SUBMISSIONS_PATH, zip_name)
            zf = zipfile.ZipFile(zip_path)
            zf.extractall(TEMP_PATH)
        except:
            print 'Failed extracting zip file: %s' % zip_name
            continue

        #
        # Check if the students sent a zip of a folder (instead of zipping the folder contents.)
        #
        temp_zip_content = os.listdir(TEMP_PATH)
        if len(temp_zip_content) == 1:
            if os.path.isdir(os.path.join(TEMP_PATH, temp_zip_content[0])):
                raise Exception('Zip file: %s has the wrong folder structure' % zip_name)

        #
        # Extract the kernel files
        #
        kernel_path = os.path.join(TEMP_PATH, KERNEL_ARCHIVE_NAME)
        if os.path.exists(kernel_path):
            try:
                tar = tarfile.open(kernel_path)
                tar.extractall(TEMP_PATH)
                tar.close()
                os.remove(kernel_path)
            except Exception, err:
                print 'Failed extracting kernel.tar.gz for file: %s' % zip_name
                print(traceback.format_exc())
                continue
            
        filtered_folder = tempfile.mkdtemp()
        
        #
        # Parse the zip file
        #
        submitters = parseZip(zip_path, TEMP_PATH)
        
        #
        # Start the diff.
        #
        df = filecmp.dircmp(TEMP_PATH, original_kernel_path)
        new_files, diff_files = diffFolders(df, filtered_folder, REPORT_PATH, filtered_files)
        
        filtered_folders.append(filtered_folder)
        
        #
        # Log the files list.
        #
        with tmpHtmlFile(REPORT_PATH) as subs_f:
            page_name = subs_f.name
            tpl = Template(SUBMISSION_TEMPLATE)
            subs_f.write(
                tpl.render(
                    submission=zip_name,
                    submitters=submitters,
                    navigation_new=new_files,
                    navigation_diff=diff_files
                    )
                )

        #
        # Log for the main index
        #
        subs_links.append(
            {
                'href': makeLocalPath(page_name),
                'caption': zip_name
            }
        )

        #
        # Update the list of proccessed zip files.
        #
        zip_names.append(zip_name)

    #
    # Create CSS page for source codes.
    #
    with open(os.path.join(REPORT_PATH, 'report.css'), 'w') as report_css:
        report_css.write(HtmlFormatter().get_style_defs('.highlight'))

    #
    # Compare submissions
    #
    similarity_table = calcSimilarity(filtered_folders, zip_names, REPORT_PATH)
    
    #
    # Create index page
    #
    with open(os.path.join(REPORT_PATH, 'index.htm'), 'w') as report_f:
        index_tpl = Template(INDEX_TEMPLATE)
        report_f.write(
            index_tpl.render(
                navigation=subs_links,
                similarity_table=similarity_table
                )
            )

    
if __name__=='__main__':

    parser = argparse.ArgumentParser(description='Create a source diff report for all submissions.')
    parser.add_argument(
        'submissions_folder',
        type=str,
        help='path to submissions folder'
        )
    
    parser.add_argument(
        '--kernel_folder',
        type=str,
        help='path to original kernel',
        default=REPO_PATH
        )
    
    parser.add_argument(
        '--filter',
        type=str,
        help='list of files to ignore (encolse in parentheses to pass several file names)',
        default=''
        )
    
    args = parser.parse_args()
    filtered_files = args.filter.split()
    main(args.submissions_folder, args.kernel_folder, filtered_files)
