# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig
import os
import sys

sys.path.append(os.path.join(buildconfig.topsrcdir, "js", "src", "frontend"))
import ReservedWordReader


def line(opt, s):
    opt["output"].write("{}\n".format(s))


def main(output, reserved_words_h, *args):
    reserved_word_list = ReservedWordReader.read_reserved_word_list(
        reserved_words_h, *args
    )

    opt = {"output": output}

    line(opt, "const JS_RESERVED_WORDS = [")
    for index, word in reserved_word_list:
        line(opt, '  "{}",'.format(word))
    line(opt, "];")
    line(opt, "module.exports = JS_RESERVED_WORDS;")


if __name__ == "__main__":
    main(sys.stdout, *sys.argv[1:])
