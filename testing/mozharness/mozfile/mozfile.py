# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from contextlib import contextmanager
import os
import shutil
import stat
import tarfile
import tempfile
import urlparse
import urllib2
import zipfile
import time

__all__ = ['extract_tarball',
           'extract_zip',
           'extract',
           'is_url',
           'load',
           'remove',
           'rmtree',
           'tree',
           'NamedTemporaryFile',
           'TemporaryDirectory']

try:
    WindowsError
except NameError:
    WindowsError = None # so we can unconditionally catch it later...


### utilities for extracting archives

def extract_tarball(src, dest):
    """extract a .tar file"""

    bundle = tarfile.open(src)
    namelist = bundle.getnames()

    for name in namelist:
        bundle.extract(name, path=dest)
    bundle.close()
    return namelist


def extract_zip(src, dest):
    """extract a zip file"""

    if isinstance(src, zipfile.ZipFile):
        bundle = src
    else:
        try:
            bundle = zipfile.ZipFile(src)
        except Exception, e:
            print "src: %s" % src
            raise

    namelist = bundle.namelist()

    for name in namelist:
        filename = os.path.realpath(os.path.join(dest, name))
        if name.endswith('/'):
            if not os.path.isdir(filename):
                os.makedirs(filename)
        else:
            path = os.path.dirname(filename)
            if not os.path.isdir(path):
                os.makedirs(path)
            _dest = open(filename, 'wb')
            _dest.write(bundle.read(name))
            _dest.close()
        mode = bundle.getinfo(name).external_attr >> 16 & 0x1FF
        os.chmod(filename, mode)
    bundle.close()
    return namelist


def extract(src, dest=None):
    """
    Takes in a tar or zip file and extracts it to dest

    If dest is not specified, extracts to os.path.dirname(src)

    Returns the list of top level files that were extracted
    """

    assert os.path.exists(src), "'%s' does not exist" % src

    if dest is None:
        dest = os.path.dirname(src)
    elif not os.path.isdir(dest):
        os.makedirs(dest)
    assert not os.path.isfile(dest), "dest cannot be a file"

    if zipfile.is_zipfile(src):
        namelist = extract_zip(src, dest)
    elif tarfile.is_tarfile(src):
        namelist = extract_tarball(src, dest)
    else:
        raise Exception("mozfile.extract: no archive format found for '%s'" %
                        src)

    # namelist returns paths with forward slashes even in windows
    top_level_files = [os.path.join(dest, name.rstrip('/')) for name in namelist
                       if len(name.rstrip('/').split('/')) == 1]

    # namelist doesn't include folders, append these to the list
    for name in namelist:
        index = name.find('/')
        if index != -1:
            root = os.path.join(dest, name[:index])
            if root not in top_level_files:
                top_level_files.append(root)

    return top_level_files


### utilities for removal of files and directories

def rmtree(dir):
    """Deprecated wrapper method to remove a directory tree.

    Ensure to update your code to use mozfile.remove() directly

    :param dir: directory to be removed
    """

    return remove(dir)


def remove(path):
    """Removes the specified file, link, or directory tree

    This is a replacement for shutil.rmtree that works better under
    windows.

    :param path: path to be removed
    """

    def _call_with_windows_retry(func, path, retry_max=5, retry_delay=0.5):
        """
        It's possible to see spurious errors on Windows due to various things
        keeping a handle to the directory open (explorer, virus scanners, etc)
        So we try a few times if it fails with a known error.
        """
        retry_count = 0
        while True:
            try:
                func(path)
                break
            except WindowsError as e:
                # Error   5 == Access is denied
                # Error  32 == The process cannot access the file because it is
                #              being used by another process
                # Error 145 == The directory is not empty

                if retry_count == retry_max or e.winerror not in [5, 32, 145]:
                    raise
                retry_count += 1

                print 'Retrying to remove "%s" because it is in use.' % path
                time.sleep(retry_delay)

    if not os.path.exists(path):
        return

    path_stats = os.stat(path)

    if os.path.isfile(path) or os.path.islink(path):
        # Verify the file or link is read/write for the current user
        os.chmod(path, path_stats.st_mode | stat.S_IRUSR | stat.S_IWUSR)
        _call_with_windows_retry(os.remove, path)

    elif os.path.isdir(path):
        # Verify the directory is read/write/execute for the current user
        os.chmod(path, path_stats.st_mode | stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
        _call_with_windows_retry(shutil.rmtree, path)

def depth(directory):
    """returns the integer depth of a directory or path relative to '/' """

    directory = os.path.abspath(directory)
    level = 0
    while True:
        directory, remainder = os.path.split(directory)
        level += 1
        if not remainder:
            break
    return level

# ASCII delimeters
ascii_delimeters = {
    'vertical_line' : '|',
    'item_marker'   : '+',
    'last_child'    : '\\'
    }

# unicode delimiters
unicode_delimeters = {
    'vertical_line' : '│',
    'item_marker'   : '├',
    'last_child'    : '└'
    }

def tree(directory,
         item_marker=unicode_delimeters['item_marker'],
         vertical_line=unicode_delimeters['vertical_line'],
         last_child=unicode_delimeters['last_child'],
         sort_key=lambda x: x.lower()):
    """
    display tree directory structure for `directory`
    """

    retval = []
    indent = []
    last = {}
    top = depth(directory)

    for dirpath, dirnames, filenames in os.walk(directory, topdown=True):

        abspath = os.path.abspath(dirpath)
        basename = os.path.basename(abspath)
        parent = os.path.dirname(abspath)
        level = depth(abspath) - top

        # sort articles of interest
        for resource in (dirnames, filenames):
            resource[:] = sorted(resource, key=sort_key)

        files_end =  item_marker
        dirpath_marker = item_marker

        if level > len(indent):
            indent.append(vertical_line)
        indent = indent[:level]

        if dirnames:
            files_end = item_marker
            last[abspath] = dirnames[-1]
        else:
            files_end = last_child

        if last.get(parent) == os.path.basename(abspath):
            # last directory of parent
            dirpath_mark = last_child
            indent[-1] = ' '
        elif not indent:
            dirpath_mark = ''
        else:
            dirpath_mark = item_marker

        # append the directory and piece of tree structure
        # if the top-level entry directory, print as passed
        retval.append('%s%s%s'% (''.join(indent[:-1]),
                                 dirpath_mark,
                                 basename if retval else directory))
        # add the files
        if filenames:
            last_file = filenames[-1]
            retval.extend([('%s%s%s' % (''.join(indent),
                                        files_end if filename == last_file else item_marker,
                                        filename))
                                        for index, filename in enumerate(filenames)])

    return '\n'.join(retval)


### utilities for temporary resources

class NamedTemporaryFile(object):
    """
    Like tempfile.NamedTemporaryFile except it works on Windows
    in the case where you open the created file a second time.

    This behaves very similarly to tempfile.NamedTemporaryFile but may
    not behave exactly the same. For example, this function does not
    prevent fd inheritance by children.

    Example usage:

    with NamedTemporaryFile() as fh:
        fh.write(b'foobar')

        print('Filename: %s' % fh.name)

    see https://bugzilla.mozilla.org/show_bug.cgi?id=821362
    """
    def __init__(self, mode='w+b', bufsize=-1, suffix='', prefix='tmp',
                 dir=None, delete=True):

        fd, path = tempfile.mkstemp(suffix, prefix, dir, 't' in mode)
        os.close(fd)

        self.file = open(path, mode)
        self._path = path
        self._delete = delete
        self._unlinked = False

    def __getattr__(self, k):
        return getattr(self.__dict__['file'], k)

    def __iter__(self):
        return self.__dict__['file']

    def __enter__(self):
        self.file.__enter__()
        return self

    def __exit__(self, exc, value, tb):
        self.file.__exit__(exc, value, tb)
        if self.__dict__['_delete']:
            os.unlink(self.__dict__['_path'])
            self._unlinked = True

    def __del__(self):
        if self.__dict__['_unlinked']:
            return
        self.file.__exit__(None, None, None)
        if self.__dict__['_delete']:
            os.unlink(self.__dict__['_path'])


@contextmanager
def TemporaryDirectory():
    """
    create a temporary directory using tempfile.mkdtemp, and then clean it up.

    Example usage:
    with TemporaryDirectory() as tmp:
       open(os.path.join(tmp, "a_temp_file"), "w").write("data")

    """
    tempdir = tempfile.mkdtemp()
    try:
        yield tempdir
    finally:
        shutil.rmtree(tempdir)


### utilities dealing with URLs

def is_url(thing):
    """
    Return True if thing looks like a URL.
    """

    parsed = urlparse.urlparse(thing)
    if 'scheme' in parsed:
        return len(parsed.scheme) >= 2
    else:
        return len(parsed[0]) >= 2

def load(resource):
    """
    open a file or URL for reading.  If the passed resource string is not a URL,
    or begins with 'file://', return a ``file``.  Otherwise, return the
    result of urllib2.urlopen()
    """

    # handle file URLs separately due to python stdlib limitations
    if resource.startswith('file://'):
        resource = resource[len('file://'):]

    if not is_url(resource):
        # if no scheme is given, it is a file path
        return file(resource)

    return urllib2.urlopen(resource)

