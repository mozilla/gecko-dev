from itertools import chain
import os

from mozharness.base.log import INFO

# BalrogMixin {{{1
class BalrogMixin(object):
    def _query_balrog_username(self, product=None):
        c = self.config
        if "balrog_username" in c:
            return c["balrog_username"]

        if "balrog_usernames" in c and product in c["balrog_usernames"]:
            return c["balrog_usernames"][product]

        raise KeyError("Couldn't find balrog username.")

    def submit_balrog_updates(self, release_type="nightly"):
        c = self.config
        dirs = self.query_abs_dirs()
        product = self.buildbot_config["properties"]["product"]
        props_path = os.path.join(dirs["base_work_dir"], "balrog_props.json")
        credentials_file = os.path.join(
            dirs["base_work_dir"], c["balrog_credentials_file"]
        )
        submitter_script = os.path.join(
            dirs["abs_tools_dir"], "scripts", "updates", "balrog-submitter.py"
        )
        self.set_buildbot_property(
            "hashType", c.get("hash_type", "sha512"), write_to_file=True
        )

        balrog_props = dict(properties=dict(chain(
            self.buildbot_config["properties"].items(),
            self.buildbot_properties.items(),
        )))
        # XXX: hack alert, turn fake graphene platforms into real ones. This
        # was done more generically originally (bug 1140437), but it broke
        # flame-kk updates (bug 1141633)
        balrog_props["properties"]["platform"] = balrog_props["properties"]["platform"].replace("_graphene", "")
        self.dump_config(props_path, balrog_props)
        cmd = [
            self.query_exe("python"),
            submitter_script,
            "--build-properties", props_path,
            "--api-root", c["balrog_api_root"],
            "--username", self._query_balrog_username(product),
            "-t", release_type,
            "--credentials-file", credentials_file,
        ]
        if self._log_level_at_least(INFO):
            cmd.append("--verbose")

        self.info("Calling Balrog submission script")
        return_code = self.retry(
            self.run_command, attempts=5, args=(cmd,),
        )
        return return_code

    def submit_balrog_release_pusher(self, dirs):
        product = self.buildbot_config["properties"]["product"]
        cmd = [self.query_exe("python"), os.path.join(os.path.join(dirs['abs_tools_dir'], "scripts/updates/balrog-release-pusher.py"))]
        cmd.extend(["--build-properties", os.path.join(dirs["base_work_dir"], "balrog_props.json")])
        cmd.extend(["--api-root", self.config.get("balrog_api_root")])
        cmd.extend(["--buildbot-configs", "https://hg.mozilla.org/build/buildbot-configs"])
        cmd.extend(["--release-config", os.path.join(dirs['build_dir'], self.config.get("release_config_file"))])
        cmd.extend(["--credentials-file", os.path.join(dirs['base_work_dir'], self.config.get("balrog_credentials_file"))])
        cmd.extend(["--username", self.config.get("balrog_usernames")[product]])

        self.info("Calling Balrog release pusher script")
        return_code = self.retry(self.run_command,
                                 args=(cmd,),
                                 kwargs={'cwd': dirs['abs_work_dir']})

        return return_code

    def lock_balrog_rules(self, rule_ids):
        c = self.config
        dirs = self.query_abs_dirs()
        submitter_script = os.path.join(
            dirs["abs_tools_dir"], "scripts", "updates", "balrog-nightly-locker.py"
        )
        credentials_file = os.path.join(
            dirs["base_work_dir"], c["balrog_credentials_file"]
        )

        cmd = [
            self.query_exe("python"),
            submitter_script,
            "--api-root", c["balrog_api_root"],
            "--username", self._query_balrog_username(),
            "--credentials-file", credentials_file,
        ]
        for r in rule_ids:
            cmd.extend(["-r", str(r)])

        if self._log_level_at_least(INFO):
            cmd.append("--verbose")

        cmd.append("lock")

        self.info("Calling Balrog rule locking script.")
        return_code = self.retry(
            self.run_command, attempts=5, args=(cmd,),
        )
        if return_code not in [0]:
            self.return_code = 1
