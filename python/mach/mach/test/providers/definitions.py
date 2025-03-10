# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mach.decorators import Command


@Command("cmd_default_visible", category="testing")
def run_default_visible(self, command_context):
    pass


@Command("cmd_not_hidden", category="testing")
def run_not_hidden(self, command_context, hidden=False):
    pass


@Command("cmd_hidden", category="testing", hidden=True)
def run_hidden(self, command_context):
    pass
