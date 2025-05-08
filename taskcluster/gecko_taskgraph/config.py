# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.util.schema import Schema, optionally_keyed_by
from voluptuous import Any, Optional, Required
from voluptuous.validators import Length

graph_config_schema = Schema(
    {
        # The trust-domain for this graph.
        # (See https://firefox-source-docs.mozilla.org/taskcluster/taskcluster/taskgraph.html#taskgraph-trust-domain)  # noqa
        Required("trust-domain"): str,
        # This specifes the prefix for repo parameters that refer to the project being built.
        # This selects between `head_rev` and `comm_head_rev` and related paramters.
        # (See http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#push-information  # noqa
        # and http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#comm-push-information)  # noqa
        Required("project-repo-param-prefix"): str,
        # This specifies the top level directory of the application being built.
        # ie. "browser/" for Firefox, "comm/mail/" for Thunderbird.
        Required("product-dir"): str,
        Required("treeherder"): {
            # Mapping of treeherder group symbols to descriptive names
            Required("group-names"): {str: Length(max=100)}
        },
        Required("index"): {Required("products"): [str]},
        Required("try"): {
            # We have a few platforms for which we want to do some "extra" builds, or at
            # least build-ish things.  Sort of.  Anyway, these other things are implemented
            # as different "platforms".  These do *not* automatically ride along with "-p
            # all"
            Required("ridealong-builds"): {str: [str]},
        },
        Required("release-promotion"): {
            Required("products"): [str],
            Required("flavors"): {
                str: {
                    Required("product"): str,
                    Required("target-tasks-method"): str,
                    Optional("is-rc"): bool,
                    Optional("rebuild-kinds"): [str],
                    Optional("version-bump"): bool,
                    Optional("partial-updates"): bool,
                }
            },
            Optional("rebuild-kinds"): [str],
        },
        Required("merge-automation"): {
            Required("behaviors"): {
                str: {
                    Optional("from-branch"): str,
                    Required("to-branch"): str,
                    Required("version-files"): [
                        {
                            Required("filename"): str,
                            Optional("new-suffix"): str,
                            Optional("version-bump"): Any("major", "minor"),
                        }
                    ],
                    Required("replacements"): [[str]],
                    Required("merge-old-head"): bool,
                    Optional("regex-replacements"): [[str]],
                    Optional("base-tag"): str,
                    Optional("end-tag"): str,
                    Optional("fetch-version-from"): str,
                }
            },
        },
        Required("scriptworker"): {
            # Prefix to add to scopes controlling scriptworkers
            Required("scope-prefix"): str,
        },
        Required("task-priority"): optionally_keyed_by(
            "project",
            Any(
                "highest",
                "very-high",
                "high",
                "medium",
                "low",
                "very-low",
                "lowest",
            ),
        ),
        Required("partner-urls"): {
            Required("release-partner-repack"): optionally_keyed_by(
                "release-product", "release-level", "release-type", Any(str, None)
            ),
            Optional("release-partner-attribution"): optionally_keyed_by(
                "release-product", "release-level", "release-type", Any(str, None)
            ),
            Required("release-eme-free-repack"): optionally_keyed_by(
                "release-product", "release-level", "release-type", Any(str, None)
            ),
        },
        Required("workers"): {
            Required("aliases"): {
                str: {
                    Required("provisioner"): optionally_keyed_by("level", str),
                    Required("implementation"): str,
                    Required("os"): str,
                    Required("worker-type"): optionally_keyed_by(
                        "level", "release-level", "project", str
                    ),
                }
            },
        },
        Required("mac-signing"): {
            Required("mac-requirements"): optionally_keyed_by("platform", str),
            Required("hardened-sign-config"): optionally_keyed_by(
                "hardened-signing-type",
                [
                    {
                        Optional("deep"): bool,
                        Optional("runtime"): bool,
                        Optional("force"): bool,
                        Optional("requirements"): optionally_keyed_by(
                            "release-product", "release-level", str
                        ),
                        Optional("entitlements"): optionally_keyed_by(
                            "build-platform", "project", str
                        ),
                        Required("globs"): [str],
                    }
                ],
            ),
        },
        Required("taskgraph"): {
            Optional(
                "register",
                description="Python function to call to register extensions.",
            ): str,
            Optional("decision-parameters"): str,
            Optional("run"): {
                Optional("use-caches"): Any(bool, [str]),
            },
        },
        Required("expiration-policy"): optionally_keyed_by("project", {str: str}),
    }
)
