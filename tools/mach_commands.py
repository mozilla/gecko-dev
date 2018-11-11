# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import sys
import os
import stat
import platform
import errno
import subprocess

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase, MozbuildObject


@CommandProvider
class SearchProvider(object):
    @Command('dxr', category='misc',
        description='Search for something in DXR.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def dxr(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'http://dxr.mozilla.org/mozilla-central/search?q=%s&redirect=true' % term
        webbrowser.open_new_tab(uri)

    @Command('mdn', category='misc',
        description='Search for something on MDN.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def mdn(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'https://developer.mozilla.org/search?q=%s' % term
        webbrowser.open_new_tab(uri)

    @Command('google', category='misc',
        description='Search for something on Google.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def google(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'https://www.google.com/search?q=%s' % term
        webbrowser.open_new_tab(uri)

    @Command('search', category='misc',
        description='Search for something on the Internets. '
        'This will open 3 new browser tabs and search for the term on Google, '
        'MDN, and DXR.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def search(self, term):
        self.google(term)
        self.mdn(term)
        self.dxr(term)


@CommandProvider
class UUIDProvider(object):
    @Command('uuid', category='misc',
        description='Generate a uuid.')
    @CommandArgument('--format', '-f', choices=['idl', 'cpp', 'c++'],
                     help='Output format for the generated uuid.')
    def uuid(self, format=None):
        import uuid
        u = uuid.uuid4()
        if format in [None, 'idl']:
            print(u)
            if format is None:
                print('')
        if format in [None, 'cpp', 'c++']:
            u = u.hex
            print('{ 0x%s, 0x%s, 0x%s, \\' % (u[0:8], u[8:12], u[12:16]))
            pairs = tuple(map(lambda n: u[n:n+2], range(16, 32, 2)))
            print(('  { ' + '0x%s, ' * 7 + '0x%s } }') % pairs)


@CommandProvider
class RageProvider(MachCommandBase):
    @Command('rage', category='misc',
             description='Express your frustration')
    def rage(self):
        """Have a bad experience developing Firefox? Run this command to
        express your frustration.

        This command will open your default configured web browser to a short
        form where you can submit feedback. Just close the tab when done.
        """
        import getpass
        import urllib
        import webbrowser

        # Try to resolve the current user.
        user = None
        with open(os.devnull, 'wb') as null:
            if os.path.exists(os.path.join(self.topsrcdir, '.hg')):
                try:
                    user = subprocess.check_output(['hg', 'config',
                                                    'ui.username'],
                                                   cwd=self.topsrcdir,
                                                   stderr=null)

                    i = user.find('<')
                    if i >= 0:
                        user = user[i + 1:-2]
                except subprocess.CalledProcessError:
                    pass
            elif os.path.exists(os.path.join(self.topsrcdir, '.git')):
                try:
                    user = subprocess.check_output(['git', 'config', '--get',
                                                    'user.email'],
                                                   cwd=self.topsrcdir,
                                                   stderr=null)
                except subprocess.CalledProcessError:
                    pass

        if not user:
            try:
                user = getpass.getuser()
            except Exception:
                pass

        url = 'https://docs.google.com/a/mozilla.com/forms/d/e/1FAIpQLSeDVC3IXJu5d33Hp_ZTCOw06xEUiYH1pBjAqJ1g_y63sO2vvA/viewform'
        if user:
            url += '?entry.1281044204=%s' % urllib.quote(user)

        print('Please leave your feedback in the opened web form')
        webbrowser.open_new_tab(url)


@CommandProvider
class PastebinProvider(object):
    @Command('pastebin', category='misc',
        description='Command line interface to pastebin.mozilla.org.')
    @CommandArgument('--language', default=None,
                     help='Language to use for syntax highlighting')
    @CommandArgument('--poster', default='',
                     help='Specify your name for use with pastebin.mozilla.org')
    @CommandArgument('--duration', default='day',
                     choices=['d', 'day', 'm', 'month', 'f', 'forever'],
                     help='Keep for specified duration (default: %(default)s)')
    @CommandArgument('file', nargs='?', default=None,
                     help='Specify the file to upload to pastebin.mozilla.org')

    def pastebin(self, language, poster, duration, file):
        import urllib
        import urllib2

        URL = 'https://pastebin.mozilla.org/'

        FILE_TYPES = [{'value': 'text', 'name': 'None', 'extension': 'txt'},
        {'value': 'bash', 'name': 'Bash', 'extension': 'sh'},
        {'value': 'c', 'name': 'C', 'extension': 'c'},
        {'value': 'cpp', 'name': 'C++', 'extension': 'cpp'},
        {'value': 'html4strict', 'name': 'HTML', 'extension': 'html'},
        {'value': 'javascript', 'name': 'Javascript', 'extension': 'js'},
        {'value': 'javascript', 'name': 'Javascript', 'extension': 'jsm'},
        {'value': 'lua', 'name': 'Lua', 'extension': 'lua'},
        {'value': 'perl', 'name': 'Perl', 'extension': 'pl'},
        {'value': 'php', 'name': 'PHP', 'extension': 'php'},
        {'value': 'python', 'name': 'Python', 'extension': 'py'},
        {'value': 'ruby', 'name': 'Ruby', 'extension': 'rb'},
        {'value': 'css', 'name': 'CSS', 'extension': 'css'},
        {'value': 'diff', 'name': 'Diff', 'extension': 'diff'},
        {'value': 'ini', 'name': 'INI file', 'extension': 'ini'},
        {'value': 'java', 'name': 'Java', 'extension': 'java'},
        {'value': 'xml', 'name': 'XML', 'extension': 'xml'},
        {'value': 'xml', 'name': 'XML', 'extension': 'xul'}]

        lang = ''

        if file:
            try:
                with open(file, 'r') as f:
                    content = f.read()
                # TODO: Use mime-types instead of extensions; suprocess('file <f_name>')
                # Guess File-type based on file extension
                extension = file.split('.')[-1]
                for l in FILE_TYPES:
                    if extension == l['extension']:
                        print('Identified file as %s' % l['name'])
                        lang = l['value']
            except IOError:
                print('ERROR. No such file')
                return 1
        else:
            content = sys.stdin.read()
        duration = duration[0]

        if language:
            lang = language


        params = [
            ('parent_pid', ''),
            ('format', lang),
            ('code2', content),
            ('poster', poster),
            ('expiry', duration),
            ('paste', 'Send')]

        data = urllib.urlencode(params)
        print('Uploading ...')
        try:
            req = urllib2.Request(URL, data)
            response = urllib2.urlopen(req)
            http_response_code = response.getcode()
            if http_response_code == 200:
                print(response.geturl())
            else:
                print('Could not upload the file, '
                      'HTTP Response Code %s' %(http_response_code))
        except urllib2.URLError:
            print('ERROR. Could not connect to pastebin.mozilla.org.')
            return 1
        return 0


@CommandProvider
class FormatProvider(MachCommandBase):
    @Command('clang-format', category='misc',
        description='Run clang-format on current changes')
    @CommandArgument('--show', '-s', action = 'store_true',
        help = 'Show diff output on instead of applying changes')
    def clang_format(self, show=False):
        import urllib2

        plat = platform.system()
        fmt = plat.lower() + "/clang-format-3.5"
        fmt_diff = "clang-format-diff-3.5"

        # We are currently using a modified version of clang-format hosted on people.mozilla.org.
        # This is a temporary work around until we upstream the necessary changes and we can use
        # a system version of clang-format. See bug 961541.
        if plat == "Windows":
            fmt += ".exe"
        else:
            arch = os.uname()[4]
            if (plat != "Linux" and plat != "Darwin") or arch != 'x86_64':
                print("Unsupported platform " + plat + "/" + arch +
                      ". Supported platforms are Windows/*, Linux/x86_64 and Darwin/x86_64")
                return 1

        os.chdir(self.topsrcdir)
        self.prompt = True

        try:
            if not self.locate_or_fetch(fmt):
                return 1
            clang_format_diff = self.locate_or_fetch(fmt_diff)
            if not clang_format_diff:
                return 1

        except urllib2.HTTPError as e:
            print("HTTP error {0}: {1}".format(e.code, e.reason))
            return 1

        from subprocess import Popen, PIPE

        if os.path.exists(".hg"):
            diff_process = Popen(["hg", "diff", "-U0", "-r", "tip^",
                                  "--include", "glob:**.c", "--include", "glob:**.cpp", "--include", "glob:**.h",
                                  "--exclude", "listfile:.clang-format-ignore"], stdout=PIPE)
        else:
            git_process = Popen(["git", "diff", "-U0", "HEAD^"], stdout=PIPE)
            try:
                diff_process = Popen(["filterdiff", "--include=*.h", "--include=*.cpp",
                                      "--exclude-from-file=.clang-format-ignore"],
                                     stdin=git_process.stdout, stdout=PIPE)
            except OSError as e:
                if e.errno == errno.ENOENT:
                    print("Can't find filterdiff. Please install patchutils.")
                else:
                    print("OSError {0}: {1}".format(e.code, e.reason))
                return 1


        args = [sys.executable, clang_format_diff, "-p1"]
        if not show:
           args.append("-i")
        cf_process = Popen(args, stdin=diff_process.stdout)
        return cf_process.communicate()[0]

    def locate_or_fetch(self, root):
        target = os.path.join(self._mach_context.state_dir, os.path.basename(root))
        if not os.path.exists(target):
            site = "https://people.mozilla.org/~ajones/clang-format/"
            if self.prompt and raw_input("Download clang-format executables from {0} (yN)? ".format(site)).lower() != 'y':
                print("Download aborted.")
                return 1
            self.prompt = False

            u = site + root
            print("Downloading {0} to {1}".format(u, target))
            data = urllib2.urlopen(url=u).read()
            temp = target + ".tmp"
            with open(temp, "wb") as fh:
                fh.write(data)
                fh.close()
            os.chmod(temp, os.stat(temp).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
            os.rename(temp, target)
        return target

def mozregression_import():
    # Lazy loading of mozregression.
    # Note that only the mach_interface module should be used from this file.
    try:
        import mozregression.mach_interface
    except ImportError:
        return None
    return mozregression.mach_interface


def mozregression_create_parser():
    # Create the mozregression command line parser.
    # if mozregression is not installed, or not up to date, it will
    # first be installed.
    cmd = MozbuildObject.from_environment()
    cmd._activate_virtualenv()
    mozregression = mozregression_import()
    if not mozregression:
        # mozregression is not here at all, install it
        cmd.virtualenv_manager.install_pip_package('mozregression')
        print("mozregression was installed. please re-run your"
              " command. If you keep getting this message please "
              " manually run: 'pip install -U mozregression'.")
    else:
        # check if there is a new release available
        release = mozregression.new_release_on_pypi()
        if release:
            print(release)
            # there is one, so install it. Note that install_pip_package
            # does not work here, so just run pip directly.
            cmd.virtualenv_manager._run_pip([
                'install',
                'mozregression==%s' % release
            ])
            print("mozregression was updated to version %s. please"
                  " re-run your command." % release)
        else:
            # mozregression is up to date, return the parser.
            return mozregression.parser()
    # exit if we updated or installed mozregression because
    # we may have already imported mozregression and running it
    # as this may cause issues.
    sys.exit(0)


@CommandProvider
class MozregressionCommand(MachCommandBase):
    @Command('mozregression',
             category='misc',
             description=("Regression range finder for nightly"
                          " and inbound builds."),
             parser=mozregression_create_parser)
    def run(self, **options):
        self._activate_virtualenv()
        mozregression = mozregression_import()
        mozregression.run(options)
