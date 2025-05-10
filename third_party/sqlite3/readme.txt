To vendor a new version of SQLite:

	./mach vendor third_party/sqlite3/moz.yaml

To vendor new versions of SQLite extensions, check for `moz.yaml`
files in `ext/` subfolders.

Vendoring tracks GitHub tags, specific tags can be targeted
using the `--revision` option.

If patches are present, they should be in `patches/*.patch` files
and `--patch-mode check` must be passed to `./mach vendor`.

For example, to vendor sqlite-vec:

	./mach vendor third_party/sqlite3/ext/sqlite-vec/moz.yaml --patch-mode check
