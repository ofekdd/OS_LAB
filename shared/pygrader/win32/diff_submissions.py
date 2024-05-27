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
    <img src="{{ similarity_figure }}" alt="similatiry figure"/>
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

#
# Text parsing stuff
#
IDENTIFIER_REPLACEMENT = 'IDENT_REPL'
FUNC_REPLACEMENT = 'FUNC_REPL'

IDENTIFIER_REPLACEMENT_SHORT = 'I'
FUNC_REPLACEMENT_SHORT = 'F'

C_TYPE_WORDS = [
    'auto',
    'char',
    'double',
    'float',
    'int',
    'long',
    'short',
    'void'
]

C_FUNC_WORDS = [
    'kmalloc',
    'kfree',
    'malloc',
    'free'
]

C_RESERVED_WORDS = [
    'if',
    'inline',
    'break',
    'case',
    'register',
    'continue',
    'return',
    'default',
    'do',
    'sizeof',
    'static',
    'else',
    'struct',
    'entry',
    'switch',
    'extern',
    'typedef',
    'union',
    'for',
    'unsigned',
    'goto',
    'while',
    'enum',
    'const',
    'signed',
    'volatile'
] + C_TYPE_WORDS + C_FUNC_WORDS + [FUNC_REPLACEMENT]

IDENTIFIER_PATTERN = r'[a-zA-Z_]\w*'
FUNC_PATTERN = r'([a-zA-Z_]\w*\ ?)\('

IDENTIFIER_C = re.compile(IDENTIFIER_PATTERN)
FUNC_C = re.compile(FUNC_PATTERN)


def repl_user_functions(match):
    """Replace user functions with a standard text"""
    
    if match.group(1) in C_FUNC_WORDS:
        return match.group(1)

    return FUNC_REPLACEMENT+'('

    
def repl_user_identifier(match):
    """Replace user indentifiers with a standard text"""
    
    if match.group() in C_RESERVED_WORDS:
        return match.group()

    return IDENTIFIER_REPLACEMENT

    
def stripText(text):
    """Strip things that might confuse the comparisions"""

    #
    # Strip comments
    #
    text_no_comments = re.sub('//.*?(\r\n|\n)|/\*.*?\*/', '', text, count=0, flags=re.S)

    #
    # Strip all #include/#define
    #
    text_no_comments = re.sub('#include.*?(\r\n|\n)|#define.*?(\r\n|\n)', '', text_no_comments, count=0, flags=re.S)

    #
    # Remove spaces.
    #
    text_no_comments = re.sub('\s+|\r\n+|\n+', ' ', text_no_comments, count=0, flags=re.S)

    #
    # Repalce user identifiers and functions
    #
    text_no_comments = FUNC_C.sub(repl_user_functions, text_no_comments, count=0)
    text_no_comments = IDENTIFIER_C.sub(repl_user_identifier, text_no_comments, count=0)
    text_no_comments = re.sub(IDENTIFIER_REPLACEMENT, IDENTIFIER_REPLACEMENT_SHORT, text_no_comments, count=0, flags=re.S)
    text_no_comments = re.sub(FUNC_REPLACEMENT, FUNC_REPLACEMENT_SHORT, text_no_comments, count=0, flags=re.S)

    return text_no_comments
    

def calcSignature(text):
    """
    Calculate a signature for source code text
    """

    text = stripText(text)
    
    return text
    
    
def calcSimilarity(subs_signatures, zip_names, report_path):

    #
    # Grade the similarity of each submission to the rest.
    #
    dissimilarity = np.eye(len(zip_names))
    for i in range(len(subs_signatures)):
        for j in range(i+1, len(subs_signatures)):
            s = difflib.SequenceMatcher(
                    lambda x: x == " ",
                    subs_signatures[i],
                    subs_signatures[j]
                    )
            dissimilarity[i, j] = round(s.ratio(), 3)

    #
    # Draw the result
    #
    submission_names = [str(i) for i in range(len(zip_names)+1)]
    fig = plt.figure()
    ax = fig.add_subplot(111)
    img = plt.imshow(dissimilarity, interpolation="nearest")
    img.set_cmap(plt.cm.gray_r)
    ax.xaxis.set_major_locator(MaxNLocator(len(submission_names)))
    ax.yaxis.set_major_locator(MaxNLocator(len(submission_names)))
    xtickNames = plt.setp(ax, xticklabels=submission_names)
    ytickNames = plt.setp(ax, yticklabels=submission_names)
    ax.tick_params(direction='out')
    plt.setp(ytickNames, fontsize=8)
    plt.setp(xtickNames, rotation=45, fontsize=8)
    ax.xaxis.label.set_rotation(45)
    plt.grid()
    plt.colorbar()
    plt.savefig(os.path.join(report_path, SIMILARITY_FIGURE))
    

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

    return html_name, calcSignature(''.join([line for line in lines]))


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
                  
    return html_name, calcSignature(''.join(filtered_lines))


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
    
    
def diffFolders(diff_obj, report_path):
    """Traverse folders recursively diffing their content
    """

    new_files = []
    diff_files = []
    signature = ''
    
    #
    # Recursively call into sub folders
    #
    for sub_diff_obj in diff_obj.subdirs.values():
        new_files_t, diff_files_t, text_sign = diffFolders(sub_diff_obj, report_path)
        new_files += new_files_t
        diff_files += diff_files_t
        signature += text_sign

    for lo_file_name in diff_obj.left_only:
        if lo_file_name.lower().endswith('txt'):
            continue
        
        page_name, text_sign = catFile(lo_file_name, diff_obj.left, report_path)
        new_files.append(
            {
                'href': makeLocalPath(page_name),
                'caption': lo_file_name,
            }
        )

        signature += text_sign
        
    for common_file_name in diff_obj.common_files:
        page_name, text_sign = catDiff(common_file_name, diff_obj.left, diff_obj.right, report_path)
        diff_files.append(
            {
                'href': makeLocalPath(page_name),
                'caption': common_file_name,
            }
        )

        signature += text_sign

    return new_files, diff_files, signature


def main(subs_path, original_kernel_path):
    """entry point"""

    SUBMISSIONS_PATH = os.path.abspath(subs_path)
    TEMP_PATH = os.path.join(SUBMISSIONS_PATH, 'temp_diff')
    REPORT_PATH = os.path.join(SUBMISSIONS_PATH, REPORT_FOLDER)

    if os.path.exists(REPORT_PATH):
        shutil.rmtree(REPORT_PATH)

    os.mkdir(REPORT_PATH)

    subs_links = []
    subs_signatures = []
    zip_names = []
    
    for zip_name in os.listdir(SUBMISSIONS_PATH):
        if not zip_name[-3:] in ('zip', 'ZIP'):
            print 'Ignoring %s, not a zip file' % zip_name
            continue

        #
        # Remove the temp folder.
        #
        if os.path.exists(TEMP_PATH):
            shutil.rmtree(TEMP_PATH)

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
        # Extract the kernel files
        #
        kernel_path = os.path.join(TEMP_PATH, KERNEL_ARCHIVE_NAME)
        if os.path.exists(kernel_path):
            try:
                tar = tarfile.open(kernel_path)
                tar.extractall(TEMP_PATH)
                tar.close()
                os.remove(kernel_path)
            except:
                print 'Failed extracting kernel.tar.gz for file: %s' % zip_name
                continue
                
        
        try:
            #
            # Parse the zip file
            #
            submitters = parseZip(zip_path, TEMP_PATH)
    
            #
            # Start the diff.
            #
            df = filecmp.dircmp(TEMP_PATH, original_kernel_path)
            new_files, diff_files, signature = diffFolders(df, REPORT_PATH)
        except:
            print 'Failed creating signature for file: %s' % zip_name
            continue
    
        subs_signatures.append(signature)
        
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
    # Create index page
    #
    with open(os.path.join(REPORT_PATH, 'index.htm'), 'w') as report_f:
        index_tpl = Template(INDEX_TEMPLATE)
        report_f.write(
            index_tpl.render(
                navigation=subs_links,
                similarity_figure=SIMILARITY_FIGURE
                )
            )

    #
    # Create CSS page for source codes.
    #
    with open(os.path.join(REPORT_PATH, 'report.css'), 'w') as report_css:
        report_css.write(HtmlFormatter().get_style_defs('.highlight'))

    #
    # Compare submissions
    #
    calcSimilarity(subs_signatures, zip_names, REPORT_PATH)
    
    
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
    
    args = parser.parse_args()

    main(args.submissions_folder, args.kernel_folder)
