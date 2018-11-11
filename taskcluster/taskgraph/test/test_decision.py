# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import json
import yaml
import shutil
import unittest
import tempfile

from mock import patch
from mozunit import main, MockedOpen
from taskgraph import decision


FAKE_GRAPH_CONFIG = {'product-dir': 'browser'}


class TestDecision(unittest.TestCase):

    def test_write_artifact_json(self):
        data = [{'some': 'data'}]
        tmpdir = tempfile.mkdtemp()
        try:
            decision.ARTIFACTS_DIR = os.path.join(tmpdir, "artifacts")
            decision.write_artifact("artifact.json", data)
            with open(os.path.join(decision.ARTIFACTS_DIR, "artifact.json")) as f:
                self.assertEqual(json.load(f), data)
        finally:
            if os.path.exists(tmpdir):
                shutil.rmtree(tmpdir)
            decision.ARTIFACTS_DIR = 'artifacts'

    def test_write_artifact_yml(self):
        data = [{'some': 'data'}]
        tmpdir = tempfile.mkdtemp()
        try:
            decision.ARTIFACTS_DIR = os.path.join(tmpdir, "artifacts")
            decision.write_artifact("artifact.yml", data)
            with open(os.path.join(decision.ARTIFACTS_DIR, "artifact.yml")) as f:
                self.assertEqual(yaml.safe_load(f), data)
        finally:
            if os.path.exists(tmpdir):
                shutil.rmtree(tmpdir)
            decision.ARTIFACTS_DIR = 'artifacts'


class TestGetDecisionParameters(unittest.TestCase):

    ttc_file = os.path.join(os.getcwd(), 'try_task_config.json')

    def setUp(self):
        self.options = {
            'base_repository': 'https://hg.mozilla.org/mozilla-unified',
            'head_repository': 'https://hg.mozilla.org/mozilla-central',
            'head_rev': 'abcd',
            'head_ref': 'ef01',
            'message': '',
            'project': 'mozilla-central',
            'pushlog_id': 143,
            'pushdate': 1503691511,
            'owner': 'nobody@mozilla.com',
            'level': 3,
        }

    @patch('taskgraph.decision.get_hg_revision_branch')
    def test_simple_options(self, mock_get_hg_revision_branch):
        mock_get_hg_revision_branch.return_value = 'default'
        with MockedOpen({self.ttc_file: None}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
        self.assertEqual(params['pushlog_id'], 143)
        self.assertEqual(params['build_date'], 1503691511)
        self.assertEqual(params['hg_branch'], 'default')
        self.assertEqual(params['moz_build_date'], '20170825200511')
        self.assertEqual(params['try_mode'], None)
        self.assertEqual(params['try_options'], None)
        self.assertEqual(params['try_task_config'], None)

    @patch('taskgraph.decision.get_hg_revision_branch')
    def test_no_email_owner(self, _):
        self.options['owner'] = 'ffxbld'
        with MockedOpen({self.ttc_file: None}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
        self.assertEqual(params['owner'], 'ffxbld@noreply.mozilla.org')

    @patch('taskgraph.decision.get_hg_revision_branch')
    def test_try_options(self, _):
        self.options['message'] = 'try: -b do -t all'
        self.options['project'] = 'try'
        with MockedOpen({self.ttc_file: None}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
        self.assertEqual(params['try_mode'], 'try_option_syntax')
        self.assertEqual(params['try_options']['build_types'], 'do')
        self.assertEqual(params['try_options']['unittests'], 'all')
        self.assertEqual(params['try_task_config'], None)

    @patch('taskgraph.decision.get_hg_revision_branch')
    def test_try_task_config(self, _):
        ttc = {'tasks': ['a', 'b'], 'templates': {}}
        self.options['project'] = 'try'
        with MockedOpen({self.ttc_file: json.dumps(ttc)}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
            self.assertEqual(params['try_mode'], 'try_task_config')
            self.assertEqual(params['try_options'], None)
            self.assertEqual(params['try_task_config'], ttc)


if __name__ == '__main__':
    main()
