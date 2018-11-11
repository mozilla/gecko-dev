#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

# This is the modified version of Google's cpplint. The original code is
# http://google-styleguide.googlecode.com/svn/trunk/cpplint/cpplint.py

"""Does WebKit-lint on c++ files.

The goal of this script is to identify places in the code that *may*
be in non-compliance with WebKit style.  It does not attempt to fix
up these problems -- the point is to educate.  It does also not
attempt to find all problems, or to ensure that everything it does
find is legitimately a problem.

In particular, we can get very confused by /* and // inside strings!
We do a small hack, which is to ignore //'s with "'s after them on the
same line, but it is far from perfect (in either direction).
"""

import codecs
import getopt
import math  # for log
import os
import os.path
import re
import sre_compile
import string
import sys
import unicodedata


_USAGE = """
Syntax: cpplint.py [--verbose=#] [--output=vs7] [--filter=-x,+y,...]
        <file> [file] ...

  The style guidelines this tries to follow are those in
    http://webkit.org/coding/coding-style.html

  Every problem is given a confidence score from 1-5, with 5 meaning we are
  certain of the problem, and 1 meaning it could be a legitimate construct.
  This will miss some errors, and is not a substitute for a code review.

  To prevent specific lines from being linted, add a '// NOLINT' comment to the
  end of the line.

  The files passed in will be linted; at least one file must be provided.
  Linted extensions are .cpp, .c and .h.  Other file types will be ignored.

  Flags:

    output=vs7
      By default, the output is formatted to ease emacs parsing.  Visual Studio
      compatible output (vs7) may also be used.  Other formats are unsupported.

    verbose=#
      Specify a number 0-5 to restrict errors to certain verbosity levels.

    filter=-x,+y,...
      Specify a comma-separated list of category-filters to apply: only
      error messages whose category names pass the filters will be printed.
      (Category names are printed with the message and look like
      "[whitespace/indent]".)  Filters are evaluated left to right.
      "-FOO" and "FOO" means "do not print categories that start with FOO".
      "+FOO" means "do print categories that start with FOO".

      Examples: --filter=-whitespace,+whitespace/braces
                --filter=whitespace,runtime/printf,+runtime/printf_format
                --filter=-,+build/include_what_you_use

      To see a list of all the categories used in cpplint, pass no arg:
         --filter=
"""

# We categorize each error message we print.  Here are the categories.
# We want an explicit list so we can list them all in cpplint --filter=.
# If you add a new error message with a new category, add it to the list
# here!  cpplint_unittest.py should tell you if you forget to do this.
# \ used for clearer layout -- pylint: disable-msg=C6013
_ERROR_CATEGORIES = '''\
    build/class
    build/deprecated
    build/endif_comment
    build/forward_decl
    build/header_guard
    build/include
    build/include_order
    build/include_what_you_use
    build/namespaces
    build/printf_format
    build/storage_class
    legal/copyright
    readability/braces
    readability/casting
    readability/check
    readability/comparison_to_zero
    readability/constructors
    readability/control_flow
    readability/fn_size
    readability/function
    readability/multiline_comment
    readability/multiline_string
    readability/null
    readability/streams
    readability/todo
    readability/utf8
    runtime/arrays
    runtime/casting
    runtime/explicit
    runtime/int
    runtime/init
    runtime/invalid_increment
    runtime/memset
    runtime/printf
    runtime/printf_format
    runtime/references
    runtime/rtti
    runtime/sizeof
    runtime/string
    runtime/threadsafe_fn
    runtime/virtual
    whitespace/blank_line
    whitespace/braces
    whitespace/comma
    whitespace/comments
    whitespace/comments-doublespace
    whitespace/end_of_line
    whitespace/ending_newline
    whitespace/indent
    whitespace/labels
    whitespace/line_length
    whitespace/newline
    whitespace/operators
    whitespace/parens
    whitespace/semicolon
    whitespace/tab
    whitespace/todo
'''

# The default state of the category filter. This is overrided by the --filter=
# flag. By default all errors are on, so only add here categories that should be
# off by default (i.e., categories that must be enabled by the --filter= flags).
# All entries here should start with a '-' or '+', as in the --filter= flag.
_DEFAULT_FILTERS = []

# Headers that we consider STL headers.
_STL_HEADERS = frozenset([
    'algobase.h', 'algorithm', 'alloc.h', 'bitset', 'deque', 'exception',
    'function.h', 'functional', 'hash_map', 'hash_map.h', 'hash_set',
    'hash_set.h', 'iterator', 'list', 'list.h', 'map', 'memory', 'pair.h',
    'pthread_alloc', 'queue', 'set', 'set.h', 'sstream', 'stack',
    'stl_alloc.h', 'stl_relops.h', 'type_traits.h',
    'utility', 'vector', 'vector.h',
    ])


# Non-STL C++ system headers.
_CPP_HEADERS = frozenset([
    'algo.h', 'builtinbuf.h', 'bvector.h', 'cassert', 'cctype',
    'cerrno', 'cfloat', 'ciso646', 'climits', 'clocale', 'cmath',
    'complex', 'complex.h', 'csetjmp', 'csignal', 'cstdarg', 'cstddef',
    'cstdio', 'cstdlib', 'cstring', 'ctime', 'cwchar', 'cwctype',
    'defalloc.h', 'deque.h', 'editbuf.h', 'exception', 'fstream',
    'fstream.h', 'hashtable.h', 'heap.h', 'indstream.h', 'iomanip',
    'iomanip.h', 'ios', 'iosfwd', 'iostream', 'iostream.h', 'istream.h',
    'iterator.h', 'limits', 'map.h', 'multimap.h', 'multiset.h',
    'numeric', 'ostream.h', 'parsestream.h', 'pfstream.h', 'PlotFile.h',
    'procbuf.h', 'pthread_alloc.h', 'rope', 'rope.h', 'ropeimpl.h',
    'SFile.h', 'slist', 'slist.h', 'stack.h', 'stdexcept',
    'stdiostream.h', 'streambuf.h', 'stream.h', 'strfile.h', 'string',
    'strstream', 'strstream.h', 'tempbuf.h', 'tree.h', 'typeinfo', 'valarray',
    ])


# Assertion macros.  These are defined in base/logging.h and
# testing/base/gunit.h.  Note that the _M versions need to come first
# for substring matching to work.
_CHECK_MACROS = [
    'DCHECK', 'CHECK',
    'EXPECT_TRUE_M', 'EXPECT_TRUE',
    'ASSERT_TRUE_M', 'ASSERT_TRUE',
    'EXPECT_FALSE_M', 'EXPECT_FALSE',
    'ASSERT_FALSE_M', 'ASSERT_FALSE',
    ]

# Replacement macros for CHECK/DCHECK/EXPECT_TRUE/EXPECT_FALSE
_CHECK_REPLACEMENT = dict([(m, {}) for m in _CHECK_MACROS])

for op, replacement in [('==', 'EQ'), ('!=', 'NE'),
                        ('>=', 'GE'), ('>', 'GT'),
                        ('<=', 'LE'), ('<', 'LT')]:
    _CHECK_REPLACEMENT['DCHECK'][op] = 'DCHECK_%s' % replacement
    _CHECK_REPLACEMENT['CHECK'][op] = 'CHECK_%s' % replacement
    _CHECK_REPLACEMENT['EXPECT_TRUE'][op] = 'EXPECT_%s' % replacement
    _CHECK_REPLACEMENT['ASSERT_TRUE'][op] = 'ASSERT_%s' % replacement
    _CHECK_REPLACEMENT['EXPECT_TRUE_M'][op] = 'EXPECT_%s_M' % replacement
    _CHECK_REPLACEMENT['ASSERT_TRUE_M'][op] = 'ASSERT_%s_M' % replacement

for op, inv_replacement in [('==', 'NE'), ('!=', 'EQ'),
                            ('>=', 'LT'), ('>', 'LE'),
                            ('<=', 'GT'), ('<', 'GE')]:
    _CHECK_REPLACEMENT['EXPECT_FALSE'][op] = 'EXPECT_%s' % inv_replacement
    _CHECK_REPLACEMENT['ASSERT_FALSE'][op] = 'ASSERT_%s' % inv_replacement
    _CHECK_REPLACEMENT['EXPECT_FALSE_M'][op] = 'EXPECT_%s_M' % inv_replacement
    _CHECK_REPLACEMENT['ASSERT_FALSE_M'][op] = 'ASSERT_%s_M' % inv_replacement


# These constants define types of headers for use with
# _IncludeState.check_next_include_order().
_CONFIG_HEADER = 0
_PRIMARY_HEADER = 1
_OTHER_HEADER = 2


_regexp_compile_cache = {}


def match(pattern, s):
    """Matches the string with the pattern, caching the compiled regexp."""
    # The regexp compilation caching is inlined in both match and search for
    # performance reasons; factoring it out into a separate function turns out
    # to be noticeably expensive.
    if not pattern in _regexp_compile_cache:
        _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
    return _regexp_compile_cache[pattern].match(s)


def search(pattern, s):
    """Searches the string for the pattern, caching the compiled regexp."""
    if not pattern in _regexp_compile_cache:
        _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
    return _regexp_compile_cache[pattern].search(s)


class _IncludeState(dict):
    """Tracks line numbers for includes, and the order in which includes appear.

    As a dict, an _IncludeState object serves as a mapping between include
    filename and line number on which that file was included.

    Call check_next_include_order() once for each header in the file, passing
    in the type constants defined above. Calls in an illegal order will
    raise an _IncludeError with an appropriate error message.

    """
    # self._section will move monotonically through this set. If it ever
    # needs to move backwards, check_next_include_order will raise an error.
    _INITIAL_SECTION = 0
    _CONFIG_SECTION = 1
    _PRIMARY_SECTION = 2
    _OTHER_SECTION = 3

    _TYPE_NAMES = {
        _CONFIG_HEADER: 'WebCore config.h',
        _PRIMARY_HEADER: 'header this file implements',
        _OTHER_HEADER: 'other header',
        }
    _SECTION_NAMES = {
        _INITIAL_SECTION: "... nothing.",
        _CONFIG_SECTION: "WebCore config.h.",
        _PRIMARY_SECTION: 'a header this file implements.',
        _OTHER_SECTION: 'other header.',
        }

    def __init__(self):
        dict.__init__(self)
        self._section = self._INITIAL_SECTION
        self._visited_primary_section = False
        self.header_types = dict();

    def visited_primary_section(self):
        return self._visited_primary_section

    def check_next_include_order(self, header_type, file_is_header):
        """Returns a non-empty error message if the next header is out of order.

        This function also updates the internal state to be ready to check
        the next include.

        Args:
          header_type: One of the _XXX_HEADER constants defined above.
          file_is_header: Whether the file that owns this _IncludeState is itself a header

        Returns:
          The empty string if the header is in the right order, or an
          error message describing what's wrong.

        """
        if header_type == _CONFIG_HEADER and file_is_header:
            return 'Header file should not contain WebCore config.h.'
        if header_type == _PRIMARY_HEADER and file_is_header:
            return 'Header file should not contain itself.'

        error_message = ''
        if self._section != self._OTHER_SECTION:
            before_error_message = ('Found %s before %s' %
                                    (self._TYPE_NAMES[header_type],
                                     self._SECTION_NAMES[self._section + 1]))
        after_error_message = ('Found %s after %s' %
                                (self._TYPE_NAMES[header_type],
                                 self._SECTION_NAMES[self._section]))

        if header_type == _CONFIG_HEADER:
            if self._section >= self._CONFIG_SECTION:
                error_message = after_error_message
            self._section = self._CONFIG_SECTION
        elif header_type == _PRIMARY_HEADER:
            if self._section >= self._PRIMARY_SECTION:
                error_message = after_error_message
            elif self._section < self._CONFIG_SECTION:
                error_message = before_error_message
            self._section = self._PRIMARY_SECTION
            self._visited_primary_section = True
        else:
            assert header_type == _OTHER_HEADER
            if not file_is_header and self._section < self._PRIMARY_SECTION:
                error_message = before_error_message
            self._section = self._OTHER_SECTION

        return error_message


class _CppLintState(object):
    """Maintains module-wide state.."""

    def __init__(self):
        self.verbose_level = 1  # global setting.
        self.error_count = 0    # global count of reported errors
        # filters to apply when emitting error messages
        self.filters = _DEFAULT_FILTERS[:]

        # output format:
        # "emacs" - format that emacs can parse (default)
        # "vs7" - format that Microsoft Visual Studio 7 can parse
        self.output_format = 'emacs'

        self.output_stream = sys.stderr

    def set_output_format(self, output_format):
        """Sets the output format for errors."""
        self.output_format = output_format

    def set_verbose_level(self, level):
        """Sets the module's verbosity, and returns the previous setting."""
        last_verbose_level = self.verbose_level
        self.verbose_level = level
        return last_verbose_level

    def set_filters(self, filters):
        """Sets the error-message filters.

        These filters are applied when deciding whether to emit a given
        error message.

        Args:
          filters: A string of comma-separated filters (eg "+whitespace/indent").
                   Each filter should start with + or -; else we die.

        Raises:
          ValueError: The comma-separated filters did not all start with '+' or '-'.
                      E.g. "-,+whitespace,-whitespace/indent,whitespace/badfilter"
        """
        # Default filters always have less priority than the flag ones.
        self.filters = _DEFAULT_FILTERS[:]
        for filter in filters.split(','):
            clean_filter = filter.strip()
            if clean_filter:
                self.filters.append(clean_filter)
        for filter in self.filters:
            if not (filter.startswith('+') or filter.startswith('-')):
                raise ValueError('Every filter in --filter must start with '
                                 '+ or - (%s does not)' % filter)

    def reset_error_count(self):
        """Sets the module's error statistic back to zero."""
        self.error_count = 0

    def increment_error_count(self):
        """Bumps the module's error statistic."""
        self.error_count += 1

    def set_stream(self, stream):
        self.output_stream = stream

    def write_error(self, error):
        self.output_stream.write(error)


_cpplint_state = _CppLintState()


def _output_format():
    """Gets the module's output format."""
    return _cpplint_state.output_format


def _set_output_format(output_format):
    """Sets the module's output format."""
    _cpplint_state.set_output_format(output_format)


def _verbose_level():
    """Returns the module's verbosity setting."""
    return _cpplint_state.verbose_level


def _set_verbose_level(level):
    """Sets the module's verbosity, and returns the previous setting."""
    return _cpplint_state.set_verbose_level(level)


def _filters():
    """Returns the module's list of output filters, as a list."""
    return _cpplint_state.filters


def _set_filters(filters):
    """Sets the module's error-message filters.

    These filters are applied when deciding whether to emit a given
    error message.

    Args:
      filters: A string of comma-separated filters (eg "whitespace/indent").
               Each filter should start with + or -; else we die.
    """
    _cpplint_state.set_filters(filters)


def error_count():
    """Returns the global count of reported errors."""
    return _cpplint_state.error_count


class _FunctionState(object):
    """Tracks current function name and the number of lines in its body."""

    _NORMAL_TRIGGER = 250  # for --v=0, 500 for --v=1, etc.
    _TEST_TRIGGER = 400    # about 50% more than _NORMAL_TRIGGER.

    def __init__(self):
        self.in_a_function = False
        self.lines_in_function = 0
        self.current_function = ''

    def begin(self, function_name):
        """Start analyzing function body.

        Args:
            function_name: The name of the function being tracked.
        """
        self.in_a_function = True
        self.lines_in_function = 0
        self.current_function = function_name

    def count(self):
        """Count line in current function body."""
        if self.in_a_function:
            self.lines_in_function += 1

    def check(self, error, filename, line_number):
        """Report if too many lines in function body.

        Args:
          error: The function to call with any errors found.
          filename: The name of the current file.
          line_number: The number of the line to check.
        """
        if match(r'T(EST|est)', self.current_function):
            base_trigger = self._TEST_TRIGGER
        else:
            base_trigger = self._NORMAL_TRIGGER
        trigger = base_trigger * 2 ** _verbose_level()

        if self.lines_in_function > trigger:
            error_level = int(math.log(self.lines_in_function / base_trigger, 2))
            # 50 => 0, 100 => 1, 200 => 2, 400 => 3, 800 => 4, 1600 => 5, ...
            if error_level > 5:
                error_level = 5
            error(filename, line_number, 'readability/fn_size', error_level,
                  'Small and focused functions are preferred:'
                  ' %s has %d non-comment lines'
                  ' (error triggered by exceeding %d lines).'  % (
                      self.current_function, self.lines_in_function, trigger))

    def end(self):
        """Stop analizing function body."""
        self.in_a_function = False


class _IncludeError(Exception):
    """Indicates a problem with the include order in a file."""
    pass


class FileInfo:
    """Provides utility functions for filenames.

    FileInfo provides easy access to the components of a file's path
    relative to the project root.
    """

    def __init__(self, filename):
        self._filename = filename

    def full_name(self):
        """Make Windows paths like Unix."""
        return os.path.abspath(self._filename).replace('\\', '/')

    def repository_name(self):
        """Full name after removing the local path to the repository.

        If we have a real absolute path name here we can try to do something smart:
        detecting the root of the checkout and truncating /path/to/checkout from
        the name so that we get header guards that don't include things like
        "C:\Documents and Settings\..." or "/home/username/..." in them and thus
        people on different computers who have checked the source out to different
        locations won't see bogus errors.
        """
        fullname = self.full_name()

        if os.path.exists(fullname):
            project_dir = os.path.dirname(fullname)

            if os.path.exists(os.path.join(project_dir, ".svn")):
                # If there's a .svn file in the current directory, we
                # recursively look up the directory tree for the top
                # of the SVN checkout
                root_dir = project_dir
                one_up_dir = os.path.dirname(root_dir)
                while os.path.exists(os.path.join(one_up_dir, ".svn")):
                    root_dir = os.path.dirname(root_dir)
                    one_up_dir = os.path.dirname(one_up_dir)

                prefix = os.path.commonprefix([root_dir, project_dir])
                return fullname[len(prefix) + 1:]

            # Not SVN? Try to find a git top level directory by
            # searching up from the current path.
            root_dir = os.path.dirname(fullname)
            while (root_dir != os.path.dirname(root_dir)
                   and not os.path.exists(os.path.join(root_dir, ".git"))):
                root_dir = os.path.dirname(root_dir)
                if os.path.exists(os.path.join(root_dir, ".git")):
                    prefix = os.path.commonprefix([root_dir, project_dir])
                    return fullname[len(prefix) + 1:]

        # Don't know what to do; header guard warnings may be wrong...
        return fullname

    def split(self):
        """Splits the file into the directory, basename, and extension.

        For 'chrome/browser/browser.cpp', Split() would
        return ('chrome/browser', 'browser', '.cpp')

        Returns:
          A tuple of (directory, basename, extension).
        """

        googlename = self.repository_name()
        project, rest = os.path.split(googlename)
        return (project,) + os.path.splitext(rest)

    def base_name(self):
        """File base name - text after the final slash, before the final period."""
        return self.split()[1]

    def extension(self):
        """File extension - text following the final period."""
        return self.split()[2]

    def no_extension(self):
        """File has no source file extension."""
        return '/'.join(self.split()[0:2])

    def is_source(self):
        """File has a source file extension."""
        return self.extension()[1:] in ('c', 'cc', 'cpp', 'cxx')


def _should_print_error(category, confidence):
    """Returns true iff confidence >= verbose, and category passes filter."""
    # There are two ways we might decide not to print an error message:
    # the verbosity level isn't high enough, or the filters filter it out.
    if confidence < _cpplint_state.verbose_level:
        return False

    is_filtered = False
    for one_filter in _filters():
        if one_filter.startswith('-'):
            if category.startswith(one_filter[1:]):
                is_filtered = True
        elif one_filter.startswith('+'):
            if category.startswith(one_filter[1:]):
                is_filtered = False
        else:
            assert False  # should have been checked for in set_filter.
    if is_filtered:
        return False

    return True


def error(filename, line_number, category, confidence, message):
    """Logs the fact we've found a lint error.

    We log where the error was found, and also our confidence in the error,
    that is, how certain we are this is a legitimate style regression, and
    not a misidentification or a use that's sometimes justified.

    Args:
      filename: The name of the file containing the error.
      line_number: The number of the line containing the error.
      category: A string used to describe the "category" this bug
                falls under: "whitespace", say, or "runtime".  Categories
                may have a hierarchy separated by slashes: "whitespace/indent".
      confidence: A number from 1-5 representing a confidence score for
                  the error, with 5 meaning that we are certain of the problem,
                  and 1 meaning that it could be a legitimate construct.
      message: The error message.
    """
    # There are two ways we might decide not to print an error message:
    # the verbosity level isn't high enough, or the filters filter it out.
    if _should_print_error(category, confidence):
        _cpplint_state.increment_error_count()
        if _cpplint_state.output_format == 'vs7':
            write_error('%s(%s):  %s  [%s] [%d]\n' % (
                filename, line_number, message, category, confidence))
        else:
            write_error('%s:%s:  %s  [%s] [%d]\n' % (
                filename, line_number, message, category, confidence))


# Matches standard C++ escape esequences per 2.13.2.3 of the C++ standard.
_RE_PATTERN_CLEANSE_LINE_ESCAPES = re.compile(
    r'\\([abfnrtv?"\\\']|\d+|x[0-9a-fA-F]+)')
# Matches strings.  Escape codes should already be removed by ESCAPES.
_RE_PATTERN_CLEANSE_LINE_DOUBLE_QUOTES = re.compile(r'"[^"]*"')
# Matches characters.  Escape codes should already be removed by ESCAPES.
_RE_PATTERN_CLEANSE_LINE_SINGLE_QUOTES = re.compile(r"'.'")
# Matches multi-line C++ comments.
# This RE is a little bit more complicated than one might expect, because we
# have to take care of space removals tools so we can handle comments inside
# statements better.
# The current rule is: We only clear spaces from both sides when we're at the
# end of the line. Otherwise, we try to remove spaces from the right side,
# if this doesn't work we try on left side but only if there's a non-character
# on the right.
_RE_PATTERN_CLEANSE_LINE_C_COMMENTS = re.compile(
    r"""(\s*/\*.*\*/\s*$|
            /\*.*\*/\s+|
         \s+/\*.*\*/(?=\W)|
            /\*.*\*/)""", re.VERBOSE)


def is_cpp_string(line):
    """Does line terminate so, that the next symbol is in string constant.

    This function does not consider single-line nor multi-line comments.

    Args:
      line: is a partial line of code starting from the 0..n.

    Returns:
      True, if next character appended to 'line' is inside a
      string constant.
    """

    line = line.replace(r'\\', 'XX')  # after this, \\" does not match to \"
    return ((line.count('"') - line.count(r'\"') - line.count("'\"'")) & 1) == 1


def find_next_multi_line_comment_start(lines, line_index):
    """Find the beginning marker for a multiline comment."""
    while line_index < len(lines):
        if lines[line_index].strip().startswith('/*'):
            # Only return this marker if the comment goes beyond this line
            if lines[line_index].strip().find('*/', 2) < 0:
                return line_index
        line_index += 1
    return len(lines)


def find_next_multi_line_comment_end(lines, line_index):
    """We are inside a comment, find the end marker."""
    while line_index < len(lines):
        if lines[line_index].strip().endswith('*/'):
            return line_index
        line_index += 1
    return len(lines)


def remove_multi_line_comments_from_range(lines, begin, end):
    """Clears a range of lines for multi-line comments."""
    # Having // dummy comments makes the lines non-empty, so we will not get
    # unnecessary blank line warnings later in the code.
    for i in range(begin, end):
        lines[i] = '// dummy'


def remove_multi_line_comments(filename, lines, error):
    """Removes multiline (c-style) comments from lines."""
    line_index = 0
    while line_index < len(lines):
        line_index_begin = find_next_multi_line_comment_start(lines, line_index)
        if line_index_begin >= len(lines):
            return
        line_index_end = find_next_multi_line_comment_end(lines, line_index_begin)
        if line_index_end >= len(lines):
            error(filename, line_index_begin + 1, 'readability/multiline_comment', 5,
                  'Could not find end of multi-line comment')
            return
        remove_multi_line_comments_from_range(lines, line_index_begin, line_index_end + 1)
        line_index = line_index_end + 1


def cleanse_comments(line):
    """Removes //-comments and single-line C-style /* */ comments.

    Args:
      line: A line of C++ source.

    Returns:
      The line with single-line comments removed.
    """
    comment_position = line.find('//')
    if comment_position != -1 and not is_cpp_string(line[:comment_position]):
        line = line[:comment_position]
    # get rid of /* ... */
    return _RE_PATTERN_CLEANSE_LINE_C_COMMENTS.sub('', line)


class CleansedLines(object):
    """Holds 3 copies of all lines with different preprocessing applied to them.

    1) elided member contains lines without strings and comments,
    2) lines member contains lines without comments, and
    3) raw member contains all the lines without processing.
    All these three members are of <type 'list'>, and of the same length.
    """

    def __init__(self, lines):
        self.elided = []
        self.lines = []
        self.raw_lines = lines
        self._num_lines = len(lines)
        for line_number in range(len(lines)):
            self.lines.append(cleanse_comments(lines[line_number]))
            elided = self.collapse_strings(lines[line_number])
            self.elided.append(cleanse_comments(elided))

    def num_lines(self):
        """Returns the number of lines represented."""
        return self._num_lines

    @staticmethod
    def collapse_strings(elided):
        """Collapses strings and chars on a line to simple "" or '' blocks.

        We nix strings first so we're not fooled by text like '"http://"'

        Args:
          elided: The line being processed.

        Returns:
          The line with collapsed strings.
        """
        if not _RE_PATTERN_INCLUDE.match(elided):
            # Remove escaped characters first to make quote/single quote collapsing
            # basic.  Things that look like escaped characters shouldn't occur
            # outside of strings and chars.
            elided = _RE_PATTERN_CLEANSE_LINE_ESCAPES.sub('', elided)
            elided = _RE_PATTERN_CLEANSE_LINE_SINGLE_QUOTES.sub("''", elided)
            elided = _RE_PATTERN_CLEANSE_LINE_DOUBLE_QUOTES.sub('""', elided)
        return elided


def close_expression(clean_lines, line_number, pos):
    """If input points to ( or { or [, finds the position that closes it.

    If lines[line_number][pos] points to a '(' or '{' or '[', finds the the
    line_number/pos that correspond to the closing of the expression.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      pos: A position on the line.

    Returns:
      A tuple (line, line_number, pos) pointer *past* the closing brace, or
      (line, len(lines), -1) if we never find a close.  Note we ignore
      strings and comments when matching; and the line we return is the
      'cleansed' line at line_number.
    """

    line = clean_lines.elided[line_number]
    start_character = line[pos]
    if start_character not in '({[':
        return (line, clean_lines.num_lines(), -1)
    if start_character == '(':
        end_character = ')'
    if start_character == '[':
        end_character = ']'
    if start_character == '{':
        end_character = '}'

    num_open = line.count(start_character) - line.count(end_character)
    while line_number < clean_lines.num_lines() and num_open > 0:
        line_number += 1
        line = clean_lines.elided[line_number]
        num_open += line.count(start_character) - line.count(end_character)
    # OK, now find the end_character that actually got us back to even
    endpos = len(line)
    while num_open >= 0:
        endpos = line.rfind(')', 0, endpos)
        num_open -= 1                 # chopped off another )
    return (line, line_number, endpos + 1)


def check_for_copyright(filename, lines, error):
    """Logs an error if no Copyright message appears at the top of the file."""

    # We'll say it should occur by line 10. Don't forget there's a
    # dummy line at the front.
    for line in xrange(1, min(len(lines), 11)):
        if re.search(r'Copyright|License', lines[line], re.I):
            break
    else:                       # means no copyright line was found
        error(filename, 1, 'legal/copyright', 3,
              'No copyright message found.')


def get_header_guard_cpp_variable(filename):
    """Returns the CPP variable that should be used as a header guard.

    Args:
      filename: The name of a C++ header file.

    Returns:
      The CPP variable that should be used as a header guard in the
      named file.

    """

    fileinfo = FileInfo(filename)
    return re.sub(r'[-./\s]', '_', fileinfo.repository_name()).upper() + '_'


def check_for_header_guard(filename, lines, error):
    """Checks that the file contains a header guard.

    Logs an error if no #ifndef header guard is present.  For other
    headers, checks that the full pathname is used.

    Args:
      filename: The name of the C++ header file.
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """

    cppvar = get_header_guard_cpp_variable(filename)

    ifndef = None
    ifndef_line_number = 0
    define = None
    endif = None
    endif_line_number = 0
    for line_number, line in enumerate(lines):
        line_split = line.split()
        if len(line_split) >= 2:
            # find the first occurrence of #ifndef and #define, save arg
            if not ifndef and line_split[0] == '#ifndef':
                # set ifndef to the header guard presented on the #ifndef line.
                ifndef = line_split[1]
                ifndef_line_number = line_number
            if not define and line_split[0] == '#define':
                define = line_split[1]
        # find the last occurrence of #endif, save entire line
        if line.startswith('#endif'):
            endif = line
            endif_line_number = line_number

    if not ifndef or not define or ifndef != define:
        error(filename, 1, 'build/header_guard', 5,
              'No #ifndef header guard found, suggested CPP variable is: %s' %
              cppvar)
        return

    # The guard should be PATH_FILE_H_, but we also allow PATH_FILE_H__
    # for backward compatibility.
    if ifndef != cppvar:
        error_level = 0
        if ifndef != cppvar + '_':
            error_level = 5

        error(filename, ifndef_line_number, 'build/header_guard', error_level,
              '#ifndef header guard has wrong style, please use: %s' % cppvar)

    if endif != ('#endif  // %s' % cppvar):
        error_level = 0
        if endif != ('#endif  // %s' % (cppvar + '_')):
            error_level = 5

        error(filename, endif_line_number, 'build/header_guard', error_level,
              '#endif line should be "#endif  // %s"' % cppvar)


def check_for_unicode_replacement_characters(filename, lines, error):
    """Logs an error for each line containing Unicode replacement characters.

    These indicate that either the file contained invalid UTF-8 (likely)
    or Unicode replacement characters (which it shouldn't).  Note that
    it's possible for this to throw off line numbering if the invalid
    UTF-8 occurred adjacent to a newline.

    Args:
      filename: The name of the current file.
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """
    for line_number, line in enumerate(lines):
        if u'\ufffd' in line:
            error(filename, line_number, 'readability/utf8', 5,
                  'Line contains invalid UTF-8 (or Unicode replacement character).')


def check_for_new_line_at_eof(filename, lines, error):
    """Logs an error if there is no newline char at the end of the file.

    Args:
      filename: The name of the current file.
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """

    # The array lines() was created by adding two newlines to the
    # original file (go figure), then splitting on \n.
    # To verify that the file ends in \n, we just have to make sure the
    # last-but-two element of lines() exists and is empty.
    if len(lines) < 3 or lines[-2]:
        error(filename, len(lines) - 2, 'whitespace/ending_newline', 5,
              'Could not find a newline character at the end of the file.')


def check_for_multiline_comments_and_strings(filename, clean_lines, line_number, error):
    """Logs an error if we see /* ... */ or "..." that extend past one line.

    /* ... */ comments are legit inside macros, for one line.
    Otherwise, we prefer // comments, so it's ok to warn about the
    other.  Likewise, it's ok for strings to extend across multiple
    lines, as long as a line continuation character (backslash)
    terminates each line. Although not currently prohibited by the C++
    style guide, it's ugly and unnecessary. We don't do well with either
    in this lint program, so we warn about both.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]

    # Remove all \\ (escaped backslashes) from the line. They are OK, and the
    # second (escaped) slash may trigger later \" detection erroneously.
    line = line.replace('\\\\', '')

    if line.count('/*') > line.count('*/'):
        error(filename, line_number, 'readability/multiline_comment', 5,
              'Complex multi-line /*...*/-style comment found. '
              'Lint may give bogus warnings.  '
              'Consider replacing these with //-style comments, '
              'with #if 0...#endif, '
              'or with more clearly structured multi-line comments.')

    if (line.count('"') - line.count('\\"')) % 2:
        error(filename, line_number, 'readability/multiline_string', 5,
              'Multi-line string ("...") found.  This lint script doesn\'t '
              'do well with such strings, and may give bogus warnings.  They\'re '
              'ugly and unnecessary, and you should use concatenation instead".')


_THREADING_LIST = (
    ('asctime(', 'asctime_r('),
    ('ctime(', 'ctime_r('),
    ('getgrgid(', 'getgrgid_r('),
    ('getgrnam(', 'getgrnam_r('),
    ('getlogin(', 'getlogin_r('),
    ('getpwnam(', 'getpwnam_r('),
    ('getpwuid(', 'getpwuid_r('),
    ('gmtime(', 'gmtime_r('),
    ('localtime(', 'localtime_r('),
    ('rand(', 'rand_r('),
    ('readdir(', 'readdir_r('),
    ('strtok(', 'strtok_r('),
    ('ttyname(', 'ttyname_r('),
    )


def check_posix_threading(filename, clean_lines, line_number, error):
    """Checks for calls to thread-unsafe functions.

    Much code has been originally written without consideration of
    multi-threading. Also, engineers are relying on their old experience;
    they have learned posix before threading extensions were added. These
    tests guide the engineers to use thread-safe functions (when using
    posix directly).

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]
    for single_thread_function, multithread_safe_function in _THREADING_LIST:
        index = line.find(single_thread_function)
        # Comparisons made explicit for clarity -- pylint: disable-msg=C6403
        if index >= 0 and (index == 0 or (not line[index - 1].isalnum()
                                          and line[index - 1] not in ('_', '.', '>'))):
            error(filename, line_number, 'runtime/threadsafe_fn', 2,
                  'Consider using ' + multithread_safe_function +
                  '...) instead of ' + single_thread_function +
                  '...) for improved thread safety.')


# Matches invalid increment: *count++, which moves pointer instead of
# incrementing a value.
_RE_PATTERN_INVALID_INCREMENT = re.compile(
    r'^\s*\*\w+(\+\+|--);')


def check_invalid_increment(filename, clean_lines, line_number, error):
    """Checks for invalid increment *count++.

    For example following function:
    void increment_counter(int* count) {
        *count++;
    }
    is invalid, because it effectively does count++, moving pointer, and should
    be replaced with ++*count, (*count)++ or *count += 1.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]
    if _RE_PATTERN_INVALID_INCREMENT.match(line):
        error(filename, line_number, 'runtime/invalid_increment', 5,
              'Changing pointer instead of value (or unused value of operator*).')


class _ClassInfo(object):
    """Stores information about a class."""

    def __init__(self, name, line_number):
        self.name = name
        self.line_number = line_number
        self.seen_open_brace = False
        self.is_derived = False
        self.virtual_method_line_number = None
        self.has_virtual_destructor = False
        self.brace_depth = 0


class _ClassState(object):
    """Holds the current state of the parse relating to class declarations.

    It maintains a stack of _ClassInfos representing the parser's guess
    as to the current nesting of class declarations. The innermost class
    is at the top (back) of the stack. Typically, the stack will either
    be empty or have exactly one entry.
    """

    def __init__(self):
        self.classinfo_stack = []

    def check_finished(self, filename, error):
        """Checks that all classes have been completely parsed.

        Call this when all lines in a file have been processed.
        Args:
          filename: The name of the current file.
          error: The function to call with any errors found.
        """
        if self.classinfo_stack:
            # Note: This test can result in false positives if #ifdef constructs
            # get in the way of brace matching. See the testBuildClass test in
            # cpplint_unittest.py for an example of this.
            error(filename, self.classinfo_stack[0].line_number, 'build/class', 5,
                  'Failed to find complete declaration of class %s' %
                  self.classinfo_stack[0].name)


def check_for_non_standard_constructs(filename, clean_lines, line_number,
                                      class_state, error):
    """Logs an error if we see certain non-ANSI constructs ignored by gcc-2.

    Complain about several constructs which gcc-2 accepts, but which are
    not standard C++.  Warning about these in lint is one way to ease the
    transition to new compilers.
    - put storage class first (e.g. "static const" instead of "const static").
    - "%lld" instead of %qd" in printf-type functions.
    - "%1$d" is non-standard in printf-type functions.
    - "\%" is an undefined character escape sequence.
    - text after #endif is not allowed.
    - invalid inner-style forward declaration.
    - >? and <? operators, and their >?= and <?= cousins.
    - classes with virtual methods need virtual destructors (compiler warning
        available, but not turned on yet.)

    Additionally, check for constructor/destructor style violations as it
    is very convenient to do so while checking for gcc-2 compliance.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      class_state: A _ClassState instance which maintains information about
                   the current stack of nested class declarations being parsed.
      error: A callable to which errors are reported, which takes 4 arguments:
             filename, line number, error level, and message
    """

    # Remove comments from the line, but leave in strings for now.
    line = clean_lines.lines[line_number]

    if search(r'printf\s*\(.*".*%[-+ ]?\d*q', line):
        error(filename, line_number, 'runtime/printf_format', 3,
              '%q in format strings is deprecated.  Use %ll instead.')

    if search(r'printf\s*\(.*".*%\d+\$', line):
        error(filename, line_number, 'runtime/printf_format', 2,
              '%N$ formats are unconventional.  Try rewriting to avoid them.')

    # Remove escaped backslashes before looking for undefined escapes.
    line = line.replace('\\\\', '')

    if search(r'("|\').*\\(%|\[|\(|{)', line):
        error(filename, line_number, 'build/printf_format', 3,
              '%, [, (, and { are undefined character escapes.  Unescape them.')

    # For the rest, work with both comments and strings removed.
    line = clean_lines.elided[line_number]

    if search(r'\b(const|volatile|void|char|short|int|long'
              r'|float|double|signed|unsigned'
              r'|schar|u?int8|u?int16|u?int32|u?int64)'
              r'\s+(auto|register|static|extern|typedef)\b',
              line):
        error(filename, line_number, 'build/storage_class', 5,
              'Storage class (static, extern, typedef, etc) should be first.')

    if match(r'\s*#\s*endif\s*[^/\s]+', line):
        error(filename, line_number, 'build/endif_comment', 5,
              'Uncommented text after #endif is non-standard.  Use a comment.')

    if match(r'\s*class\s+(\w+\s*::\s*)+\w+\s*;', line):
        error(filename, line_number, 'build/forward_decl', 5,
              'Inner-style forward declarations are invalid.  Remove this line.')

    if search(r'(\w+|[+-]?\d+(\.\d*)?)\s*(<|>)\?=?\s*(\w+|[+-]?\d+)(\.\d*)?', line):
        error(filename, line_number, 'build/deprecated', 3,
              '>? and <? (max and min) operators are non-standard and deprecated.')

    # Track class entry and exit, and attempt to find cases within the
    # class declaration that don't meet the C++ style
    # guidelines. Tracking is very dependent on the code matching Google
    # style guidelines, but it seems to perform well enough in testing
    # to be a worthwhile addition to the checks.
    classinfo_stack = class_state.classinfo_stack
    # Look for a class declaration
    class_decl_match = match(
        r'\s*(template\s*<[\w\s<>,:]*>\s*)?(class|struct)\s+(\w+(::\w+)*)', line)
    if class_decl_match:
        classinfo_stack.append(_ClassInfo(class_decl_match.group(3), line_number))

    # Everything else in this function uses the top of the stack if it's
    # not empty.
    if not classinfo_stack:
        return

    classinfo = classinfo_stack[-1]

    # If the opening brace hasn't been seen look for it and also
    # parent class declarations.
    if not classinfo.seen_open_brace:
        # If the line has a ';' in it, assume it's a forward declaration or
        # a single-line class declaration, which we won't process.
        if line.find(';') != -1:
            classinfo_stack.pop()
            return
        classinfo.seen_open_brace = (line.find('{') != -1)
        # Look for a bare ':'
        if search('(^|[^:]):($|[^:])', line):
            classinfo.is_derived = True
        if not classinfo.seen_open_brace:
            return  # Everything else in this function is for after open brace

    # The class may have been declared with namespace or classname qualifiers.
    # The constructor and destructor will not have those qualifiers.
    base_classname = classinfo.name.split('::')[-1]

    # Look for single-argument constructors that aren't marked explicit.
    # Technically a valid construct, but against style.
    args = match(r'(?<!explicit)\s+%s\s*\(([^,()]+)\)'
                 % re.escape(base_classname),
                 line)
    if (args
        and args.group(1) != 'void'
        and not match(r'(const\s+)?%s\s*&' % re.escape(base_classname),
                      args.group(1).strip())):
        error(filename, line_number, 'runtime/explicit', 5,
              'Single-argument constructors should be marked explicit.')

    # Look for methods declared virtual.
    if search(r'\bvirtual\b', line):
        classinfo.virtual_method_line_number = line_number
        # Only look for a destructor declaration on the same line. It would
        # be extremely unlikely for the destructor declaration to occupy
        # more than one line.
        if search(r'~%s\s*\(' % base_classname, line):
            classinfo.has_virtual_destructor = True

    # Look for class end.
    brace_depth = classinfo.brace_depth
    brace_depth = brace_depth + line.count('{') - line.count('}')
    if brace_depth <= 0:
        classinfo = classinfo_stack.pop()
        # Try to detect missing virtual destructor declarations.
        # For now, only warn if a non-derived class with virtual methods lacks
        # a virtual destructor. This is to make it less likely that people will
        # declare derived virtual destructors without declaring the base
        # destructor virtual.
        if ((classinfo.virtual_method_line_number is not None)
            and (not classinfo.has_virtual_destructor)
            and (not classinfo.is_derived)):  # Only warn for base classes
            error(filename, classinfo.line_number, 'runtime/virtual', 4,
                  'The class %s probably needs a virtual destructor due to '
                  'having virtual method(s), one declared at line %d.'
                  % (classinfo.name, classinfo.virtual_method_line_number))
    else:
        classinfo.brace_depth = brace_depth


def check_spacing_for_function_call(filename, line, line_number, error):
    """Checks for the correctness of various spacing around function calls.

    Args:
      filename: The name of the current file.
      line: The text of the line to check.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    # Since function calls often occur inside if/for/foreach/while/switch
    # expressions - which have their own, more liberal conventions - we
    # first see if we should be looking inside such an expression for a
    # function call, to which we can apply more strict standards.
    function_call = line    # if there's no control flow construct, look at whole line
    for pattern in (r'\bif\s*\((.*)\)\s*{',
                    r'\bfor\s*\((.*)\)\s*{',
                    r'\bforeach\s*\((.*)\)\s*{',
                    r'\bwhile\s*\((.*)\)\s*[{;]',
                    r'\bswitch\s*\((.*)\)\s*{'):
        matched = search(pattern, line)
        if matched:
            function_call = matched.group(1)    # look inside the parens for function calls
            break

    # Except in if/for/foreach/while/switch, there should never be space
    # immediately inside parens (eg "f( 3, 4 )").  We make an exception
    # for nested parens ( (a+b) + c ).  Likewise, there should never be
    # a space before a ( when it's a function argument.  I assume it's a
    # function argument when the char before the whitespace is legal in
    # a function name (alnum + _) and we're not starting a macro. Also ignore
    # pointers and references to arrays and functions coz they're too tricky:
    # we use a very simple way to recognize these:
    # " (something)(maybe-something)" or
    # " (something)(maybe-something," or
    # " (something)[something]"
    # Note that we assume the contents of [] to be short enough that
    # they'll never need to wrap.
    if (  # Ignore control structures.
        not search(r'\b(if|for|foreach|while|switch|return|new|delete)\b', function_call)
        # Ignore pointers/references to functions.
        and not search(r' \([^)]+\)\([^)]*(\)|,$)', function_call)
        # Ignore pointers/references to arrays.
        and not search(r' \([^)]+\)\[[^\]]+\]', function_call)):
        if search(r'\w\s*\([ \t](?!\s*\\$)', function_call):      # a ( used for a fn call
            error(filename, line_number, 'whitespace/parens', 4,
                  'Extra space after ( in function call')
        elif search(r'\([ \t]+(?!(\s*\\)|\()', function_call):
            error(filename, line_number, 'whitespace/parens', 2,
                  'Extra space after (')
        if (search(r'\w\s+\(', function_call)
            and not search(r'#\s*define|typedef', function_call)):
            error(filename, line_number, 'whitespace/parens', 4,
                  'Extra space before ( in function call')
        # If the ) is followed only by a newline or a { + newline, assume it's
        # part of a control statement (if/while/etc), and don't complain
        if search(r'[^)\s]\s+\)(?!\s*$|{\s*$)', function_call):
            error(filename, line_number, 'whitespace/parens', 2,
                  'Extra space before )')


def is_blank_line(line):
    """Returns true if the given line is blank.

    We consider a line to be blank if the line is empty or consists of
    only white spaces.

    Args:
      line: A line of a string.

    Returns:
      True, if the given line is blank.
    """
    return not line or line.isspace()


def check_for_function_lengths(filename, clean_lines, line_number,
                               function_state, error):
    """Reports for long function bodies.

    For an overview why this is done, see:
    http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Write_Short_Functions

    Uses a simplistic algorithm assuming other style guidelines
    (especially spacing) are followed.
    Only checks unindented functions, so class members are unchecked.
    Trivial bodies are unchecked, so constructors with huge initializer lists
    may be missed.
    Blank/comment lines are not counted so as to avoid encouraging the removal
    of vertical space and commments just to get through a lint check.
    NOLINT *on the last line of a function* disables this check.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      function_state: Current function name and lines in body so far.
      error: The function to call with any errors found.
    """
    lines = clean_lines.lines
    line = lines[line_number]
    raw = clean_lines.raw_lines
    raw_line = raw[line_number]
    joined_line = ''

    starting_func = False
    regexp = r'(\w(\w|::|\*|\&|\s)*)\('  # decls * & space::name( ...
    match_result = match(regexp, line)
    if match_result:
        # If the name is all caps and underscores, figure it's a macro and
        # ignore it, unless it's TEST or TEST_F.
        function_name = match_result.group(1).split()[-1]
        if function_name == 'TEST' or function_name == 'TEST_F' or (not match(r'[A-Z_]+$', function_name)):
            starting_func = True

    if starting_func:
        body_found = False
        for start_line_number in xrange(line_number, clean_lines.num_lines()):
            start_line = lines[start_line_number]
            joined_line += ' ' + start_line.lstrip()
            if search(r'(;|})', start_line):  # Declarations and trivial functions
                body_found = True
                break                              # ... ignore
            if search(r'{', start_line):
                body_found = True
                function = search(r'((\w|:)*)\(', line).group(1)
                if match(r'TEST', function):    # Handle TEST... macros
                    parameter_regexp = search(r'(\(.*\))', joined_line)
                    if parameter_regexp:             # Ignore bad syntax
                        function += parameter_regexp.group(1)
                else:
                    function += '()'
                function_state.begin(function)
                break
        if not body_found:
            # No body for the function (or evidence of a non-function) was found.
            error(filename, line_number, 'readability/fn_size', 5,
                  'Lint failed to find start of function body.')
    elif match(r'^\}\s*$', line):  # function end
        if not search(r'\bNOLINT\b', raw_line):
            function_state.check(error, filename, line_number)
        function_state.end()
    elif not match(r'^\s*$', line):
        function_state.count()  # Count non-blank/non-comment lines.


def check_spacing(filename, clean_lines, line_number, error):
    """Checks for the correctness of various spacing issues in the code.

    Things we check for: spaces around operators, spaces after
    if/for/while/switch, no spaces around parens in function calls, two
    spaces between code and comment, don't start a block with a blank
    line, don't end a function with a blank line, don't have too many
    blank lines in a row.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    raw = clean_lines.raw_lines
    line = raw[line_number]

    # Before nixing comments, check if the line is blank for no good
    # reason.  This includes the first line after a block is opened, and
    # blank lines at the end of a function (ie, right before a line like '}').
    if is_blank_line(line):
        elided = clean_lines.elided
        previous_line = elided[line_number - 1]
        previous_brace = previous_line.rfind('{')
        # FIXME: Don't complain if line before blank line, and line after,
        #        both start with alnums and are indented the same amount.
        #        This ignores whitespace at the start of a namespace block
        #        because those are not usually indented.
        if (previous_brace != -1 and previous_line[previous_brace:].find('}') == -1
            and previous_line[:previous_brace].find('namespace') == -1):
            # OK, we have a blank line at the start of a code block.  Before we
            # complain, we check if it is an exception to the rule: The previous
            # non-empty line has the parameters of a function header that are indented
            # 4 spaces (because they did not fit in a 80 column line when placed on
            # the same line as the function name).  We also check for the case where
            # the previous line is indented 6 spaces, which may happen when the
            # initializers of a constructor do not fit into a 80 column line.
            exception = False
            if match(r' {6}\w', previous_line):  # Initializer list?
                # We are looking for the opening column of initializer list, which
                # should be indented 4 spaces to cause 6 space indentation afterwards.
                search_position = line_number - 2
                while (search_position >= 0
                       and match(r' {6}\w', elided[search_position])):
                    search_position -= 1
                exception = (search_position >= 0
                             and elided[search_position][:5] == '    :')
            else:
                # Search for the function arguments or an initializer list.  We use a
                # simple heuristic here: If the line is indented 4 spaces; and we have a
                # closing paren, without the opening paren, followed by an opening brace
                # or colon (for initializer lists) we assume that it is the last line of
                # a function header.  If we have a colon indented 4 spaces, it is an
                # initializer list.
                exception = (match(r' {4}\w[^\(]*\)\s*(const\s*)?(\{\s*$|:)',
                                   previous_line)
                             or match(r' {4}:', previous_line))

            if not exception:
                error(filename, line_number, 'whitespace/blank_line', 2,
                      'Blank line at the start of a code block.  Is this needed?')
        # This doesn't ignore whitespace at the end of a namespace block
        # because that is too hard without pairing open/close braces;
        # however, a special exception is made for namespace closing
        # brackets which have a comment containing "namespace".
        #
        # Also, ignore blank lines at the end of a block in a long if-else
        # chain, like this:
        #   if (condition1) {
        #     // Something followed by a blank line
        #
        #   } else if (condition2) {
        #     // Something else
        #   }
        if line_number + 1 < clean_lines.num_lines():
            next_line = raw[line_number + 1]
            if (next_line
                and match(r'\s*}', next_line)
                and next_line.find('namespace') == -1
                and next_line.find('} else ') == -1):
                error(filename, line_number, 'whitespace/blank_line', 3,
                      'Blank line at the end of a code block.  Is this needed?')

    # Next, we complain if there's a comment too near the text
    comment_position = line.find('//')
    if comment_position != -1:
        # Check if the // may be in quotes.  If so, ignore it
        # Comparisons made explicit for clarity -- pylint: disable-msg=C6403
        if (line.count('"', 0, comment_position) - line.count('\\"', 0, comment_position)) % 2 == 0:   # not in quotes
            # Allow one space for new scopes, two spaces otherwise:
            if (not match(r'^\s*{ //', line)
                and ((comment_position >= 1
                      and line[comment_position-1] not in string.whitespace)
                     or (comment_position >= 2
                         and line[comment_position-2] not in string.whitespace))):
                error(filename, line_number, 'whitespace/comments-doublespace', 2,
                      'At least two spaces is best between code and comments')
            # There should always be a space between the // and the comment
            commentend = comment_position + 2
            if commentend < len(line) and not line[commentend] == ' ':
                # but some lines are exceptions -- e.g. if they're big
                # comment delimiters like:
                # //----------------------------------------------------------
                # or they begin with multiple slashes followed by a space:
                # //////// Header comment
                matched = (search(r'[=/-]{4,}\s*$', line[commentend:])
                           or search(r'^/+ ', line[commentend:]))
                if not matched:
                    error(filename, line_number, 'whitespace/comments', 4,
                          'Should have a space between // and comment')

    line = clean_lines.elided[line_number]  # get rid of comments and strings

    # Don't try to do spacing checks for operator methods
    line = re.sub(r'operator(==|!=|<|<<|<=|>=|>>|>)\(', 'operator\(', line)

    # We allow no-spaces around = within an if: "if ( (a=Foo()) == 0 )".
    # Otherwise not.  Note we only check for non-spaces on *both* sides;
    # sometimes people put non-spaces on one side when aligning ='s among
    # many lines (not that this is behavior that I approve of...)
    if search(r'[\w.]=[\w.]', line) and not search(r'\b(if|while) ', line):
        error(filename, line_number, 'whitespace/operators', 4,
              'Missing spaces around =')

    # FIXME: It's not ok to have spaces around binary operators like + - * / .

    # You should always have whitespace around binary operators.
    # Alas, we can't test < or > because they're legitimately used sans spaces
    # (a->b, vector<int> a).  The only time we can tell is a < with no >, and
    # only if it's not template params list spilling into the next line.
    matched = search(r'[^<>=!\s](==|!=|<=|>=)[^<>=!\s]', line)
    if not matched:
        # Note that while it seems that the '<[^<]*' term in the following
        # regexp could be simplified to '<.*', which would indeed match
        # the same class of strings, the [^<] means that searching for the
        # regexp takes linear rather than quadratic time.
        if not search(r'<[^<]*,\s*$', line):  # template params spill
            matched = search(r'[^<>=!\s](<)[^<>=!\s]([^>]|->)*$', line)
    if matched:
        error(filename, line_number, 'whitespace/operators', 3,
              'Missing spaces around %s' % matched.group(1))
    # We allow no-spaces around << and >> when used like this: 10<<20, but
    # not otherwise (particularly, not when used as streams)
    matched = search(r'[^0-9\s](<<|>>)[^0-9\s]', line)
    if matched:
        error(filename, line_number, 'whitespace/operators', 3,
              'Missing spaces around %s' % matched.group(1))

    # There shouldn't be space around unary operators
    matched = search(r'(!\s|~\s|[\s]--[\s;]|[\s]\+\+[\s;])', line)
    if matched:
        error(filename, line_number, 'whitespace/operators', 4,
              'Extra space for operator %s' % matched.group(1))

    # A pet peeve of mine: no spaces after an if, while, switch, or for
    matched = search(r' (if\(|for\(|foreach\(|while\(|switch\()', line)
    if matched:
        error(filename, line_number, 'whitespace/parens', 5,
              'Missing space before ( in %s' % matched.group(1))

    # For if/for/foreach/while/switch, the left and right parens should be
    # consistent about how many spaces are inside the parens, and
    # there should either be zero or one spaces inside the parens.
    # We don't want: "if ( foo)" or "if ( foo   )".
    # Exception: "for ( ; foo; bar)" and "for (foo; bar; )" are allowed.
    matched = search(r'\b(if|for|foreach|while|switch)\s*\(([ ]*)(.).*[^ ]+([ ]*)\)\s*{\s*$',
                     line)
    if matched:
        if len(matched.group(2)) != len(matched.group(4)):
            if not (matched.group(3) == ';'
                    and len(matched.group(2)) == 1 + len(matched.group(4))
                    or not matched.group(2) and search(r'\bfor\s*\(.*; \)', line)):
                error(filename, line_number, 'whitespace/parens', 5,
                      'Mismatching spaces inside () in %s' % matched.group(1))
        if not len(matched.group(2)) in [0, 1]:
            error(filename, line_number, 'whitespace/parens', 5,
                  'Should have zero or one spaces inside ( and ) in %s' %
                  matched.group(1))

    # You should always have a space after a comma (either as fn arg or operator)
    if search(r',[^\s]', line):
        error(filename, line_number, 'whitespace/comma', 3,
              'Missing space after ,')

    # Next we will look for issues with function calls.
    check_spacing_for_function_call(filename, line, line_number, error)

    # Except after an opening paren, you should have spaces before your braces.
    # And since you should never have braces at the beginning of a line, this is
    # an easy test.
    if search(r'[^ ({]{', line):
        error(filename, line_number, 'whitespace/braces', 5,
              'Missing space before {')

    # Make sure '} else {' has spaces.
    if search(r'}else', line):
        error(filename, line_number, 'whitespace/braces', 5,
              'Missing space before else')

    # You shouldn't have spaces before your brackets, except maybe after
    # 'delete []' or 'new char * []'.
    if search(r'\w\s+\[', line) and not search(r'delete\s+\[', line):
        error(filename, line_number, 'whitespace/braces', 5,
              'Extra space before [')

    # You shouldn't have a space before a semicolon at the end of the line.
    # There's a special case for "for" since the style guide allows space before
    # the semicolon there.
    if search(r':\s*;\s*$', line):
        error(filename, line_number, 'whitespace/semicolon', 5,
              'Semicolon defining empty statement. Use { } instead.')
    elif search(r'^\s*;\s*$', line):
        error(filename, line_number, 'whitespace/semicolon', 5,
              'Line contains only semicolon. If this should be an empty statement, '
              'use { } instead.')
    elif (search(r'\s+;\s*$', line) and not search(r'\bfor\b', line)):
        error(filename, line_number, 'whitespace/semicolon', 5,
              'Extra space before last semicolon. If this should be an empty '
              'statement, use { } instead.')
    elif (search(r'\b(for|while)\s*\(.*\)\s*;\s*$', line)
          and line.count('(') == line.count(')')
          # Allow do {} while();
          and not search(r'}\s*while', line)):
        error(filename, line_number, 'whitespace/semicolon', 5,
              'Semicolon defining empty statement for this loop. Use { } instead.')


def get_previous_non_blank_line(clean_lines, line_number):
    """Return the most recent non-blank line and its line number.

    Args:
      clean_lines: A CleansedLines instance containing the file contents.
      line_number: The number of the line to check.

    Returns:
      A tuple with two elements.  The first element is the contents of the last
      non-blank line before the current line, or the empty string if this is the
      first non-blank line.  The second is the line number of that line, or -1
      if this is the first non-blank line.
    """

    previous_line_number = line_number - 1
    while previous_line_number >= 0:
        previous_line = clean_lines.elided[previous_line_number]
        if not is_blank_line(previous_line):     # if not a blank line...
            return (previous_line, previous_line_number)
        previous_line_number -= 1
    return ('', -1)


def check_namespace_indentation(filename, clean_lines, line_number, file_extension, error):
    """Looks for indentation errors inside of namespaces.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (dot not included) of the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number] # Get rid of comments and strings.

    namespace_match = match(r'(?P<namespace_indentation>\s*)namespace\s+\S+\s*{\s*$', line)
    if not namespace_match:
        return

    namespace_indentation = namespace_match.group('namespace_indentation')

    is_header_file = file_extension == 'h'
    is_implementation_file = not is_header_file
    line_offset = 0

    if is_header_file:
        inner_indentation = namespace_indentation + ' ' * 4

        for current_line in clean_lines.raw_lines[line_number + 1:]:
            line_offset += 1

            # Skip not only empty lines but also those with preprocessor directives.
            # Goto labels don't occur in header files, so no need to check for those.
            if current_line.strip() == '' or current_line.startswith('#'):
                continue

            if not current_line.startswith(inner_indentation):
                # If something unindented was discovered, make sure it's a closing brace.
                if not current_line.startswith(namespace_indentation + '}'):
                    error(filename, line_number + line_offset, 'whitespace/indent', 4,
                          'In a header, code inside a namespace should be indented.')
                break

    if is_implementation_file:
        for current_line in clean_lines.raw_lines[line_number + 1:]:
            line_offset += 1

            # Skip not only empty lines but also those with (goto) labels.
            # The goto label regexp accepts spaces or the beginning of a
            # comment (if anything) after the initial colon.
            if current_line.strip() == '' or match(r'\w+\s*:([\s\/].*)?$', current_line):
                continue

            remaining_line = current_line[len(namespace_indentation):]
            if not match(r'\S', remaining_line):
                error(filename, line_number + line_offset, 'whitespace/indent', 4,
                      'In an implementation file, code inside a namespace should not be indented.')

            # Just check the first non-empty line in any case, because
            # otherwise we would need to count opened and closed braces,
            # which is obviously a lot more complicated.
            break


def check_switch_indentation(filename, clean_lines, line_number, error):
    """Looks for indentation errors inside of switch statements.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number] # Get rid of comments and strings.

    switch_match = match(r'(?P<switch_indentation>\s*)switch\s*\(.+\)\s*{\s*$', line)
    if not switch_match:
        return

    switch_indentation = switch_match.group('switch_indentation')
    inner_indentation = switch_indentation + ' ' * 4
    line_offset = 0
    encountered_nested_switch = False

    for current_line in clean_lines.elided[line_number + 1:]:
        line_offset += 1

        # Skip not only empty lines but also those with preprocessor directives.
        if current_line.strip() == '' or current_line.startswith('#'):
            continue

        if match(r'\s*switch\s*\(.+\)\s*{\s*$', current_line):
            # Complexity alarm - another switch statement nested inside the one
            # that we're currently testing. We'll need to track the extent of
            # that inner switch if the upcoming label tests are still supposed
            # to work correctly. Let's not do that; instead, we'll finish
            # checking this line, and then leave it like that. Assuming the
            # indentation is done consistently (even if incorrectly), this will
            # still catch all indentation issues in practice.
            encountered_nested_switch = True

        current_indentation_match = match(r'(?P<indentation>\s*)(?P<remaining_line>.*)$', current_line);
        current_indentation = current_indentation_match.group('indentation')
        remaining_line = current_indentation_match.group('remaining_line')

        # End the check at the end of the switch statement.
        if remaining_line.startswith('}') and current_indentation == switch_indentation:
            break
        # Case and default branches should not be indented. The regexp also
        # catches single-line cases like "default: break;" but does not trigger
        # on stuff like "Document::Foo();".
        elif match(r'(default|case\s+.*)\s*:([^:].*)?$', remaining_line):
            if current_indentation != switch_indentation:
                error(filename, line_number + line_offset, 'whitespace/indent', 4,
                      'A case label should not be indented, but line up with its switch statement.')
                # Don't throw an error for multiple badly indented labels,
                # one should be enough to figure out the problem.
                break
        # We ignore goto labels at the very beginning of a line.
        elif match(r'\w+\s*:\s*$', remaining_line):
            continue
        # It's not a goto label, so check if it's indented at least as far as
        # the switch statement plus one more level of indentation.
        elif not current_indentation.startswith(inner_indentation):
            error(filename, line_number + line_offset, 'whitespace/indent', 4,
                  'Non-label code inside switch statements should be indented.')
            # Don't throw an error for multiple badly indented statements,
            # one should be enough to figure out the problem.
            break

        if encountered_nested_switch:
            break


def check_braces(filename, clean_lines, line_number, error):
    """Looks for misplaced braces (e.g. at the end of line).

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number] # Get rid of comments and strings.

    """
    These don't match our style guideline:
    https://developer.mozilla.org/en-US/docs/Developer_Guide/Coding_Style#Control_Structures

    TODO: Spin this off in a different rule and disable that rule for mozilla
    rather then commenting this out


    if match(r'\s*{\s*$', line):
        # We allow an open brace to start a line in the case where someone
        # is using braces for function definition or in a block to
        # explicitly create a new scope, which is commonly used to control
        # the lifetime of stack-allocated variables.  We don't detect this
        # perfectly: we just don't complain if the last non-whitespace
        # character on the previous non-blank line is ';', ':', '{', '}',
        # ')', or ') const' and doesn't begin with 'if|for|while|switch|else'.
        # We also allow '#' for #endif and '=' for array initialization.
        previous_line = get_previous_non_blank_line(clean_lines, line_number)[0]
        if ((not search(r'[;:}{)=]\s*$|\)\s*const\s*$', previous_line)
             or search(r'\b(if|for|foreach|while|switch|else)\b', previous_line))
            and previous_line.find('#') < 0):
            error(filename, line_number, 'whitespace/braces', 4,
                  'This { should be at the end of the previous line')
    elif (search(r'\)\s*(const\s*)?{\s*$', line)
          and line.count('(') == line.count(')')
          and not search(r'\b(if|for|foreach|while|switch)\b', line)):
        error(filename, line_number, 'whitespace/braces', 4,
              'Place brace on its own line for function definitions.')

    if (match(r'\s*}\s*$', line) and line_number > 1):
        # We check if a closed brace has started a line to see if a
        # one line control statement was previous.
        previous_line = clean_lines.elided[line_number - 2]
        if (previous_line.find('{') > 0
            and search(r'\b(if|for|foreach|while|else)\b', previous_line)):
            error(filename, line_number, 'whitespace/braces', 4,
                  'One line control clauses should not use braces.')
    """

    # An else clause should be on the same line as the preceding closing brace.
    if match(r'\s*else\s*', line):
        previous_line = get_previous_non_blank_line(clean_lines, line_number)[0]
        if match(r'\s*}\s*$', previous_line):
            error(filename, line_number, 'whitespace/newline', 4,
                  'An else should appear on the same line as the preceding }')

    # Likewise, an else should never have the else clause on the same line
    if search(r'\belse [^\s{]', line) and not search(r'\belse if\b', line):
        error(filename, line_number, 'whitespace/newline', 4,
              'Else clause should never be on same line as else (use 2 lines)')

    # In the same way, a do/while should never be on one line
    if match(r'\s*do [^\s{]', line):
        error(filename, line_number, 'whitespace/newline', 4,
              'do/while clauses should not be on a single line')

    # Braces shouldn't be followed by a ; unless they're defining a struct
    # or initializing an array.
    # We can't tell in general, but we can for some common cases.
    previous_line_number = line_number
    while True:
        (previous_line, previous_line_number) = get_previous_non_blank_line(clean_lines, previous_line_number)
        if match(r'\s+{.*}\s*;', line) and not previous_line.count(';'):
            line = previous_line + line
        else:
            break
    if (search(r'{.*}\s*;', line)
        and line.count('{') == line.count('}')
        and not search(r'struct|class|enum|\s*=\s*{', line)):
        error(filename, line_number, 'readability/braces', 4,
              "You don't need a ; after a }")


def check_exit_statement_simplifications(filename, clean_lines, line_number, error):
    """Looks for else or else-if statements that should be written as an
    if statement when the prior if concludes with a return, break, continue or
    goto statement.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number] # Get rid of comments and strings.

    else_match = match(r'(?P<else_indentation>\s*)(\}\s*)?else(\s+if\s*\(|(?P<else>\s*(\{\s*)?\Z))', line)
    if not else_match:
        return

    else_indentation = else_match.group('else_indentation')
    inner_indentation = else_indentation + ' ' * 4

    previous_lines = clean_lines.elided[:line_number]
    previous_lines.reverse()
    line_offset = 0
    encountered_exit_statement = False

    for current_line in previous_lines:
        line_offset -= 1

        # Skip not only empty lines but also those with preprocessor directives
        # and goto labels.
        if current_line.strip() == '' or current_line.startswith('#') or match(r'\w+\s*:\s*$', current_line):
            continue

        # Skip lines with closing braces on the original indentation level.
        # Even though the styleguide says they should be on the same line as
        # the "else if" statement, we also want to check for instances where
        # the current code does not comply with the coding style. Thus, ignore
        # these lines and proceed to the line before that.
        if current_line == else_indentation + '}':
            continue

        current_indentation_match = match(r'(?P<indentation>\s*)(?P<remaining_line>.*)$', current_line);
        current_indentation = current_indentation_match.group('indentation')
        remaining_line = current_indentation_match.group('remaining_line')

        # As we're going up the lines, the first real statement to encounter
        # has to be an exit statement (return, break, continue or goto) -
        # otherwise, this check doesn't apply.
        if not encountered_exit_statement:
            # We only want to find exit statements if they are on exactly
            # the same level of indentation as expected from the code inside
            # the block. If the indentation doesn't strictly match then we
            # might have a nested if or something, which must be ignored.
            if current_indentation != inner_indentation:
                break
            if match(r'(return(\W+.*)|(break|continue)\s*;|goto\s*\w+;)$', remaining_line):
                encountered_exit_statement = True
                continue
            break

        # When code execution reaches this point, we've found an exit statement
        # as last statement of the previous block. Now we only need to make
        # sure that the block belongs to an "if", then we can throw an error.

        # Skip lines with opening braces on the original indentation level,
        # similar to the closing braces check above. ("if (condition)\n{")
        if current_line == else_indentation + '{':
            continue

        # Skip everything that's further indented than our "else" or "else if".
        if current_indentation.startswith(else_indentation) and current_indentation != else_indentation:
            continue

        # So we've got a line with same (or less) indentation. Is it an "if"?
        # If yes: throw an error. If no: don't throw an error.
        # Whatever the outcome, this is the end of our loop.
        if match(r'if\s*\(', remaining_line):
            if else_match.start('else') != -1:
                error(filename, line_number + line_offset, 'readability/control_flow', 4,
                      'An else statement can be removed when the prior "if" '
                      'concludes with a return, break, continue or goto statement.')
            else:
                error(filename, line_number + line_offset, 'readability/control_flow', 4,
                      'An else if statement should be written as an if statement '
                      'when the prior "if" concludes with a return, break, '
                      'continue or goto statement.')
        break


def replaceable_check(operator, macro, line):
    """Determine whether a basic CHECK can be replaced with a more specific one.

    For example suggest using CHECK_EQ instead of CHECK(a == b) and
    similarly for CHECK_GE, CHECK_GT, CHECK_LE, CHECK_LT, CHECK_NE.

    Args:
      operator: The C++ operator used in the CHECK.
      macro: The CHECK or EXPECT macro being called.
      line: The current source line.

    Returns:
      True if the CHECK can be replaced with a more specific one.
    """

    # This matches decimal and hex integers, strings, and chars (in that order).
    match_constant = r'([-+]?(\d+|0[xX][0-9a-fA-F]+)[lLuU]{0,3}|".*"|\'.*\')'

    # Expression to match two sides of the operator with something that
    # looks like a literal, since CHECK(x == iterator) won't compile.
    # This means we can't catch all the cases where a more specific
    # CHECK is possible, but it's less annoying than dealing with
    # extraneous warnings.
    match_this = (r'\s*' + macro + r'\((\s*' +
                  match_constant + r'\s*' + operator + r'[^<>].*|'
                  r'.*[^<>]' + operator + r'\s*' + match_constant +
                  r'\s*\))')

    # Don't complain about CHECK(x == NULL) or similar because
    # CHECK_EQ(x, NULL) won't compile (requires a cast).
    # Also, don't complain about more complex boolean expressions
    # involving && or || such as CHECK(a == b || c == d).
    return match(match_this, line) and not search(r'NULL|&&|\|\|', line)


def check_check(filename, clean_lines, line_number, error):
    """Checks the use of CHECK and EXPECT macros.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    # Decide the set of replacement macros that should be suggested
    raw_lines = clean_lines.raw_lines
    current_macro = ''
    for macro in _CHECK_MACROS:
        if raw_lines[line_number].find(macro) >= 0:
            current_macro = macro
            break
    if not current_macro:
        # Don't waste time here if line doesn't contain 'CHECK' or 'EXPECT'
        return

    line = clean_lines.elided[line_number]        # get rid of comments and strings

    # Encourage replacing plain CHECKs with CHECK_EQ/CHECK_NE/etc.
    for operator in ['==', '!=', '>=', '>', '<=', '<']:
        if replaceable_check(operator, current_macro, line):
            error(filename, line_number, 'readability/check', 2,
                  'Consider using %s instead of %s(a %s b)' % (
                      _CHECK_REPLACEMENT[current_macro][operator],
                      current_macro, operator))
            break


def check_for_comparisons_to_zero(filename, clean_lines, line_number, error):
    # Get the line without comments and strings.
    line = clean_lines.elided[line_number]

    # Include NULL here so that users don't have to convert NULL to 0 first and then get this error.
    if search(r'[=!]=\s*(NULL|0|true|false)\W', line) or search(r'\W(NULL|0|true|false)\s*[=!]=', line):
        error(filename, line_number, 'readability/comparison_to_zero', 5,
              'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.')


def check_for_null(filename, clean_lines, line_number, error):
    # This check doesn't apply to C or Objective-C implementation files.
    if filename.endswith('.c') or filename.endswith('.m'):
        return

    line = clean_lines.elided[line_number]
    if search(r'\bNULL\b', line):
        error(filename, line_number, 'readability/null', 5, 'Use 0 instead of NULL.')
        return

    line = clean_lines.raw_lines[line_number]
    # See if NULL occurs in any comments in the line. If the search for NULL using the raw line
    # matches, then do the check with strings collapsed to avoid giving errors for
    # NULLs occurring in strings.
    if search(r'\bNULL\b', line) and search(r'\bNULL\b', CleansedLines.collapse_strings(line)):
        error(filename, line_number, 'readability/null', 4, 'Use 0 instead of NULL.')

def get_line_width(line):
    """Determines the width of the line in column positions.

    Args:
      line: A string, which may be a Unicode string.

    Returns:
      The width of the line in column positions, accounting for Unicode
      combining characters and wide characters.
    """
    if isinstance(line, unicode):
        width = 0
        for c in unicodedata.normalize('NFC', line):
            if unicodedata.east_asian_width(c) in ('W', 'F'):
                width += 2
            elif not unicodedata.combining(c):
                width += 1
        return width
    return len(line)


def check_style(filename, clean_lines, line_number, file_extension, error):
    """Checks rules from the 'C++ style rules' section of cppguide.html.

    Most of these rules are hard to test (naming, comment style), but we
    do what we can.  In particular we check for 4-space indents, line lengths,
    tab usage, spaces inside code, etc.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (without the dot) of the filename.
      error: The function to call with any errors found.
    """

    raw_lines = clean_lines.raw_lines
    line = raw_lines[line_number]

    if line.find('\t') != -1:
        error(filename, line_number, 'whitespace/tab', 1,
              'Tab found; better to use spaces')

    # One or three blank spaces at the beginning of the line is weird; it's
    # hard to reconcile that with 4-space indents.
    # NOTE: here are the conditions rob pike used for his tests.  Mine aren't
    # as sophisticated, but it may be worth becoming so:  RLENGTH==initial_spaces
    # if(RLENGTH > 20) complain = 0;
    # if(match($0, " +(error|private|public|protected):")) complain = 0;
    # if(match(prev, "&& *$")) complain = 0;
    # if(match(prev, "\\|\\| *$")) complain = 0;
    # if(match(prev, "[\",=><] *$")) complain = 0;
    # if(match($0, " <<")) complain = 0;
    # if(match(prev, " +for \\(")) complain = 0;
    # if(prevodd && match(prevprev, " +for \\(")) complain = 0;
    initial_spaces = 0
    cleansed_line = clean_lines.elided[line_number]
    while initial_spaces < len(line) and line[initial_spaces] == ' ':
        initial_spaces += 1
    if line and line[-1].isspace():
        error(filename, line_number, 'whitespace/end_of_line', 4,
              'Line ends in whitespace.  Consider deleting these extra spaces.')
    # There are certain situations we allow one space, notably for labels
    elif ((initial_spaces == 1 or initial_spaces == 3)
          and not match(r'\s*\w+\s*:\s*$', cleansed_line)):
        error(filename, line_number, 'whitespace/indent', 3,
              'Weird number of spaces at line-start.  '
              'Are you using at least 2-space indent?')
    # Labels should always be indented at least one space.
    elif not initial_spaces and line[:2] != '//':
        label_match = match(r'(?P<label>[^:]+):\s*$', line)

        if label_match:
            label = label_match.group('label')
            # Only throw errors for stuff that is definitely not a goto label,
            # because goto labels can in fact occur at the start of the line.
            if label in ['public', 'private', 'protected'] or label.find(' ') != -1:
                error(filename, line_number, 'whitespace/labels', 4,
                      'Labels should always be indented at least one space.  '
                      'If this is a member-initializer list in a constructor, '
                      'the colon should be on the line after the definition header.')

    if (cleansed_line.count(';') > 1
        # for loops are allowed two ;'s (and may run over two lines).
        and cleansed_line.find('for') == -1
        and (get_previous_non_blank_line(clean_lines, line_number)[0].find('for') == -1
             or get_previous_non_blank_line(clean_lines, line_number)[0].find(';') != -1)
        # It's ok to have many commands in a switch case that fits in 1 line
        and not ((cleansed_line.find('case ') != -1
                  or cleansed_line.find('default:') != -1)
                 and cleansed_line.find('break;') != -1)):
        error(filename, line_number, 'whitespace/newline', 4,
              'More than one command on the same line')

    if cleansed_line.strip().endswith('||') or cleansed_line.strip().endswith('&&'):
        error(filename, line_number, 'whitespace/operators', 4,
              'Boolean expressions that span multiple lines should have their '
              'operators on the left side of the line instead of the right side.')

    # Some more style checks
    check_namespace_indentation(filename, clean_lines, line_number, file_extension, error)
    check_switch_indentation(filename, clean_lines, line_number, error)
    check_braces(filename, clean_lines, line_number, error)
    check_exit_statement_simplifications(filename, clean_lines, line_number, error)
    check_spacing(filename, clean_lines, line_number, error)
    check_check(filename, clean_lines, line_number, error)
    check_for_comparisons_to_zero(filename, clean_lines, line_number, error)
    check_for_null(filename, clean_lines, line_number, error)


_RE_PATTERN_INCLUDE_NEW_STYLE = re.compile(r'#include +"[^/]+\.h"')
_RE_PATTERN_INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]*)[>"].*$')
# Matches the first component of a filename delimited by -s and _s. That is:
#  _RE_FIRST_COMPONENT.match('foo').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo.cpp').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo-bar_baz.cpp').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo_bar-baz.cpp').group(0) == 'foo'
_RE_FIRST_COMPONENT = re.compile(r'^[^-_.]+')


def _drop_common_suffixes(filename):
    """Drops common suffixes like _test.cpp or -inl.h from filename.

    For example:
      >>> _drop_common_suffixes('foo/foo-inl.h')
      'foo/foo'
      >>> _drop_common_suffixes('foo/bar/foo.cpp')
      'foo/bar/foo'
      >>> _drop_common_suffixes('foo/foo_internal.h')
      'foo/foo'
      >>> _drop_common_suffixes('foo/foo_unusualinternal.h')
      'foo/foo_unusualinternal'

    Args:
      filename: The input filename.

    Returns:
      The filename with the common suffix removed.
    """
    for suffix in ('test.cpp', 'regtest.cpp', 'unittest.cpp',
                   'inl.h', 'impl.h', 'internal.h'):
        if (filename.endswith(suffix) and len(filename) > len(suffix)
            and filename[-len(suffix) - 1] in ('-', '_')):
            return filename[:-len(suffix) - 1]
    return os.path.splitext(filename)[0]


def _is_test_filename(filename):
    """Determines if the given filename has a suffix that identifies it as a test.

    Args:
      filename: The input filename.

    Returns:
      True if 'filename' looks like a test, False otherwise.
    """
    if (filename.endswith('_test.cpp')
        or filename.endswith('_unittest.cpp')
        or filename.endswith('_regtest.cpp')):
        return True
    return False


def _classify_include(filename, include, is_system, include_state):
    """Figures out what kind of header 'include' is.

    Args:
      filename: The current file cpplint is running over.
      include: The path to a #included file.
      is_system: True if the #include used <> rather than "".
      include_state: An _IncludeState instance in which the headers are inserted.

    Returns:
      One of the _XXX_HEADER constants.

    For example:
      >>> _classify_include('foo.cpp', 'config.h', False)
      _CONFIG_HEADER
      >>> _classify_include('foo.cpp', 'foo.h', False)
      _PRIMARY_HEADER
      >>> _classify_include('foo.cpp', 'bar.h', False)
      _OTHER_HEADER
    """

    # If it is a system header we know it is classified as _OTHER_HEADER.
    if is_system:
        return _OTHER_HEADER

    # If the include is named config.h then this is WebCore/config.h.
    if include == "config.h":
        return _CONFIG_HEADER

    # There cannot be primary includes in header files themselves. Only an
    # include exactly matches the header filename will be is flagged as
    # primary, so that it triggers the "don't include yourself" check.
    if filename.endswith('.h') and filename != include:
        return _OTHER_HEADER;

    # If the target file basename starts with the include we're checking
    # then we consider it the primary header.
    target_base = FileInfo(filename).base_name()
    include_base = FileInfo(include).base_name()

    # If we haven't encountered a primary header, then be lenient in checking.
    if not include_state.visited_primary_section() and target_base.startswith(include_base):
        return _PRIMARY_HEADER
    # If we already encountered a primary header, perform a strict comparison.
    # In case the two filename bases are the same then the above lenient check
    # probably was a false positive.
    elif include_state.visited_primary_section() and target_base == include_base:
        return _PRIMARY_HEADER

    return _OTHER_HEADER



def check_include_line(filename, clean_lines, line_number, include_state, error):
    """Check rules that are applicable to #include lines.

    Strings on #include lines are NOT removed from elided line, to make
    certain tasks easier. However, to prevent false positives, checks
    applicable to #include lines in CheckLanguage must be put here.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      include_state: An _IncludeState instance in which the headers are inserted.
      error: The function to call with any errors found.
    """

    line = clean_lines.lines[line_number]

    # we shouldn't include a file more than once. actually, there are a
    # handful of instances where doing so is okay, but in general it's
    # not.
    matched = _RE_PATTERN_INCLUDE.search(line)
    if matched:
        include = matched.group(2)
        is_system = (matched.group(1) == '<')
        if include in include_state:
            error(filename, line_number, 'build/include', 4,
                  '"%s" already included at %s:%s' %
                  (include, filename, include_state[include]))
        else:
            include_state[include] = line_number

            # We want to ensure that headers appear in the right order:
            # 1) for implementation files: config.h, primary header, blank line, alphabetically sorted
            # 2) for header files: alphabetically sorted
            #
            # We classify each include statement as one of 4 types
            # using a number of techniques. The include_state object keeps
            # track of the highest type seen, and complains if we see a
            # lower type after that.
            header_type = _classify_include(filename, include, is_system, include_state)
            error_message = include_state.check_next_include_order(header_type, filename.endswith('.h'))
            include_state.header_types[line_number] = header_type

            # Check to make sure we have a blank line after primary header.
            if not error_message and header_type == _PRIMARY_HEADER:
                 next_line = clean_lines.raw_lines[line_number + 1]
                 if not is_blank_line(next_line):
                    error(filename, line_number, 'build/include_order', 4,
                          'You should add a blank line after implementation file\'s own header.')

            # Check to make sure all headers besides config.h and the primary header are
            # alphabetically sorted.
            if not error_message and header_type == _OTHER_HEADER:
                 previous_line_number = line_number - 1;
                 previous_line = clean_lines.lines[previous_line_number]
                 previous_match = _RE_PATTERN_INCLUDE.search(previous_line)
                 while (not previous_match and previous_line_number > 0
                        and not search(r'\A(#if|#ifdef|#ifndef|#else|#elif|#endif)', previous_line)):
                    previous_line_number -= 1;
                    previous_line = clean_lines.lines[previous_line_number]
                    previous_match = _RE_PATTERN_INCLUDE.search(previous_line)
                 if previous_match:
                    previous_header_type = include_state.header_types[previous_line_number]
                    if previous_header_type == _OTHER_HEADER and previous_line.strip() > line.strip():
                        error(filename, line_number, 'build/include_order', 4,
                              'Alphabetical sorting problem.')

            if error_message:
                if filename.endswith('.h'):
                    error(filename, line_number, 'build/include_order', 4,
                          '%s Should be: alphabetically sorted.' %
                          error_message)
                else:
                    error(filename, line_number, 'build/include_order', 4,
                          '%s Should be: config.h, primary header, blank line, and then alphabetically sorted.' %
                          error_message)

        # Look for any of the stream classes that are part of standard C++.
        if match(r'(f|ind|io|i|o|parse|pf|stdio|str|)?stream$', include):
            # Many unit tests use cout, so we exempt them.
            if not _is_test_filename(filename):
                error(filename, line_number, 'readability/streams', 3,
                      'Streams are highly discouraged.')

        # Look for specific includes to fix.
        if include.startswith('wtf/') and not is_system:
            error(filename, line_number, 'build/include', 4,
                  'wtf includes should be <wtf/file.h> instead of "wtf/file.h".')


def check_language(filename, clean_lines, line_number, file_extension, include_state,
                   error):
    """Checks rules from the 'C++ language rules' section of cppguide.html.

    Some of these rules are hard to test (function overloading, using
    uint32 inappropriately), but we do the best we can.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (without the dot) of the filename.
      include_state: An _IncludeState instance in which the headers are inserted.
      error: The function to call with any errors found.
    """
    # If the line is empty or consists of entirely a comment, no need to
    # check it.
    line = clean_lines.elided[line_number]
    if not line:
        return

    matched = _RE_PATTERN_INCLUDE.search(line)
    if matched:
        check_include_line(filename, clean_lines, line_number, include_state, error)
        return

    # FIXME: figure out if they're using default arguments in fn proto.

    # Check to see if they're using an conversion function cast.
    # I just try to capture the most common basic types, though there are more.
    # Parameterless conversion functions, such as bool(), are allowed as they are
    # probably a member operator declaration or default constructor.
    matched = search(
        r'\b(int|float|double|bool|char|int32|uint32|int64|uint64)\([^)]', line)
    if matched:
        # gMock methods are defined using some variant of MOCK_METHODx(name, type)
        # where type may be float(), int(string), etc.  Without context they are
        # virtually indistinguishable from int(x) casts.
        if not match(r'^\s*MOCK_(CONST_)?METHOD\d+(_T)?\(', line):
            error(filename, line_number, 'readability/casting', 4,
                  'Using deprecated casting style.  '
                  'Use static_cast<%s>(...) instead' %
                  matched.group(1))

    check_c_style_cast(filename, line_number, line, clean_lines.raw_lines[line_number],
                       'static_cast',
                       r'\((int|float|double|bool|char|u?int(16|32|64))\)',
                       error)
    # This doesn't catch all cases.  Consider (const char * const)"hello".
    check_c_style_cast(filename, line_number, line, clean_lines.raw_lines[line_number],
                       'reinterpret_cast', r'\((\w+\s?\*+\s?)\)', error)

    # In addition, we look for people taking the address of a cast.  This
    # is dangerous -- casts can assign to temporaries, so the pointer doesn't
    # point where you think.
    """
    if search(
        r'(&\([^)]+\)[\w(])|(&(static|dynamic|reinterpret)_cast\b)', line):
        error(filename, line_number, 'runtime/casting', 4,
              ('Are you taking an address of a cast?  '
               'This is dangerous: could be a temp var.  '
               'Take the address before doing the cast, rather than after'))
    """

    # Check for people declaring static/global STL strings at the top level.
    # This is dangerous because the C++ language does not guarantee that
    # globals with constructors are initialized before the first access.
    matched = match(
        r'((?:|static +)(?:|const +))string +([a-zA-Z0-9_:]+)\b(.*)',
        line)
    # Make sure it's not a function.
    # Function template specialization looks like: "string foo<Type>(...".
    # Class template definitions look like: "string Foo<Type>::Method(...".
    if matched and not match(r'\s*(<.*>)?(::[a-zA-Z0-9_]+)?\s*\(([^"]|$)',
                             matched.group(3)):
        error(filename, line_number, 'runtime/string', 4,
              'For a static/global string constant, use a C style string instead: '
              '"%schar %s[]".' %
              (matched.group(1), matched.group(2)))

    # Check that we're not using RTTI outside of testing code.
    if search(r'\bdynamic_cast<', line) and not _is_test_filename(filename):
        error(filename, line_number, 'runtime/rtti', 5,
              'Do not use dynamic_cast<>.  If you need to cast within a class '
              "hierarchy, use static_cast<> to upcast.  Mozilla doesn't support "
              'RTTI.')

    if search(r'\b([A-Za-z0-9_]*_)\(\1\)', line):
        error(filename, line_number, 'runtime/init', 4,
              'You seem to be initializing a member variable with itself.')

    if file_extension == 'h':
        # FIXME: check that 1-arg constructors are explicit.
        #        How to tell it's a constructor?
        #        (handled in check_for_non_standard_constructs for now)
        pass

    # Check if people are using the verboten C basic types.  The only exception
    # we regularly allow is "unsigned short port" for port.
    if search(r'\bshort port\b', line):
        if not search(r'\bunsigned short port\b', line):
            error(filename, line_number, 'runtime/int', 4,
                  'Use "unsigned short" for ports, not "short"')

    # When snprintf is used, the second argument shouldn't be a literal.
    matched = search(r'snprintf\s*\(([^,]*),\s*([0-9]*)\s*,', line)
    if matched:
        error(filename, line_number, 'runtime/printf', 3,
              'If you can, use sizeof(%s) instead of %s as the 2nd arg '
              'to snprintf.' % (matched.group(1), matched.group(2)))

    # Check if some verboten C functions are being used.
    if search(r'\bsprintf\b', line):
        error(filename, line_number, 'runtime/printf', 5,
              'Never use sprintf.  Use snprintf instead.')
    matched = search(r'\b(strcpy|strcat)\b', line)
    if matched:
        error(filename, line_number, 'runtime/printf', 4,
              'Almost always, snprintf is better than %s' % matched.group(1))

    if search(r'\bsscanf\b', line):
        error(filename, line_number, 'runtime/printf', 1,
              'sscanf can be ok, but is slow and can overflow buffers.')

    # Check for suspicious usage of "if" like
    # } if (a == b) {
    if search(r'\}\s*if\s*\(', line):
        error(filename, line_number, 'readability/braces', 4,
              'Did you mean "else if"? If not, start a new line for "if".')

    # Check for potential format string bugs like printf(foo).
    # We constrain the pattern not to pick things like DocidForPrintf(foo).
    # Not perfect but it can catch printf(foo.c_str()) and printf(foo->c_str())
    matched = re.search(r'\b((?:string)?printf)\s*\(([\w.\->()]+)\)', line, re.I)
    if matched:
        error(filename, line_number, 'runtime/printf', 4,
              'Potential format string bug. Do %s("%%s", %s) instead.'
              % (matched.group(1), matched.group(2)))

    # Check for potential memset bugs like memset(buf, sizeof(buf), 0).
    matched = search(r'memset\s*\(([^,]*),\s*([^,]*),\s*0\s*\)', line)
    if matched and not match(r"^''|-?[0-9]+|0x[0-9A-Fa-f]$", matched.group(2)):
        error(filename, line_number, 'runtime/memset', 4,
              'Did you mean "memset(%s, 0, %s)"?'
              % (matched.group(1), matched.group(2)))

    # Detect variable-length arrays.
    matched = match(r'\s*(.+::)?(\w+) [a-z]\w*\[(.+)];', line)
    if (matched and matched.group(2) != 'return' and matched.group(2) != 'delete' and
        matched.group(3).find(']') == -1):
        # Split the size using space and arithmetic operators as delimiters.
        # If any of the resulting tokens are not compile time constants then
        # report the error.
        tokens = re.split(r'\s|\+|\-|\*|\/|<<|>>]', matched.group(3))
        is_const = True
        skip_next = False
        for tok in tokens:
            if skip_next:
                skip_next = False
                continue

            if search(r'sizeof\(.+\)', tok):
                continue
            if search(r'arraysize\(\w+\)', tok):
                continue

            tok = tok.lstrip('(')
            tok = tok.rstrip(')')
            if not tok:
                continue
            if match(r'\d+', tok):
                continue
            if match(r'0[xX][0-9a-fA-F]+', tok):
                continue
            if match(r'k[A-Z0-9]\w*', tok):
                continue
            if match(r'(.+::)?k[A-Z0-9]\w*', tok):
                continue
            if match(r'(.+::)?[A-Z][A-Z0-9_]*', tok):
                continue
            # A catch all for tricky sizeof cases, including 'sizeof expression',
            # 'sizeof(*type)', 'sizeof(const type)', 'sizeof(struct StructName)'
            # requires skipping the next token becasue we split on ' ' and '*'.
            if tok.startswith('sizeof'):
                skip_next = True
                continue
            is_const = False
            break
        if not is_const:
            error(filename, line_number, 'runtime/arrays', 1,
                  'Do not use variable-length arrays.  Use an appropriately named '
                  "('k' followed by CamelCase) compile-time constant for the size.")

    # Check for use of unnamed namespaces in header files.  Registration
    # macros are typically OK, so we allow use of "namespace {" on lines
    # that end with backslashes.
    if (file_extension == 'h'
        and search(r'\bnamespace\s*{', line)
        and line[-1] != '\\'):
        error(filename, line_number, 'build/namespaces', 4,
              'Do not use unnamed namespaces in header files.  See '
              'http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Namespaces'
              ' for more information.')


def check_c_style_cast(filename, line_number, line, raw_line, cast_type, pattern,
                       error):
    """Checks for a C-style cast by looking for the pattern.

    This also handles sizeof(type) warnings, due to similarity of content.

    Args:
      filename: The name of the current file.
      line_number: The number of the line to check.
      line: The line of code to check.
      raw_line: The raw line of code to check, with comments.
      cast_type: The string for the C++ cast to recommend.  This is either
                 reinterpret_cast or static_cast, depending.
      pattern: The regular expression used to find C-style casts.
      error: The function to call with any errors found.
    """
    matched = search(pattern, line)
    if not matched:
        return

    # e.g., sizeof(int)
    sizeof_match = match(r'.*sizeof\s*$', line[0:matched.start(1) - 1])
    if sizeof_match:
        error(filename, line_number, 'runtime/sizeof', 1,
              'Using sizeof(type).  Use sizeof(varname) instead if possible')
        return

    remainder = line[matched.end(0):]

    # The close paren is for function pointers as arguments to a function.
    # eg, void foo(void (*bar)(int));
    # The semicolon check is a more basic function check; also possibly a
    # function pointer typedef.
    # eg, void foo(int); or void foo(int) const;
    # The equals check is for function pointer assignment.
    # eg, void *(*foo)(int) = ...
    #
    # Right now, this will only catch cases where there's a single argument, and
    # it's unnamed.  It should probably be expanded to check for multiple
    # arguments with some unnamed.
    function_match = match(r'\s*(\)|=|(const)?\s*(;|\{|throw\(\)))', remainder)
    if function_match:
        if (not function_match.group(3)
            or function_match.group(3) == ';'
            or raw_line.find('/*') < 0):
            error(filename, line_number, 'readability/function', 3,
                  'All parameters should be named in a function')
        return

    # At this point, all that should be left is actual casts.
    error(filename, line_number, 'readability/casting', 4,
          'Using C-style cast.  Use %s<%s>(...) instead' %
          (cast_type, matched.group(1)))


_HEADERS_CONTAINING_TEMPLATES = (
    ('<deque>', ('deque',)),
    ('<functional>', ('unary_function', 'binary_function',
                      'plus', 'minus', 'multiplies', 'divides', 'modulus',
                      'negate',
                      'equal_to', 'not_equal_to', 'greater', 'less',
                      'greater_equal', 'less_equal',
                      'logical_and', 'logical_or', 'logical_not',
                      'unary_negate', 'not1', 'binary_negate', 'not2',
                      'bind1st', 'bind2nd',
                      'pointer_to_unary_function',
                      'pointer_to_binary_function',
                      'ptr_fun',
                      'mem_fun_t', 'mem_fun', 'mem_fun1_t', 'mem_fun1_ref_t',
                      'mem_fun_ref_t',
                      'const_mem_fun_t', 'const_mem_fun1_t',
                      'const_mem_fun_ref_t', 'const_mem_fun1_ref_t',
                      'mem_fun_ref',
                     )),
    ('<limits>', ('numeric_limits',)),
    ('<list>', ('list',)),
    ('<map>', ('map', 'multimap',)),
    ('<memory>', ('allocator',)),
    ('<queue>', ('queue', 'priority_queue',)),
    ('<set>', ('set', 'multiset',)),
    ('<stack>', ('stack',)),
    ('<string>', ('char_traits', 'basic_string',)),
    ('<utility>', ('pair',)),
    ('<vector>', ('vector',)),

    # gcc extensions.
    # Note: std::hash is their hash, ::hash is our hash
    ('<hash_map>', ('hash_map', 'hash_multimap',)),
    ('<hash_set>', ('hash_set', 'hash_multiset',)),
    ('<slist>', ('slist',)),
    )

_HEADERS_ACCEPTED_BUT_NOT_PROMOTED = {
    # We can trust with reasonable confidence that map gives us pair<>, too.
    'pair<>': ('map', 'multimap', 'hash_map', 'hash_multimap')
}

_RE_PATTERN_STRING = re.compile(r'\bstring\b')

_re_pattern_algorithm_header = []
for _template in ('copy', 'max', 'min', 'min_element', 'sort', 'swap',
                  'transform'):
    # Match max<type>(..., ...), max(..., ...), but not foo->max, foo.max or
    # type::max().
    _re_pattern_algorithm_header.append(
        (re.compile(r'[^>.]\b' + _template + r'(<.*?>)?\([^\)]'),
         _template,
         '<algorithm>'))

_re_pattern_templates = []
for _header, _templates in _HEADERS_CONTAINING_TEMPLATES:
    for _template in _templates:
        _re_pattern_templates.append(
            (re.compile(r'(\<|\b)' + _template + r'\s*\<'),
             _template + '<>',
             _header))


def files_belong_to_same_module(filename_cpp, filename_h):
    """Check if these two filenames belong to the same module.

    The concept of a 'module' here is a as follows:
    foo.h, foo-inl.h, foo.cpp, foo_test.cpp and foo_unittest.cpp belong to the
    same 'module' if they are in the same directory.
    some/path/public/xyzzy and some/path/internal/xyzzy are also considered
    to belong to the same module here.

    If the filename_cpp contains a longer path than the filename_h, for example,
    '/absolute/path/to/base/sysinfo.cpp', and this file would include
    'base/sysinfo.h', this function also produces the prefix needed to open the
    header. This is used by the caller of this function to more robustly open the
    header file. We don't have access to the real include paths in this context,
    so we need this guesswork here.

    Known bugs: tools/base/bar.cpp and base/bar.h belong to the same module
    according to this implementation. Because of this, this function gives
    some false positives. This should be sufficiently rare in practice.

    Args:
      filename_cpp: is the path for the .cpp file
      filename_h: is the path for the header path

    Returns:
      Tuple with a bool and a string:
      bool: True if filename_cpp and filename_h belong to the same module.
      string: the additional prefix needed to open the header file.
    """

    if not filename_cpp.endswith('.cpp'):
        return (False, '')
    filename_cpp = filename_cpp[:-len('.cpp')]
    if filename_cpp.endswith('_unittest'):
        filename_cpp = filename_cpp[:-len('_unittest')]
    elif filename_cpp.endswith('_test'):
        filename_cpp = filename_cpp[:-len('_test')]
    filename_cpp = filename_cpp.replace('/public/', '/')
    filename_cpp = filename_cpp.replace('/internal/', '/')

    if not filename_h.endswith('.h'):
        return (False, '')
    filename_h = filename_h[:-len('.h')]
    if filename_h.endswith('-inl'):
        filename_h = filename_h[:-len('-inl')]
    filename_h = filename_h.replace('/public/', '/')
    filename_h = filename_h.replace('/internal/', '/')

    files_belong_to_same_module = filename_cpp.endswith(filename_h)
    common_path = ''
    if files_belong_to_same_module:
        common_path = filename_cpp[:-len(filename_h)]
    return files_belong_to_same_module, common_path


def update_include_state(filename, include_state, io=codecs):
    """Fill up the include_state with new includes found from the file.

    Args:
      filename: the name of the header to read.
      include_state: an _IncludeState instance in which the headers are inserted.
      io: The io factory to use to read the file. Provided for testability.

    Returns:
      True if a header was succesfully added. False otherwise.
    """
    header_file = None
    try:
        header_file = io.open(filename, 'r', 'utf8', 'replace')
    except IOError:
        return False
    line_number = 0
    for line in header_file:
        line_number += 1
        clean_line = cleanse_comments(line)
        matched = _RE_PATTERN_INCLUDE.search(clean_line)
        if matched:
            include = matched.group(2)
            # The value formatting is cute, but not really used right now.
            # What matters here is that the key is in include_state.
            include_state.setdefault(include, '%s:%d' % (filename, line_number))
    return True


def check_for_include_what_you_use(filename, clean_lines, include_state, error,
                                   io=codecs):
    """Reports for missing stl includes.

    This function will output warnings to make sure you are including the headers
    necessary for the stl containers and functions that you use. We only give one
    reason to include a header. For example, if you use both equal_to<> and
    less<> in a .h file, only one (the latter in the file) of these will be
    reported as a reason to include the <functional>.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      include_state: An _IncludeState instance.
      error: The function to call with any errors found.
      io: The IO factory to use to read the header file. Provided for unittest
          injection.
    """
    required = {}  # A map of header name to line_number and the template entity.
        # Example of required: { '<functional>': (1219, 'less<>') }

    for line_number in xrange(clean_lines.num_lines()):
        line = clean_lines.elided[line_number]
        if not line or line[0] == '#':
            continue

        # String is special -- it is a non-templatized type in STL.
        if _RE_PATTERN_STRING.search(line):
            required['<string>'] = (line_number, 'string')

        for pattern, template, header in _re_pattern_algorithm_header:
            if pattern.search(line):
                required[header] = (line_number, template)

        # The following function is just a speed up, no semantics are changed.
        if not '<' in line:  # Reduces the cpu time usage by skipping lines.
            continue

        for pattern, template, header in _re_pattern_templates:
            if pattern.search(line):
                required[header] = (line_number, template)

    # The policy is that if you #include something in foo.h you don't need to
    # include it again in foo.cpp. Here, we will look at possible includes.
    # Let's copy the include_state so it is only messed up within this function.
    include_state = include_state.copy()

    # Did we find the header for this file (if any) and succesfully load it?
    header_found = False

    # Use the absolute path so that matching works properly.
    abs_filename = os.path.abspath(filename)

    # For Emacs's flymake.
    # If cpplint is invoked from Emacs's flymake, a temporary file is generated
    # by flymake and that file name might end with '_flymake.cpp'. In that case,
    # restore original file name here so that the corresponding header file can be
    # found.
    # e.g. If the file name is 'foo_flymake.cpp', we should search for 'foo.h'
    # instead of 'foo_flymake.h'
    emacs_flymake_suffix = '_flymake.cpp'
    if abs_filename.endswith(emacs_flymake_suffix):
        abs_filename = abs_filename[:-len(emacs_flymake_suffix)] + '.cpp'

    # include_state is modified during iteration, so we iterate over a copy of
    # the keys.
    for header in include_state.keys():  #NOLINT
        (same_module, common_path) = files_belong_to_same_module(abs_filename, header)
        fullpath = common_path + header
        if same_module and update_include_state(fullpath, include_state, io):
            header_found = True

    # If we can't find the header file for a .cpp, assume it's because we don't
    # know where to look. In that case we'll give up as we're not sure they
    # didn't include it in the .h file.
    # FIXME: Do a better job of finding .h files so we are confident that
    #        not having the .h file means there isn't one.
    if filename.endswith('.cpp') and not header_found:
        return

    # All the lines have been processed, report the errors found.
    for required_header_unstripped in required:
        template = required[required_header_unstripped][1]
        if template in _HEADERS_ACCEPTED_BUT_NOT_PROMOTED:
            headers = _HEADERS_ACCEPTED_BUT_NOT_PROMOTED[template]
            if [True for header in headers if header in include_state]:
                continue
        if required_header_unstripped.strip('<>"') not in include_state:
            error(filename, required[required_header_unstripped][0],
                  'build/include_what_you_use', 4,
                  'Add #include ' + required_header_unstripped + ' for ' + template)


def process_line(filename, file_extension,
                 clean_lines, line, include_state, function_state,
                 class_state, error):
    """Processes a single line in the file.

    Args:
      filename: Filename of the file that is being processed.
      file_extension: The extension (dot not included) of the file.
      clean_lines: An array of strings, each representing a line of the file,
                   with comments stripped.
      line: Number of line being processed.
      include_state: An _IncludeState instance in which the headers are inserted.
      function_state: A _FunctionState instance which counts function lines, etc.
      class_state: A _ClassState instance which maintains information about
                   the current stack of nested class declarations being parsed.
      error: A callable to which errors are reported, which takes 4 arguments:
             filename, line number, error level, and message

    """
    raw_lines = clean_lines.raw_lines
    check_for_function_lengths(filename, clean_lines, line, function_state, error)
    if search(r'\bNOLINT\b', raw_lines[line]):  # ignore nolint lines
        return
    check_for_multiline_comments_and_strings(filename, clean_lines, line, error)
    check_style(filename, clean_lines, line, file_extension, error)
    check_language(filename, clean_lines, line, file_extension, include_state,
                   error)
    check_for_non_standard_constructs(filename, clean_lines, line,
                                      class_state, error)
    check_posix_threading(filename, clean_lines, line, error)
    check_invalid_increment(filename, clean_lines, line, error)


def process_file_data(filename, file_extension, lines, error):
    """Performs lint checks and reports any errors to the given error function.

    Args:
      filename: Filename of the file that is being processed.
      file_extension: The extension (dot not included) of the file.
      lines: An array of strings, each representing a line of the file, with the
             last element being empty if the file is termined with a newline.
      error: A callable to which errors are reported, which takes 4 arguments:
    """
    lines = (['// marker so line numbers and indices both start at 1'] + lines +
             ['// marker so line numbers end in a known way'])

    include_state = _IncludeState()
    function_state = _FunctionState()
    class_state = _ClassState()

    check_for_copyright(filename, lines, error)

    if file_extension == 'h':
        check_for_header_guard(filename, lines, error)

    remove_multi_line_comments(filename, lines, error)
    clean_lines = CleansedLines(lines)
    for line in xrange(clean_lines.num_lines()):
        process_line(filename, file_extension, clean_lines, line,
                     include_state, function_state, class_state, error)
    class_state.check_finished(filename, error)

    check_for_include_what_you_use(filename, clean_lines, include_state, error)

    # We check here rather than inside process_line so that we see raw
    # lines rather than "cleaned" lines.
    check_for_unicode_replacement_characters(filename, lines, error)

    check_for_new_line_at_eof(filename, lines, error)


def process_file(filename, relative_name=None, error=error):
    """Performs cpplint on a single file.

    Args:
      filename: The name of the file to parse.
      error: The function to call with any errors found.
    """

    if not relative_name:
        relative_name = filename

    try:
        # Support the UNIX convention of using "-" for stdin.  Note that
        # we are not opening the file with universal newline support
        # (which codecs doesn't support anyway), so the resulting lines do
        # contain trailing '\r' characters if we are reading a file that
        # has CRLF endings.
        # If after the split a trailing '\r' is present, it is removed
        # below. If it is not expected to be present (i.e. os.linesep !=
        # '\r\n' as in Windows), a warning is issued below if this file
        # is processed.

        if filename == '-':
            lines = codecs.StreamReaderWriter(sys.stdin,
                                              codecs.getreader('utf8'),
                                              codecs.getwriter('utf8'),
                                              'replace').read().split('\n')
        else:
            lines = codecs.open(filename, 'r', 'utf8', 'replace').read().split('\n')

        carriage_return_found = False
        # Remove trailing '\r'.
        for line_number in range(len(lines)):
            if lines[line_number].endswith('\r'):
                lines[line_number] = lines[line_number].rstrip('\r')
                carriage_return_found = True

    except IOError:
        write_error(
            "Skipping input '%s': Can't open for reading\n" % relative_name)
        return

    # Note, if no dot is found, this will give the entire filename as the ext.
    file_extension = filename[filename.rfind('.') + 1:]

    # When reading from stdin, the extension is unknown, so no cpplint tests
    # should rely on the extension.
    if (filename != '-' and file_extension != 'h' and file_extension != 'cpp'
        and file_extension != 'c'):
        write_error('Ignoring %s; not a .cpp, .c or .h file\n' % filename)
    else:
        process_file_data(relative_name, file_extension, lines, error)
        if carriage_return_found and os.linesep != '\r\n':
            # Use 0 for line_number since outputing only one error for potentially
            # several lines.
            error(relative_name, 1, 'whitespace/newline', 1,
                  'One or more unexpected \\r (^M) found;'
                  'better to use only a \\n')

    write_error('Done processing %s\n' % relative_name)


def print_usage(message):
    """Prints a brief usage string and exits, optionally with an error message.

    Args:
      message: The optional error message.
    """
    write_error(_USAGE)
    if message:
        sys.exit('\nFATAL ERROR: ' + message)
    else:
        sys.exit(1)


def print_categories():
    """Prints a list of all the error-categories used by error messages.

    These are the categories used to filter messages via --filter.
    """
    write_error(_ERROR_CATEGORIES)
    sys.exit(0)


def parse_arguments(args, additional_flags=[]):
    """Parses the command line arguments.

    This may set the output format and verbosity level as side-effects.

    Args:
      args: The command line arguments:
      additional_flags: A list of strings which specifies flags we allow.

    Returns:
      A tuple of (filenames, flags)

      filenames: The list of filenames to lint.
      flags: The dict of the flag names and the flag values.
    """
    flags = ['help', 'output=', 'verbose=', 'filter='] + additional_flags
    additional_flag_values = {}
    try:
        (opts, filenames) = getopt.getopt(args, '', flags)
    except getopt.GetoptError:
        print_usage('Invalid arguments.')

    verbosity = _verbose_level()
    output_format = _output_format()
    filters = ''

    for (opt, val) in opts:
        if opt == '--help':
            print_usage(None)
        elif opt == '--output':
            if not val in ('emacs', 'vs7'):
                print_usage('The only allowed output formats are emacs and vs7.')
            output_format = val
        elif opt == '--verbose':
            verbosity = int(val)
        elif opt == '--filter':
            filters = val
            if not filters:
                print_categories()
        else:
            additional_flag_values[opt] = val

    _set_output_format(output_format)
    _set_verbose_level(verbosity)
    _set_filters(filters)

    return (filenames, additional_flag_values)


def set_stream(stream):
    _cpplint_state.set_stream(stream)

def write_error(error):
    _cpplint_state.write_error(error)

def use_mozilla_styles():
    """Disables some features which are not suitable for WebKit."""
    # FIXME: For filters we will never want to have, remove them.
    #        For filters we want to have similar functionalities,
    #        modify the implementation and enable them.
    global _DEFAULT_FILTERS
    _DEFAULT_FILTERS = [
        '-whitespace/comments-doublespace',
        '-whitespace/blank_line',
        '-build/include',  # Webkit specific
        '-build/include_what_you_use',  # <string> for std::string
        '-readability/braces',  # int foo() {};
        '-readability/null',
        '-readability/fn_size',
        '-build/storage_class',  # const static
        '-build/endif_comment',
        '-whitespace/labels',
        '-runtime/arrays',  # variable length array
        '-build/header_guard', # TODO Write a mozilla header_guard variant
        '-runtime/casting',
    ]


def main():
    write_error(
        '''********************* WARNING WARNING WARNING *********************

This tool is in the process of development and may give inaccurate
results at present.  Please file bugs (and/or patches) for things
that you notice that it flags incorrectly.

********************* WARNING WARNING WARNING *********************

''')

    use_webkit_styles()

    (filenames, flags) = parse_arguments(sys.argv[1:])
    if not filenames:
        print_usage('No files were specified.')

    # Change stderr to write with replacement characters so we don't die
    # if we try to print something containing non-ASCII characters.
    sys.stderr = codecs.StreamReaderWriter(sys.stderr,
                                           codecs.getreader('utf8'),
                                           codecs.getwriter('utf8'),
                                           'replace')

    _cpplint_state.reset_error_count()
    for filename in filenames:
        process_file(filename)
    write_error('Total errors found: %d\n' % _cpplint_state.error_count)
    sys.exit(_cpplint_state.error_count > 0)


if __name__ == '__main__':
    main()
