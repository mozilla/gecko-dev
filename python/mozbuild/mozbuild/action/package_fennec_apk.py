# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Script to produce an Android package (.apk) for Fennec.
'''

from __future__ import absolute_import, print_function

import argparse
import buildconfig
import os
import subprocess
import sys

from mozpack.copier import Jarrer
from mozpack.files import (
    DeflatedFile,
    File,
    FileFinder,
)
from mozpack.mozjar import JarReader
import mozpack.path as mozpath


def package_fennec_apk(inputs=[], omni_ja=None,
                       lib_dirs=[],
                       assets_dirs=[],
                       features_dirs=[],
                       root_files=[],
                       verbose=False):
    jarrer = Jarrer(optimize=False)

    # First, take input files.  The contents of the later files overwrites the
    # content of earlier files.  Multidexing requires special care: we want a
    # coherent set of classesN.dex files, so we only take DEX files from a
    # single input.  This avoids taking, say, classes{1,2,3}.dex from the first
    # input and only classes{1,2}.dex from the second input, leading to
    # (potentially) duplicated symbols at runtime.
    last_input_with_dex_files = None
    for input in inputs:
        jar = JarReader(input)
        for file in jar:
            path = file.filename

            if mozpath.match(path, '/classes*.dex'):
                last_input_with_dex_files = input
                continue

            if jarrer.contains(path):
                jarrer.remove(path)
            jarrer.add(path, DeflatedFile(file), compress=file.compressed)

    # If we have an input with DEX files, take them all here.
    if last_input_with_dex_files:
        jar = JarReader(last_input_with_dex_files)
        for file in jar:
            path = file.filename

            if not mozpath.match(path, '/classes*.dex'):
                continue

            if jarrer.contains(path):
                jarrer.remove(path)
            jarrer.add(path, DeflatedFile(file), compress=file.compressed)

    def add(path, file, compress=None):
        abspath = os.path.abspath(file.path)
        if verbose:
            print('Packaging %s from %s' % (path, file.path))
        if not os.path.exists(abspath):
            raise ValueError('File %s not found (looked for %s)' % \
                             (file.path, abspath))
        if jarrer.contains(path):
            jarrer.remove(path)
        jarrer.add(path, file, compress=compress)

    for features_dir in features_dirs:
        finder = FileFinder(features_dir)
        for p, f in finder.find('**'):
            add(mozpath.join('assets', 'features', p), f, False)

    for assets_dir in assets_dirs:
        finder = FileFinder(assets_dir)
        for p, f in finder.find('**'):
            add(mozpath.join('assets', p), f)

    for lib_dir in lib_dirs:
        finder = FileFinder(lib_dir)
        for p, f in finder.find('**'):
            add(mozpath.join('lib', p), f)

    for root_file in root_files:
        add(os.path.basename(root_file), File(root_file))

    if omni_ja:
        add(mozpath.join('assets', 'omni.ja'), File(omni_ja), compress=False)

    return jarrer


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--verbose', '-v', default=False, action='store_true',
                        help='be verbose')
    parser.add_argument('--inputs', nargs='+',
                        help='Input skeleton AP_ or APK file(s).')
    parser.add_argument('-o', '--output',
                        help='Output APK file.')
    parser.add_argument('--omnijar', default=None,
                        help='Optional omni.ja to pack into APK file.')
    parser.add_argument('--lib-dirs', nargs='*', default=[],
                        help='Optional lib/ dirs to pack into APK file.')
    parser.add_argument('--assets-dirs', nargs='*', default=[],
                        help='Optional assets/ dirs to pack into APK file.')
    parser.add_argument('--features-dirs', nargs='*', default=[],
                        help='Optional features/ dirs to pack into APK file.')
    parser.add_argument('--root-files', nargs='*', default=[],
                        help='Optional files to pack into APK file root.')
    args = parser.parse_args(args)

    if buildconfig.substs.get('OMNIJAR_NAME') != 'assets/omni.ja':
        raise ValueError("Don't know how to package Fennec APKs when "
                         " OMNIJAR_NAME is not 'assets/omni.jar'.")

    jarrer = package_fennec_apk(inputs=args.inputs,
                                omni_ja=args.omnijar,
                                lib_dirs=args.lib_dirs,
                                assets_dirs=args.assets_dirs,
                                features_dirs=args.features_dirs,
                                root_files=args.root_files,
                                verbose=args.verbose)
    jarrer.copy(args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
