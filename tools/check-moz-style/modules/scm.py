# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Python module for interacting with an SCM system (like SVN or Git)

import os
import re
import subprocess
import sys

# Import WebKit-specific modules.
from modules.logging import error, log

def detect_scm_system(path):
    if HG.in_working_directory(path):
        return HG(cwd=path)

    if SVN.in_working_directory(path):
        return SVN(cwd=path)

    if Git.in_working_directory(path):
        return Git(cwd=path)

    raise ScriptError("working directory is not a HG/SVN/Git repo")

def first_non_empty_line_after_index(lines, index=0):
    first_non_empty_line = index
    for line in lines[index:]:
        if re.match("^\s*$", line):
            first_non_empty_line += 1
        else:
            break
    return first_non_empty_line


class CommitMessage:
    def __init__(self, message):
        self.message_lines = message[first_non_empty_line_after_index(message, 0):]

    def body(self, lstrip=False):
        lines = self.message_lines[first_non_empty_line_after_index(self.message_lines, 1):]
        if lstrip:
            lines = [line.lstrip() for line in lines]
        return "\n".join(lines) + "\n"

    def description(self, lstrip=False, strip_url=False):
        line = self.message_lines[0]
        if lstrip:
            line = line.lstrip()
        if strip_url:
            line = re.sub("^(\s*)<.+> ", "\1", line)
        return line

    def message(self):
        return "\n".join(self.message_lines) + "\n"

    def parse_bug_id(self):
        for line in self.message_lines:
            match = re.search("http\://webkit\.org/b/(?P<bug_id>\d+)", line)
            if match:
                return match.group('bug_id')
            match = re.search(Bugzilla.bug_server_regex + "show_bug\.cgi\?id=(?P<bug_id>\d+)", line)
            if match:
                return match.group('bug_id')
        return None


class ScriptError(Exception):
    pass


class SCM:
    def __init__(self, cwd, dryrun=False):
        self.cwd = cwd
        self.checkout_root = self.find_checkout_root(self.cwd)
        self.dryrun = dryrun

    @staticmethod
    def run_command(args, cwd=None, input=None, raise_on_failure=True, return_exit_code=False):
        stdin = subprocess.PIPE if input else None
        process = subprocess.Popen(args, stdout=subprocess.PIPE, stdin=stdin, cwd=cwd)
        output = process.communicate(input)[0].rstrip()
        exit_code = process.wait()
        if raise_on_failure and exit_code:
            raise ScriptError('Failed to run "%s"  exit_code: %d  cwd: %s' % (args, exit_code, cwd))
        if return_exit_code:
            return exit_code
        return output

    def script_path(self, script_name):
        return os.path.join(self.checkout_root, "WebKitTools", "Scripts", script_name)

    def ensure_clean_working_directory(self, force):
        if not force and not self.working_directory_is_clean():
            print self.run_command(self.status_command(), raise_on_failure=False)
            error("Working directory has modifications, pass --force-clean or --no-clean to continue.")
        
        log("Cleaning working directory")
        self.clean_working_directory()
    
    def ensure_no_local_commits(self, force):
        if not self.supports_local_commits():
            return
        commits = self.local_commits()
        if not len(commits):
            return
        if not force:
            error("Working directory has local commits, pass --force-clean to continue.")
        self.discard_local_commits()

    def apply_patch(self, patch):
        # It's possible that the patch was not made from the root directory.
        # We should detect and handle that case.
        curl_process = subprocess.Popen(['curl', patch['url']], stdout=subprocess.PIPE)
        patch_apply_process = subprocess.Popen([self.script_path('svn-apply'), '--reviewer', patch['reviewer']], stdin=curl_process.stdout)

        return_code = patch_apply_process.wait()
        if return_code:
            raise ScriptError("Patch %s from bug %s failed to download and apply." % (patch['url'], patch['bug_id']))

    def run_status_and_extract_filenames(self, status_command, status_regexp):
        filenames = []
        for line in self.run_command(status_command).splitlines():
            match = re.search(status_regexp, line)
            if not match:
                continue
            # status = match.group('status')
            filename = match.group('filename')
            filenames.append(filename)
        return filenames

    @staticmethod
    def in_working_directory(path):
        raise NotImplementedError, "subclasses must implement"

    @staticmethod
    def find_checkout_root(path):
        raise NotImplementedError, "subclasses must implement"

    @staticmethod
    def commit_success_regexp():
        raise NotImplementedError, "subclasses must implement"

    def working_directory_is_clean(self):
        raise NotImplementedError, "subclasses must implement"

    def clean_working_directory(self):
        raise NotImplementedError, "subclasses must implement"

    def update_webkit(self):
        raise NotImplementedError, "subclasses must implement"

    def status_command(self):
        raise NotImplementedError, "subclasses must implement"

    def changed_files(self):
        raise NotImplementedError, "subclasses must implement"

    def display_name(self):
        raise NotImplementedError, "subclasses must implement"

    def create_patch(self):
        raise NotImplementedError, "subclasses must implement"

    def commit_with_message(self, message):
        raise NotImplementedError, "subclasses must implement"
    
    # Subclasses must indicate if they support local commits,
    # but the SCM baseclass will only call local_commits methods when this is true.
    @staticmethod
    def supports_local_commits():
        raise NotImplementedError, "subclasses must implement"

    def create_patch_from_local_commit(self, commit_id):
        pass

    def commit_locally_with_message(self, message):
        pass

    def discard_local_commits(self):
        pass

    def local_commits(self):
        return []


class SVN(SCM):
    def __init__(self, cwd, dryrun=False):
        SCM.__init__(self, cwd, dryrun)
        self.cached_version = None
    
    @staticmethod
    def in_working_directory(path):
        return os.path.isdir(os.path.join(path, '.svn'))
    
    @staticmethod
    def find_uuid(path):
        if not SVN.in_working_directory(path):
            return None
        info = SVN.run_command(['svn', 'info', path])
        match = re.search("^Repository UUID: (?P<uuid>.+)$", info, re.MULTILINE)
        if not match:
            raise ScriptError('svn info did not contain a UUID.')
        return match.group('uuid')
    
    @staticmethod
    def find_checkout_root(path):
        uuid = SVN.find_uuid(path)
        # If |path| is not in a working directory, we're supposed to return |path|.
        if not uuid:
            return path
        # Search up the directory hierarchy until we find a different UUID.
        last_path = None
        while True:
            if uuid != SVN.find_uuid(path):
                return last_path
            last_path = path
            (path, last_component) = os.path.split(path)
            if last_path == path:
                return None
    
    @staticmethod
    def commit_success_regexp():
        return "^Committed revision (?P<svn_revision>\d+)\.$"
    
    def svn_version(self):
        if not self.cached_version:
            self.cached_version = self.run_command(['svn', '--version', '--quiet'])
        
        return self.cached_version

    def working_directory_is_clean(self):
        return self.run_command(['svn', 'diff']) == ""

    def clean_working_directory(self):
        self.run_command(['svn', 'revert', '-R', '.'])

    def update_webkit(self):
        self.run_command(self.script_path("update-webkit"))

    def status_command(self):
        return ['svn', 'status']

    def changed_files(self):
        if self.svn_version() > "1.6":
            status_regexp = "^(?P<status>[ACDMR]).{6} (?P<filename>.+)$"
        else:
            status_regexp = "^(?P<status>[ACDMR]).{5} (?P<filename>.+)$"
        return self.run_status_and_extract_filenames(self.status_command(), status_regexp)

    @staticmethod
    def supports_local_commits():
        return False

    def display_name(self):
        return "svn"

    def create_patch(self):
        return self.run_command(self.script_path("svn-create-patch"))

    def commit_with_message(self, message):
        if self.dryrun:
            return "Dry run, no remote commit."
        return self.run_command(['svn', 'commit', '-m', message])


# All git-specific logic should go here.
class Git(SCM):
    def __init__(self, cwd, dryrun=False):
        SCM.__init__(self, cwd, dryrun)
    
    @classmethod
    def in_working_directory(cls, path):
        return cls.run_command(['git', 'rev-parse', '--is-inside-work-tree'], raise_on_failure=False, cwd=path) == "true"

    @classmethod
    def find_checkout_root(cls, path):
        # "git rev-parse --show-cdup" would be another way to get to the root
        (checkout_root, dot_git) = os.path.split(cls.run_command(['git', 'rev-parse', '--git-dir'], cwd=path))
        # If we were using 2.6 # checkout_root = os.path.relpath(checkout_root, path)
        if not os.path.isabs(checkout_root): # Sometimes git returns relative paths
            checkout_root = os.path.join(path, checkout_root)
        return checkout_root
    
    @staticmethod
    def commit_success_regexp():
        return "^Committed r(?P<svn_revision>\d+)$"
    
    def discard_local_commits(self):
        self.run_command(['git', 'reset', '--hard', 'trunk'])
    
    def local_commits(self):
        return self.run_command(['git', 'log', '--pretty=oneline', 'HEAD...trunk']).splitlines()

    def working_directory_is_clean(self):
        return self.run_command(['git', 'diff-index', 'HEAD']) == ""
    
    def clean_working_directory(self):
        # Could run git clean here too, but that wouldn't match working_directory_is_clean
        self.run_command(['git', 'reset', '--hard', 'HEAD'])
    
    def update_webkit(self):
        # FIXME: Should probably call update-webkit, no?
        log("Updating working directory")
        self.run_command(['git', 'svn', 'rebase'])

    def status_command(self):
        return ['git', 'status']

    def changed_files(self):
        status_command = ['git', 'diff', '-r', '--name-status', '-C', '-M', 'HEAD']
        status_regexp = '^(?P<status>[ADM])\t(?P<filename>.+)$'
        return self.run_status_and_extract_filenames(status_command, status_regexp)
    
    @staticmethod
    def supports_local_commits():
        return True

    def display_name(self):
        return "git"

    def create_patch(self):
        return self.run_command(['git', 'diff', 'HEAD'])

    def commit_with_message(self, message):
        self.commit_locally_with_message(message)
        return self.push_local_commits_to_server()

    # Git-specific methods:

    def create_patch_from_local_commit(self, commit_id):
        return self.run_command(['git', 'diff', commit_id + "^.." + commit_id])

    def commit_locally_with_message(self, message):
        self.run_command(['git', 'commit', '--all', '-F', '-'], input=message)
        
    def push_local_commits_to_server(self):
        if self.dryrun:
            return "Dry run, no remote commit."
        return self.run_command(['git', 'svn', 'dcommit'])

    def commit_ids_from_range_arguments(self, args, cherry_pick=False):
        # First get the commit-ids for the passed in revisions.
        revisions = self.run_command(['git', 'rev-parse', '--revs-only'] + args).splitlines()

        if cherry_pick:
            return revisions

        # If we're not cherry picking and were only passed one revision, assume "^revision head" aka "revision..head".
        if len(revisions) < 2:
            revisions[0] = "^" + revisions[0]
            revisions.append("HEAD")

        return self.run_command(['git', 'rev-list'] + revisions).splitlines()

    def commit_message_for_local_commit(self, commit_id):
        commit_lines = self.run_command(['git', 'cat-file', 'commit', commit_id]).splitlines()

        # Skip the git headers.
        first_line_after_headers = 0
        for line in commit_lines:
            first_line_after_headers += 1
            if line == "":
                break
        return CommitMessage(commit_lines[first_line_after_headers:])

    def files_changed_summary_for_commit(self, commit_id):
        return self.run_command(['git', 'diff-tree', '--shortstat', '--no-commit-id', commit_id])


# All hg-specific logic should go here.
class HG(SCM):
    def __init__(self, cwd, dryrun=False):
        SCM.__init__(self, cwd, dryrun)

    @classmethod
    def in_working_directory(cls, path):
        return cls.run_command(['hg', 'status'], cwd=path, return_exit_code=True) == 0

    @classmethod
    def find_checkout_root(cls, path):
        checkout_root = cls.run_command(['hg', 'root'], cwd=path)
        return checkout_root

    def status_command(self):
        return ['hg', 'status']

    def display_name(self):
        return "hg"

    def create_patch(self):
        if self.run_command(['hg', 'diff']) !=  "":
            sys.stderr.write("Warning: outstanding changes not include in style check.\n")
        return self.run_command(['hg', 'export', 'tip'])
