# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Fetch build artifacts from a Firefox tree.

This provides an (at-the-moment special purpose) interface to download Android
artifacts from Mozilla's Task Cluster.

This module performs the following steps:

* find a candidate hg parent revision using the local pushlog.  The local
  pushlog is maintained by mozext locally and updated on every pull.

* map the candidate parent to candidate Task Cluster tasks and artifact
  locations.  Pushlog entries might not correspond to tasks (yet), and those
  tasks might not produce the desired class of artifacts.

* fetch fresh Task Cluster artifacts and purge old artifacts, using a simple
  Least Recently Used cache.

The bulk of the complexity is in managing and persisting several caches.  If
we found a Python LRU cache that pickled cleanly, we could remove a lot of
this code!  Sadly, I found no such candidate implementations, so we pickle
pylru caches manually.

None of the instances (or the underlying caches) are safe for concurrent use.
A future need, perhaps.

This module requires certain modules be importable from the ambient Python
environment.  |mach artifact| ensures these modules are available, but other
consumers will need to arrange this themselves.
'''


from __future__ import absolute_import, print_function, unicode_literals

import functools
import logging
import operator
import os
import pickle
import re
import shutil
import subprocess
import urlparse
import zipfile

import pylru
import taskcluster

from mozbuild.util import (
    ensureParentDir,
    FileAvoidWrite,
)

MAX_CACHED_PARENTS = 100  # Number of parent changesets to cache candidate pushheads for.
NUM_PUSHHEADS_TO_QUERY_PER_PARENT = 50  # Number of candidate pushheads to cache per parent changeset.

MAX_CACHED_TASKS = 400  # Number of pushheads to cache Task Cluster task data for.

# Number of downloaded artifacts to cache.  Each artifact can be very large,
# so don't make this to large!  TODO: make this a size (like 500 megs) rather than an artifact count.
MAX_CACHED_ARTIFACTS = 6

# TODO: handle multiple artifacts with the same filename.
# TODO: handle installing binaries from different types of artifacts (.tar.bz2, .dmg, etc).
# Keep the keys of this map in sync with the |mach artifact| --job options.
JOB_DETAILS = {
    # 'android-api-9': {'re': re.compile('public/build/fennec-(.*)\.android-arm\.apk')},
    'android-api-11': {'re': re.compile('public/build/geckolibs-(.*)\.aar')},
    # 'linux': {'re': re.compile('public/build/firefox-(.*)\.linux-i686\.tar\.bz2')},
    # 'linux64': {'re': re.compile('public/build/firefox-(.*)\.linux-x86_64\.tar\.bz2')},
    # 'macosx64': {'re': re.compile('public/build/firefox-(.*)\.mac\.dmg')},
}


def cachedmethod(cachefunc):
    '''Decorator to wrap a class or instance method with a memoizing callable that
    saves results in a (possibly shared) cache.
    '''
    def decorator(method):
        def wrapper(self, *args, **kwargs):
            mapping = cachefunc(self)
            if mapping is None:
                return method(self, *args, **kwargs)
            key = (method.__name__, args, tuple(sorted(kwargs.items())))
            try:
                value = mapping[key]
                return value
            except KeyError:
                pass
            result = method(self, *args, **kwargs)
            mapping[key] = result
            return result
        return functools.update_wrapper(wrapper, method)
    return decorator


class CacheManager(object):
    '''Maintain an LRU cache.  Provide simple persistence, including support for
    loading and saving the state using a "with" block.  Allow clearing the cache
    and printing the cache for debugging.

    Provide simple logging.
    '''

    def __init__(self, cache_dir, cache_name, cache_size, cache_callback=None, log=None):
        self._cache = pylru.lrucache(cache_size, callback=cache_callback)
        self._cache_filename = os.path.join(cache_dir, cache_name + '-cache.pickle')
        self._log = log

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def load_cache(self):
        try:
            items = pickle.load(open(self._cache_filename, 'rb'))
            for key, value in items:
                self._cache[key] = value
        except Exception as e:
            # Corrupt cache, perhaps?  Sadly, pickle raises many different
            # exceptions, so it's not worth trying to be fine grained here.
            # We ignore any exception, so the cache is effectively dropped.
            self.log(logging.INFO, 'artifact',
                {'filename': self._cache_filename, 'exception': repr(e)},
                'Ignoring exception unpickling cache file {filename}: {exception}')
            pass

    def dump_cache(self):
        ensureParentDir(self._cache_filename)
        pickle.dump(list(reversed(list(self._cache.items()))), open(self._cache_filename, 'wb'), -1)

    def clear_cache(self):
        with self:
            self._cache.clear()

    def print_cache(self):
        with self:
            for item in self._cache.items():
                self.log(logging.INFO, 'artifact',
                    {'item': item},
                    '{item}')

    def print_last_item(self, args, sorted_kwargs, result):
        # By default, show nothing.
        pass

    def print_last(self):
        # We use the persisted LRU caches to our advantage.  The first item is
        # most recent.
        with self:
            item = next(self._cache.items(), None)
            if item is not None:
                (name, args, sorted_kwargs), result = item
                self.print_last_item(args, sorted_kwargs, result)
            else:
                self.log(logging.WARN, 'artifact',
                    {},
                    'No last cached item found.')

    def __enter__(self):
        self.load_cache()
        return self

    def __exit__(self, type, value, traceback):
        self.dump_cache()


class PushHeadCache(CacheManager):
    '''Map parent hg revisions to candidate pushheads.'''

    def __init__(self, hg, cache_dir, log=None):
        # It's not unusual to pull hundreds of changesets at once, and perhaps
        # |hg up| back and forth a few times.
        CacheManager.__init__(self, cache_dir, 'pushheads', MAX_CACHED_PARENTS, log=log)
        self._hg = hg

    @cachedmethod(operator.attrgetter('_cache'))
    def pushheads(self, tree, parent):
        pushheads = subprocess.check_output([self._hg, 'log',
            '--template', '{node}\n',
            '-r', 'last(pushhead("{tree}") & ::"{parent}", {num})'.format(
                tree=tree, parent=parent, num=NUM_PUSHHEADS_TO_QUERY_PER_PARENT)])
        pushheads = pushheads.strip().split('\n')
        return pushheads


class TaskCache(CacheManager):
    '''Map candidate pushheads to Task Cluster task IDs and artifact URLs.'''

    def __init__(self, cache_dir, log=None):
        CacheManager.__init__(self, cache_dir, 'artifact_url', MAX_CACHED_TASKS, log=log)
        self._index = taskcluster.Index()
        self._queue = taskcluster.Queue()

    @cachedmethod(operator.attrgetter('_cache'))
    def artifact_url(self, tree, job, rev):
        try:
            artifact_re = JOB_DETAILS[job]['re']
        except KeyError:
            self.log(logging.INFO, 'artifact',
                {'job': job},
                'Unknown job {job}')
            raise KeyError("Unknown job")

        # Bug 1175655: it appears that the Task Cluster index only takes
        # 12-char hex hashes.
        key = '{rev}.{tree}.{job}'.format(rev=rev[:12], tree=tree, job=job)
        try:
            namespace = 'buildbot.revisions.{key}'.format(key=key)
            task = self._index.findTask(namespace)
        except Exception:
            # Not all revisions correspond to pushes that produce the job we
            # care about; and even those that do may not have completed yet.
            raise ValueError('Task for {key} does not exist (yet)!'.format(key=key))
        taskId = task['taskId']

        # TODO: Make this not Android-only by matching a regular expression.
        artifacts = self._queue.listLatestArtifacts(taskId)['artifacts']

        def names():
            for artifact in artifacts:
                name = artifact['name']
                if artifact_re.match(name):
                    yield name

        # TODO: Handle multiple artifacts, taking the latest one.
        for name in names():
            # We can easily extract the task ID and the build ID from the URL.
            url = self._queue.buildUrl('getLatestArtifact', taskId, name)
            return url
        raise ValueError('Task for {key} existed, but no artifacts found!'.format(key=key))

    def print_last_item(self, args, sorted_kwargs, result):
        tree, job, rev = args
        self.log(logging.INFO, 'artifact',
            {'rev': rev},
            'Last installed binaries from hg parent revision {rev}')


class ArtifactCache(CacheManager):
    '''Fetch Task Cluster artifact URLs and purge least recently used artifacts from disk.'''

    def __init__(self, cache_dir, log=None):
        # TODO: instead of storing N artifact packages, store M megabytes.
        CacheManager.__init__(self, cache_dir, 'fetch', MAX_CACHED_ARTIFACTS, cache_callback=self.delete_file, log=log)
        self._cache_dir = cache_dir

    def delete_file(self, key, value):
        try:
            os.remove(value)
            self.log(logging.INFO, 'artifact',
                {'filename': value},
                'Purged artifact {filename}')
        except IOError:
            pass

    @cachedmethod(operator.attrgetter('_cache'))
    def fetch(self, url, force=False):
        args = ['wget', url]

        if not force:
            args[1:1] = ['--timestamping']

        proc = subprocess.Popen(args, cwd=self._cache_dir)
        status = None
        # Leave it to the subprocess to handle Ctrl+C. If it terminates as
        # a result of Ctrl+C, proc.wait() will return a status code, and,
        # we get out of the loop. If it doesn't, like e.g. gdb, we continue
        # waiting.
        while status is None:
            try:
                status = proc.wait()
            except KeyboardInterrupt:
                pass

        if status != 0:
            raise Exception('Process executed with non-0 exit code: %s' % args)

        return os.path.abspath(os.path.join(self._cache_dir, os.path.basename(url)))

    def print_last_item(self, args, sorted_kwargs, result):
        url, = args
        self.log(logging.INFO, 'artifact',
            {'url': url},
            'Last installed binaries from url {url}')
        self.log(logging.INFO, 'artifact',
            {'filename': result},
            'Last installed binaries from local file {filename}')


class Artifacts(object):
    '''Maintain state to efficiently fetch build artifacts from a Firefox tree.'''

    def __init__(self, tree, job, log=None, cache_dir='.', hg='hg'):
        self._tree = tree
        self._job = job
        self._log = log
        self._hg = hg
        self._cache_dir = cache_dir

        self._pushhead_cache = PushHeadCache(self._hg, self._cache_dir, log=self._log)
        self._task_cache = TaskCache(self._cache_dir, log=self._log)
        self._artifact_cache = ArtifactCache(self._cache_dir, log=self._log)

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def install_from_file(self, filename, distdir):
        self.log(logging.INFO, 'artifact',
            {'filename': filename},
            'Installing from {filename}')

        # Copy all .so files to dist/bin, avoiding modification where possible.
        ensureParentDir(os.path.join(distdir, 'bin', '.dummy'))

        with zipfile.ZipFile(filename) as zf:
            for info in zf.infolist():
                if not info.filename.endswith('.so'):
                    continue
                n = os.path.join(distdir, 'bin', os.path.basename(info.filename))
                fh = FileAvoidWrite(n, mode='r')
                shutil.copyfileobj(zf.open(info), fh)
                fh.write(zf.open(info).read())
                file_existed, file_updated = fh.close()
                self.log(logging.INFO, 'artifact',
                    {'updating': 'Updating' if file_updated else 'Not updating', 'filename': n},
                    '{updating} {filename}')
        return 0

    def install_from_url(self, url, distdir):
        self.log(logging.INFO, 'artifact',
            {'url': url},
            'Installing from {url}')
        with self._artifact_cache as artifact_cache:  # The with block handles persistence.
            filename = artifact_cache.fetch(url)
        return self.install_from_file(filename, distdir)

    def install_from_hg(self, revset, distdir):
        if not revset:
            revset = '.'
        if len(revset) != 40:
            revset = subprocess.check_output([self._hg, 'log', '--template', '{node}\n', '-r', revset]).strip()
            if len(revset.split('\n')) != 1:
                raise ValueError('hg revision specification must resolve to exactly one commit')

        self.log(logging.INFO, 'artifact',
            {'revset': revset},
            'Installing from {revset}')

        url = None
        with self._task_cache as task_cache, self._pushhead_cache as pushhead_cache:
            # with blocks handle handle persistence.
            for pushhead in pushhead_cache.pushheads(self._tree, revset):
                try:
                    url = task_cache.artifact_url(self._tree, self._job, pushhead)
                    break
                except ValueError:
                    pass
        if url:
            return self.install_from_url(url, distdir)
        return 1

    def install_from(self, source, distdir):
        if source and os.path.isfile(source):
            return self.install_from_file(source, distdir)
        elif source and urlparse.urlparse(source).scheme:
            return self.install_from_url(source, distdir)
        else:
            return self.install_from_hg(source, distdir)

    def print_last(self):
        self.log(logging.INFO, 'artifact',
            {},
            'Printing last used artifact details.')
        self._pushhead_cache.print_last()
        self._task_cache.print_last()
        self._artifact_cache.print_last()

    def clear_cache(self):
        self.log(logging.INFO, 'artifact',
            {},
            'Deleting cached artifacts and caches.')
        self._pushhead_cache.clear_cache()
        self._task_cache.clear_cache()
        self._artifact_cache.clear_cache()

    def print_cache(self):
        self.log(logging.INFO, 'artifact',
            {},
            'Printing cached artifacts and caches.')
        self._pushhead_cache.print_cache()
        self._task_cache.print_cache()
        self._artifact_cache.print_cache()
