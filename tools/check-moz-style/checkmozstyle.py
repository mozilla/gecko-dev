#!/usr/bin/python
#
# Copyright (C) 2009 Google Inc. All rights reserved.
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

"""Script to run the linter for source code of WebKit."""

import os
import os.path
import re
import sys

import modules.cpplint as cpplint
from modules.diff_parser import DiffParser
from modules.scm import detect_scm_system


# Override the usage of the lint tool.
cpplint._USAGE = """
Syntax: %(program_name)s [--verbose=#] [--git-commit=<COMMITISH>] [--output=vs7] [--filter=-x,+y,...]

  The style guidelines this tries to follow are those in
    http://webkit.org/coding/coding-style.html

  Every problem is given a confidence score from 1-5, with 5 meaning we are
  certain of the problem, and 1 meaning it could be a legitimate construct.
  This will miss some errors, and is not a substitute for a code review.

  To prevent specific lines from being linted, add a '// NOLINT' comment to the
  end of the line.

  Linted extensions are .cpp, .c and .h.  Other file types will be ignored.

  Flags:

    verbose=#
      Specify a number 0-5 to restrict errors to certain verbosity levels.

    git-commit=<COMMITISH>
      Check style for a specified git commit.
      Note that the program checks style based on current local file
      instead of actual diff of the git commit.  So, if the files are
      updated after the specified git commit, the information of line
      number may be wrong.

    output=vs7
      By default, the output is formatted to ease emacs parsing.  Visual Studio
      compatible output (vs7) may also be used.  Other formats are unsupported.

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

      To see a list of all the categories used in %(program_name)s, pass no arg:
         --filter=
""" % {'program_name': sys.argv[0]}

def process_patch(patch_string, root, cwd, scm):
    """Does lint on a single patch.

    Args:
      patch_string: A string of a patch.
    """
    patch = DiffParser(patch_string.splitlines())

    if not len(patch.files):
        cpplint.error("patch", 0, "patch/notempty", 3,
                      "Patch does not appear to diff against any file.")
        return

    if not patch.status_line:
        cpplint.error("patch", 0, "patch/nosummary", 3,
                      "Patch does not have a summary.")
    else:
        proper_format = re.match(r"^Bug [0-9]+ - ", patch.status_line)
        if not proper_format:
            proper_format = re.match(r"^No bug - ", patch.status_line)
            cpplint.error("patch", 0, "patch/bugnumber", 3,
                          "Patch summary should begin with 'Bug XXXXX - ' " +
                          "or 'No bug -'.")

    if not patch.patch_description:
        cpplint.error("patch", 0, "patch/nodescription", 3,
                      "Patch does not have a description.")

    for filename, diff in patch.files.iteritems():
        file_extension = os.path.splitext(filename)[1]

        if file_extension in ['.cpp', '.c', '.h']:
            line_numbers = set()
            orig_filename = filename

            def error_for_patch(filename, line_number, category, confidence,
                                message):
                """Wrapper function of cpplint.error for patches.

                This function outputs errors only if the line number
                corresponds to lines which are modified or added.
                """
                if not line_numbers:
                    for line in diff.lines:
                        # When deleted line is not set, it means that
                        # the line is newly added.
                        if not line[0]:
                            line_numbers.add(line[1])

                if line_number in line_numbers:
                    cpplint.error(orig_filename, line_number,
                                  category, confidence, message)

            cpplint.process_file(os.path.join(root, filename),
                                 relative_name=orig_filename,
                                 error=error_for_patch)


def main():
    cpplint.use_mozilla_styles()

    (args, flags) = cpplint.parse_arguments(sys.argv[1:], ["git-commit="])
    if args:
        sys.stderr.write("ERROR: We don't support files as arguments for " +
                         "now.\n" + cpplint._USAGE)
        sys.exit(1)

    cwd = os.path.abspath('.')
    scm = detect_scm_system(cwd)
    root = scm.find_checkout_root(cwd)

    if "--git-commit" in flags:
        process_patch(scm.create_patch_from_local_commit(flags["--git-commit"]), root, cwd, scm)
    else:
        process_patch(scm.create_patch(), root, cwd, scm)

    sys.stderr.write('Total errors found: %d\n' % cpplint.error_count())
    sys.exit(cpplint.error_count() > 0)


if __name__ == "__main__":
    main()
