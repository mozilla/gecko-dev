# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import shutil
import sys
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory


def main(output, *paths):
    # There's no way to just get the output as a string nor to write to our
    # `output`, so we have to make a temporary directory for glean_parser to
    # write to (which is ironic as glean_parser makes a temporary directory
    # itself).
    with TemporaryDirectory() as outdir:
        outdir_path = Path(outdir)
        # Capture translate output to only display on error
        translate_output = StringIO()
        with redirect_stdout(translate_output), redirect_stderr(translate_output):
            # This is a bit tricky: sys.stderr is bound as a default argument
            # in some functions of glean_parser, so we must redirect stderr
            # _before_ importing the module.
            from glean_parser.translate import translate

            result = translate([Path(p) for p in paths], "rust", outdir_path)
        if result != 0:
            print(translate_output.getvalue())
            sys.exit(result)
        glean_metrics_file = outdir_path / "glean_metrics.rs"
        with glean_metrics_file.open() as glean_metrics:
            shutil.copyfileobj(glean_metrics, output)
