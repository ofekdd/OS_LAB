hwgrader
========
A package for writing test for linux_kernel course. This package
contains several modules that simplify writing tests.

To install the package enter:

 python setup.py install

Usage:
------
The package includes a script to help in building the custom kernel.
The script is called 'make_changed.py'. It is useful as it copies
some modified kernel files that are necessary for logging of memory
allocation and release. It can also make a backup copy of the custom
kernel. To use it enter:

    > make_changed.py <path to your submission files>

Using the `-r` flag will reset the custom kernel files using the backup
copy:

    > make_changed.py -r 

Using the `-s` flag will skip copying the custom kernel which can
save you time:

    > make_changed.py -s <path to your submission files>

Writing tests:
--------------
The testing mechanism is based on the [unittest][1] package of python
(see link for documentation).
You can base your test on the examples under the folder tests/
To run the test enter:
 
    > python <path to test>

e.g.:

    > python tests/2011w/hw3/hw3_test.py

Important Note:
---------------
The build_submission.sh shell script that compiles and installs the
submission assumes the following structure:

    submission.zip
        |
        +-> [sS]ubmitters.txt
        +-> <all source files>

[1]: http://docs.python.org/release/2.2.1/lib/module-unittest.html
