# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import platform
import signal
import subprocess
import sys
from collections import defaultdict

from mozprocess import ProcessHandlerMixin

from mozlint import result
from mozlint.util import pip
from mozlint.pathutils import get_ancestors_by_name


here = os.path.abspath(os.path.dirname(__file__))
FLAKE8_REQUIREMENTS_PATH = os.path.join(here, 'flake8_requirements.txt')

FLAKE8_NOT_FOUND = """
Could not find flake8! Install flake8 and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(FLAKE8_REQUIREMENTS_PATH)


FLAKE8_INSTALL_ERROR = """
Unable to install correct version of flake8
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(FLAKE8_REQUIREMENTS_PATH)

LINE_OFFSETS = {
    # continuation line under-indented for hanging indent
    'E121': (-1, 2),
    # continuation line missing indentation or outdented
    'E122': (-1, 2),
    # continuation line over-indented for hanging indent
    'E126': (-1, 2),
    # continuation line over-indented for visual indent
    'E127': (-1, 2),
    # continuation line under-indented for visual indent
    'E128': (-1, 2),
    # continuation line unaligned for hanging indend
    'E131': (-1, 2),
    # expected 1 blank line, found 0
    'E301': (-1, 2),
    # expected 2 blank lines, found 1
    'E302': (-2, 3),
}
"""Maps a flake8 error to a lineoffset tuple.

The offset is of the form (lineno_offset, num_lines) and is passed
to the lineoffset property of an `Issue`.
"""

# We use sys.prefix to find executables as that gets modified with
# virtualenv's activate_this.py, whereas sys.executable doesn't.
if platform.system() == 'Windows':
    bindir = os.path.join(sys.prefix, 'Scripts')
else:
    bindir = os.path.join(sys.prefix, 'bin')

results = []


class Flake8Process(ProcessHandlerMixin):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs['processOutputLine'] = [self.process_line]
        ProcessHandlerMixin.__init__(self, *args, **kwargs)

    def process_line(self, line):
        # Escape slashes otherwise JSON conversion will not work
        line = line.replace('\\', '\\\\')
        try:
            res = json.loads(line)
        except ValueError:
            print('Non JSON output from linter, will not be processed: {}'.format(line))
            return

        if res.get('code') in LINE_OFFSETS:
            res['lineoffset'] = LINE_OFFSETS[res['code']]

        results.append(result.from_config(self.config, **res))

    def run(self, *args, **kwargs):
        # flake8 seems to handle SIGINT poorly. Handle it here instead
        # so we can kill the process without a cryptic traceback.
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandlerMixin.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def run_process(config, cmd):
    proc = Flake8Process(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()
        return 1


def setup(root):
    if not pip.reinstall_program(FLAKE8_REQUIREMENTS_PATH):
        print(FLAKE8_INSTALL_ERROR)
        return 1


def lint(paths, config, **lintargs):
    # TODO don't store results in a global
    global results
    results = []

    cmdargs = [
        os.path.join(bindir, 'flake8'),
        '--format', '{"path":"%(path)s","lineno":%(row)s,'
                    '"column":%(col)s,"rule":"%(code)s","message":"%(text)s"}',
        '--filename', ','.join(['*.{}'.format(e) for e in config['extensions']]),
    ]

    fix_cmdargs = [
        os.path.join(bindir, 'autopep8'),
        '--global-config', os.path.join(lintargs['root'], '.flake8'),
        '--in-place', '--recursive',
    ]

    if config.get('exclude'):
        fix_cmdargs.extend(['--exclude', ','.join(config['exclude'])])

    # Run any paths with a .flake8 file in the directory separately so
    # it gets picked up. This means only .flake8 files that live in
    # directories that are explicitly included will be considered.
    # See bug 1277851
    paths_by_config = defaultdict(list)
    for path in paths:
        configs = get_ancestors_by_name('.flake8', path, lintargs['root'])
        paths_by_config[os.pathsep.join(configs) if configs else 'default'].append(path)

    for configs, paths in paths_by_config.items():
        if lintargs.get('fix'):
            subprocess.call(fix_cmdargs + paths)

        cmd = cmdargs[:]
        if configs != 'default':
            configs = reversed(configs.split(os.pathsep))
            cmd.extend(['--append-config={}'.format(c) for c in configs])

        cmd.extend(paths)
        if run_process(config, cmd):
            break

    return results
