# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from ..result import ResultContainer

try:
    import blessings
except ImportError:
    blessings = None


class NullTerminal(object):
    """Replacement for `blessings.Terminal()` that does no formatting."""
    class NullCallableString(unicode):
        """A dummy callable Unicode stolen from blessings"""
        def __new__(cls):
            new = unicode.__new__(cls, u'')
            return new

        def __call__(self, *args):
            if len(args) != 1 or isinstance(args[0], int):
                return u''
            return args[0]

    def __getattr__(self, attr):
        return self.NullCallableString()


class StylishFormatter(object):
    """Formatter based on the eslint default."""

    # Colors later on in the list are fallbacks in case the terminal
    # doesn't support colors earlier in the list.
    # See http://www.calmar.ws/vim/256-xterm-24bit-rgb-color-chart.html
    _colors = {
        'grey': [247, 8, 7],
        'red': [1],
        'yellow': [3],
        'brightred': [9, 1],
        'brightyellow': [11, 3],
    }
    fmt = "  {c1}{lineno}{column}  {c2}{level}{normal}  {message}  {c1}{rule}({linter}){normal}"
    fmt_summary = "{t.bold}{c}\u2716 {problem} ({error}, {warning}){t.normal}"

    def __init__(self, disable_colors=None):
        if disable_colors or not blessings:
            self.term = NullTerminal()
        else:
            self.term = blessings.Terminal()
        self.num_colors = self.term.number_of_colors

    def color(self, color):
        for num in self._colors[color]:
            if num < self.num_colors:
                return self.term.color(num)
        return ''

    def _reset_max(self):
        self.max_lineno = 0
        self.max_column = 0
        self.max_level = 0
        self.max_message = 0

    def _update_max(self, err):
        """Calculates the longest length of each token for spacing."""
        self.max_lineno = max(self.max_lineno, len(str(err.lineno)))
        if err.column:
            self.max_column = max(self.max_column, len(str(err.column)))
        self.max_level = max(self.max_level, len(str(err.level)))
        self.max_message = max(self.max_message, len(err.message))

    def _pluralize(self, s, num):
        if num != 1:
            s += 's'
        return str(num) + ' ' + s

    def __call__(self, result):
        message = []

        num_errors = 0
        num_warnings = 0
        for path, errors in sorted(result.iteritems()):
            self._reset_max()

            message.append(self.term.underline(path))
            # Do a first pass to calculate required padding
            for err in errors:
                assert isinstance(err, ResultContainer)
                self._update_max(err)
                if err.level == 'error':
                    num_errors += 1
                else:
                    num_warnings += 1

            for err in errors:
                message.append(self.fmt.format(
                    normal=self.term.normal,
                    c1=self.color('grey'),
                    c2=self.color('red') if err.level == 'error' else self.color('yellow'),
                    lineno=str(err.lineno).rjust(self.max_lineno),
                    column=(":" + str(err.column).ljust(self.max_column)) if err.column else "",
                    level=err.level.ljust(self.max_level),
                    message=err.message.ljust(self.max_message),
                    rule='{} '.format(err.rule) if err.rule else '',
                    linter=err.linter.lower(),
                ))

            message.append('')  # newline

        # Print a summary
        message.append(self.fmt_summary.format(
            t=self.term,
            c=self.color('brightred') if num_errors else self.color('brightyellow'),
            problem=self._pluralize('problem', num_errors + num_warnings),
            error=self._pluralize('error', num_errors),
            warning=self._pluralize('warning', num_warnings),
        ))

        return '\n'.join(message)
