#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Imports a test suite from a remote repository. Takes one argument, a file in
the format described in README.
Note: removes both source and destination directory before starting. Do not
      use with outstanding changes in either directory.
"""

from __future__ import print_function, unicode_literals

import os
import shutil
import subprocess
import sys

import parseManifest
import writeBuildFiles

def readManifests(iden, dirs):
    def parseManifestFile(iden, path):
        pathstr = "hg-%s/%s/MANIFEST" % (iden, path)
        subdirs, mochitests, reftests, _, supportfiles = parseManifest.parseManifestFile(pathstr)
        return subdirs, mochitests, reftests, supportfiles

    data = []
    for path in dirs:
        subdirs, mochitests, reftests, supportfiles = parseManifestFile(iden, path)
        data.append({
          "path": path,
          "mochitests": mochitests,
          "reftests": reftests,
          "supportfiles": supportfiles,
        })
        data.extend(readManifests(iden, ["%s/%s" % (path, d) for d in subdirs]))
    return data


def getData(confFile):
    """This function parses a file of the form
    (hg or git)|URL of remote repository|identifier for the local directory
    First directory of tests
    ...
    Last directory of tests"""
    vcs = ""
    url = ""
    iden = ""
    directories = []
    try:
        with open(confFile, 'r') as fp:
            first = True
            for line in fp:
                if first:
                    vcs, url, iden = line.strip().split("|")
                    first = False
                else:
                    directories.append(line.strip())
    finally:
        return vcs, url, iden, directories


def makePathInternal(a, b):
    if not b:
        # Empty directory, i.e., the repository root.
        return a
    return "%s/%s" % (a, b)


def makeSourcePath(a, b):
    """Make a path in the source (upstream) directory."""
    return makePathInternal("hg-%s" % a, b)


def makeDestPath(a, b):
    """Make a path in the destination (mozilla-central) directory, shortening as
    appropriate."""
    def shorten(path):
        path = path.replace('dom-tree-accessors', 'dta')
        path = path.replace('document.getElementsByName', 'doc.gEBN')
        path = path.replace('requirements-for-implementations', 'implreq')
        path = path.replace('other-elements-attributes-and-apis', 'oeaaa')
        return path

    return shorten(makePathInternal(a, b))


def extractReftestFiles(reftests):
    """Returns the set of files referenced in the reftests argument"""
    files = set()
    for line in reftests:
        files.update([line[1], line[2]])
    return files


def copy(dest, directories):
    """Copy mochitests and support files from the external HG directory to their
    place in mozilla-central.
    """
    print("Copying tests...")
    for d in directories:
        sourcedir = makeSourcePath(dest, d["path"])
        destdir = makeDestPath(dest, d["path"])
        os.makedirs(destdir)

        reftestfiles = extractReftestFiles(d["reftests"])

        for mochitest in d["mochitests"]:
            shutil.copy("%s/%s" % (sourcedir, mochitest), "%s/test_%s" % (destdir, mochitest))
        for reftest in sorted(reftestfiles):
            shutil.copy("%s/%s" % (sourcedir, reftest), "%s/%s" % (destdir, reftest))
        for support in d["supportfiles"]:
            shutil.copy("%s/%s" % (sourcedir, support), "%s/%s" % (destdir, support))

def printBuildFiles(dest, directories):
    """Create a mochitest.ini that all the contains tests we import.
    """
    print("Creating manifest...")
    all_mochitests = set()
    all_support = set()

    for d in directories:
        path = makeDestPath(dest, d["path"])

        all_mochitests |= set('%s/test_%s' % (d['path'], mochitest)
            for mochitest in d['mochitests'])
        all_support |= set('%s/%s' % (d['path'], p) for p in d['supportfiles'])

        if d["reftests"]:
            with open(path + "/reftest.list", "w") as fh:
                result = writeBuildFiles.substReftestList("importTestsuite.py",
                    d["reftests"])
                fh.write(result)

    manifest_path = dest + '/mochitest.ini'
    with open(manifest_path, 'w') as fh:
        result = writeBuildFiles.substManifest('importTestsuite.py',
            all_mochitests, all_support)
        fh.write(result)
    subprocess.check_call(["hg", "add", manifest_path])

def hgadd(dest, directories):
    """Inform hg of the files in |directories|."""
    print("hg addremoving...")
    for d in directories:
        subprocess.check_call(["hg", "addremove", makeDestPath(dest, d)])

def removeAndCloneRepo(vcs, url, dest):
    """Replaces the repo at dest by a fresh clone from url using vcs"""
    assert vcs in ('hg', 'git')

    print("Removing %s..." % dest)
    subprocess.check_call(["rm", "-rf", dest])

    print("Cloning %s to %s with %s..." % (url, dest, vcs))
    subprocess.check_call([vcs, "clone", url, dest])

def importRepo(confFile):
    try:
        vcs, url, iden, directories = getData(confFile)
        dest = iden
        hgdest = "hg-%s" % iden

        print("Removing %s..." % dest)
        subprocess.check_call(["rm", "-rf", dest])

        removeAndCloneRepo(vcs, url, hgdest)

        data = readManifests(iden, directories)
        print("Going to import %s..." % [d["path"] for d in data])

        copy(dest, data)
        printBuildFiles(dest, data)
        hgadd(dest, directories)
        print("Removing %s again..." % hgdest)
        subprocess.check_call(["rm", "-rf", hgdest])
    except subprocess.CalledProcessError as e:
        print(e.returncode)
    finally:
        print("Done")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Need one argument.")
    else:
        importRepo(sys.argv[1])

