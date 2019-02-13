# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import re
import sys

from datetime import datetime

# Set up Python environment to load build system packages.
OUR_DIR = os.path.dirname(__file__)
topsrcdir = os.path.normpath(os.path.join(OUR_DIR, '..', '..'))

sys.path.insert(0, os.path.join(topsrcdir, 'python', 'jsmin'))
sys.path.insert(0, os.path.join(topsrcdir, 'python', 'mozbuild'))
sys.path.insert(0, OUR_DIR)

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.graphviz',
    'sphinx.ext.todo',
    'mozbuild.sphinx',
]

templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
project = u'Mozilla Source Tree Docs'
year = datetime.now().year

# Grab the version from the source tree's milestone.
# FUTURE Use Python API from bug 941299.
with open(os.path.join(topsrcdir, 'config', 'milestone.txt'), 'rt') as fh:
    for line in fh:
        line = line.strip()

        if not line or line.startswith('#'):
            continue

        release = line
        break

version = re.sub(r'[ab]\d+$', '', release)

exclude_patterns = ['_build', '_staging', '_venv']
pygments_style = 'sphinx'

# Read The Docs can't import sphinx_rtd_theme, so don't import it there.
on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

if not on_rtd:
    import sphinx_rtd_theme
    html_theme = 'sphinx_rtd_theme'
    html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]


html_static_path = ['_static']
htmlhelp_basename = 'MozillaTreeDocs'
