# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import os.path
import re
import sys

BUILD_TYPES = [
    "optimized",
    "isDebugBuild",
    "isCoverageBuild",
    "AddressSanitizer",
    "ThreadSanitizer",
]
EQEQ = "=="
FUZZY_IF_REGEX = r"^fuzzy-if\((.*?),(\d+)-(\d+),(\d+)-(\d+)\)$"
IMPLICIT = {
    "fission": True,
    "is64Bit": True,
    "useDrawSnapshot": False,
    "swgl": False,
}
MARGIN = 0.05  # Increase difference/pixels percentage
NOT_EQ = "!="
OSES = ["Android", "cocoaWidget", "appleSilicon", "gtkWidget", "winWidget"]
PASS = "PASS"
TEST_TYPES = [EQEQ, NOT_EQ]


class ListManifestParser(object):
    """
    Meta Manifest Parser is the main class for the lmp program.
    """

    errfile = sys.stderr
    outfile = sys.stdout
    verbose = False

    def __init__(
        self, implicit_vars=False, verbose=False, error=None, warning=None, info=None
    ):
        self.implicit_vars = implicit_vars
        self.verbose = verbose
        self._error = error
        self._warning = warning
        self._info = info
        self.parser = None
        self.fuzzy_if_rx = None

    def error(self, e):
        if self._error is not None:
            self._error(e)
        else:
            print(f"ERROR: {e}", file=sys.stderr, flush=True)

    def warning(self, e):
        if self._warning is not None:
            self._warning(e)
        else:
            print(f"WARNING: {e}", file=sys.stderr, flush=True)

    def info(self, e):
        if self._info is not None:
            self._info(e)
        else:
            print(f"INFO: {e}", file=sys.stderr, flush=True)

    def vinfo(self, e):
        if self.verbose:
            self.info(e)

    def should_merge(self, condition, fuzzy_if_condition):
        """
        Return True if existing condition and proposed fuzzy_if
        differ by one dimension (or less)
        """

        c_os = None
        os = None
        conditions = condition.split("&&")
        n = len(conditions)
        fuzzy_ifs = fuzzy_if_condition.split("&&")
        m = len(fuzzy_ifs)
        dimensions = {}
        delta = 0  # dimensions of difference
        for i in range(n):
            if conditions[i].find("||") > 0:
                disjunctions = conditions[i][1:-1].split("||")
                if disjunctions[0] in OSES:
                    c_os = disjunctions[0]
                    for j in range(m):
                        if fuzzy_ifs[j] in OSES:
                            os = fuzzy_ifs[j]
                            if c_os != os:
                                return False  # do not merge different OSES
                            fuzzy_ifs[j] = ""
                            break
                    conditions[i] = ""
                elif self.implicit_vars and disjunctions[0] in IMPLICIT:
                    dimensions[disjunctions[0]] = True
                    conditions[i] = ""
                else:
                    delta += 1  # OTHER adds a dimension
            elif conditions[i] in OSES:
                c_os = conditions[i]
                for j in range(m):
                    if fuzzy_ifs[j] in OSES:
                        os = fuzzy_ifs[j]
                        if c_os != os:
                            return False  # do not merge different OSES
                        fuzzy_ifs[j] = ""
                        break  # expect only one os variable
                conditions[i] = ""
            elif conditions[i] in BUILD_TYPES:
                for j in range(m):
                    if fuzzy_ifs[j] in BUILD_TYPES:
                        if conditions[i] != fuzzy_ifs[j]:
                            delta += 1  # BUILD_TYPE different
                        fuzzy_ifs[j] = ""
                        break  # expect at most one build_type
                conditions[i] = ""  # handles also if BUILD_TYPE is omitted
            else:
                negated = False
                if conditions[i][0] == "!":
                    negated = True
                    cond = conditions[i][1:]
                else:
                    cond = conditions[i]
                dimensions[cond] = True
                if negated:
                    opposite = cond
                else:
                    opposite = "!" + cond
                for j in range(m):
                    if conditions[i] == fuzzy_ifs[j]:  # same
                        fuzzy_ifs[j] = ""
                        conditions[i] = ""
                        break
                    elif opposite == fuzzy_ifs[j]:  # opposite explicit
                        delta += 1  # different
                        fuzzy_ifs[j] = ""
                        conditions[i] = ""
                        break
                    elif fuzzy_ifs[j] == "(" + cond + "||!" + cond + ")":
                        fuzzy_ifs[j] = ""
                        conditions[i] = ""
                        break
                if (
                    conditions[i]
                    and self.implicit_vars
                    and not (IMPLICIT[cond] and not negated)
                    and not (not IMPLICIT[cond] and negated)
                ):  # opposite implicit different
                    delta += 1
                    conditions[i] = ""
        for i in range(n):
            if conditions[i]:  # unhandled
                delta += 1  # OTHER adds a dimension
        for j in range(m):
            if fuzzy_ifs[j]:  # unhandled
                if fuzzy_ifs[j] in OSES:
                    return False  # condition doesn't specify OS
                if fuzzy_ifs[j] in BUILD_TYPES:
                    continue  # does not add a dimension b/c condition doesn't specify
                if fuzzy_ifs[j][0] == "!":
                    cond = fuzzy_ifs[j][1:]
                else:
                    cond = fuzzy_ifs[j]
                if not cond in dimensions:
                    delta += 1  # OTHER adds a dimension
        return delta <= 1

    def merge(self, condition, fuzzy_if_condition):
        """
        A. if 2 of the 5 build-types are present -- eliminate ALL build types
        (i.e. the condition will apply to all build types)

        B. If both the implicit and explicit (non) default value are present, add
        an OR like this (swgl || !swgl) -- that way the condition will match
        any value of swgl. For implicit variables see:
        https://searchfox.org/mozilla-central/source/layout/tools/reftest/manifest.sys.mjs#30
        fission: true,
        is64Bit: true,
        useDrawSnapshot: false,
        swgl: false,

        C. for other vars if we have A and !A then remove A from the condition
        """

        os = ""
        build_type = ""
        conditions = condition.split("&&")
        n = len(conditions)
        fuzzy_ifs = fuzzy_if_condition.split("&&")
        m = len(fuzzy_ifs)
        conds = {}
        for i in range(n):
            if conditions[i].find("||") > 0:
                disjunctions = conditions[i][1:-1].split("||")
                cond = disjunctions[0]
                if cond in OSES:
                    for j in range(m):
                        if fuzzy_ifs[j] in OSES:
                            if fuzzy_ifs[j] not in disjunctions:
                                disjunctions.append(fuzzy_ifs[j])
                            fuzzy_ifs[j] = ""
                    disjunctions = sorted(disjunctions)
                    os = "(" + "||".join(disjunctions) + ")"
                    conditions[i] = ""
                elif self.implicit_vars and cond in IMPLICIT:
                    for j in range(m):
                        if not fuzzy_ifs[j]:
                            continue
                        if (
                            fuzzy_ifs[j] == cond
                            or fuzzy_ifs[j] == "!" + cond
                            or fuzzy_ifs[j] == "(" + cond + "||!" + cond + ")"
                        ):
                            fuzzy_ifs[j] = ""
                            break
                    conds[cond] = conditions[i]
                    conditions[i] = ""
            elif conditions[i] in OSES:
                os = conditions[i]
                conditions[i] = ""
                for j in range(m):
                    if fuzzy_ifs[j] in OSES:
                        if os < fuzzy_ifs[j]:  # add in alpha order
                            os = "(" + os + "||" + fuzzy_ifs[j] + ")"
                        elif os > fuzzy_ifs[j]:
                            os = "(" + fuzzy_ifs[j] + "||" + os + ")"
                        fuzzy_ifs[j] = ""
                        break  # expect only one os variable
            elif conditions[i] in BUILD_TYPES:
                build_type = conditions[i]
                for j in range(m):
                    if fuzzy_ifs[j] in BUILD_TYPES:
                        if fuzzy_ifs[j] != build_type:  # different
                            build_type = ""
                        fuzzy_ifs[j] = ""
                        conditions[i] = ""
                        break  # expect at most one build_type
                if conditions[i]:  # fuzzy_if had build_type _removed_
                    build_type = ""
                    conditions[i] = ""
            else:
                negated = False
                if conditions[i][0] == "!":
                    negated = True
                    cond = conditions[i][1:]
                else:
                    cond = conditions[i]
                if negated:
                    opposite = cond
                else:
                    opposite = "!" + cond
                disjunction = ""
                for j in range(m):
                    if not fuzzy_ifs[j]:
                        continue
                    if conditions[i] == fuzzy_ifs[j]:  # same
                        conds[cond] = conditions[i]
                        fuzzy_ifs[j] = ""
                        conditions[i] = ""
                        break
                    if (
                        self.implicit_vars
                        and cond in IMPLICIT
                        and (
                            opposite == fuzzy_ifs[j]
                            or fuzzy_ifs[j] == "(" + cond + "||!" + cond + ")"
                        )
                    ):
                        if negated:
                            disjunction = "(" + opposite + "||" + conditions[i] + ")"
                        else:
                            disjunction = "(" + conditions[i] + "||" + opposite + ")"
                        conds[cond] = disjunction
                        fuzzy_ifs[j] = ""
                        conditions[i] = ""
                        break
                    if opposite == fuzzy_ifs[j]:  # remove
                        fuzzy_ifs[j] = ""
                        conditions[i] = ""
                        break
                if (
                    self.implicit_vars
                    and conditions[i]
                    and not (IMPLICIT[cond] and not negated)
                    and not (not IMPLICIT[cond] and negated)
                ):  # opposite implicit
                    if negated:
                        disjunction = "(" + opposite + "||" + conditions[i] + ")"
                    else:
                        disjunction = "(" + conditions[i] + "||" + opposite + ")"
                    conds[cond] = disjunction
                    conditions[i] = ""
                if not self.implicit_vars and conditions[i]:
                    conditions[i] = ""  # remove, unspecified in fuzzy_if
        for i in range(n):
            if conditions[i]:  # unhandled
                negated = False
                if conditions[i][0] == "!":
                    negated = True
                    cond = conditions[i][1:]
                else:
                    cond = conditions[i]
                if (not (self.implicit_vars and cond in IMPLICIT)) or (  # not implicit
                    (IMPLICIT[cond] and negated)
                    or (not IMPLICIT[cond] and not negated)  # or explicit
                ):
                    conds[cond] = conditions[i]
        for j in range(m):
            if fuzzy_ifs[j]:  # unhandled
                if fuzzy_ifs[j] in OSES:
                    os = fuzzy_ifs[j]
                    continue
                if fuzzy_ifs[j] in BUILD_TYPES and fuzzy_ifs[j] != build_type:
                    build_type = ""
                    continue
                negated = False
                if fuzzy_ifs[j][0] == "!":
                    negated = True
                    cond = fuzzy_ifs[j][1:]
                else:
                    cond = fuzzy_ifs[j]
                if not (self.implicit_vars and cond in IMPLICIT):  # not implicit
                    pass  # not present in condition
                elif (IMPLICIT[cond] and negated) or (
                    not IMPLICIT[cond] and not negated
                ):  # or opposite of implicit
                    disjunction = ""
                    if negated:
                        opposite = cond
                    else:
                        opposite = "!" + cond
                    if negated:
                        disjunction = "(" + opposite + "||" + fuzzy_ifs[j] + ")"
                    else:
                        disjunction = "(" + fuzzy_ifs[j] + "||" + opposite + ")"
                    conds[cond] = disjunction
        if os:
            merged = os
        else:
            merged = ""
        if build_type:
            if merged:
                merged += "&&"
            merged += build_type
        conds_keys = sorted(list(conds.keys()))
        for cond in conds_keys:
            if os != "winWidget" and conds[cond] == "is64Bit":
                continue  # special case: elide is64Bit except on Windows
            if os != "gtkWidget" and cond == "useDrawSnapshot":
                continue  # special case: elide useDrawSnapshot except on Linux
            if merged:
                merged += "&&"
            merged += conds[cond]
        return merged

    def get_os_in_condition(self, condition):
        """Return reftest os variable for condition (or the empty string)"""

        os = ""
        conditions = condition.split("&&")
        n = len(conditions)
        for i in range(n):
            if conditions[i].find("||") > 0:
                disjunctions = conditions[i][1:-1].split("||")
                if disjunctions[0] in OSES:
                    os = disjunctions[0]  # returns ONLY first OS if disjunction
                    break
            if conditions[i] in OSES:
                os = conditions[i]
                break
        return os

    def get_dimensions(self, condition):
        """Return number of dimensions in condition"""

        dimensions = []
        conditions = condition.split("&&")
        n = len(conditions)
        for i in range(n):
            if conditions[i].find("||") > 0:
                disjunctions = conditions[i][1:-1].split("||")
                if disjunctions[0] in OSES:
                    if "os" not in dimensions:
                        dimensions.append("os")
                elif disjunctions[0] not in dimensions:
                    dimensions.append(disjunctions[0])
            if conditions[i] in OSES:
                if "os" not in dimensions:
                    dimensions.append("os")
            elif conditions[i] in BUILD_TYPES:
                if "build_type" not in dimensions:
                    dimensions.append("build_type")
            else:
                if conditions[i][0] == "!":
                    cond = conditions[i][1:]
                else:
                    cond = conditions[i]
                if cond not in dimensions:
                    dimensions.append(cond)
        if self.implicit_vars:
            for cond in IMPLICIT:
                if cond not in dimensions:
                    dimensions.append(cond)
        return len(dimensions)

    def calc_fuzzy_if(
        self, modifiers, j, fuzzy_if_condition, d_min, d_max, p_min, p_max
    ):
        """
        Will analzye modifiers in range(j) and
        - move non fuzzy-if's to the left
        - sort fuzzy-ifs by OS and by dimension
        - merge with an exising fuzzy-if ONLY if differs by one dimension (or less)
        - else add fuzzy-if in dimension order
        Returns additional_comment (if added second or subsequent for this OS)
        """

        def fuzzy_if_keyfn(fuzzy_if):
            os = ""
            dimensions = 0
            m = self.fuzzy_if_rx.findall(fuzzy_if)
            if len(m) == 1:  # NOT fuzzy-if
                condition = m[0][0]
                os = self.get_os_in_condition(condition)
                dimensions = self.get_dimensions(condition)
            try:
                os_i = OSES.index(os)
            except ValueError:
                os_i = -1
            return [os_i, dimensions]

        success = True
        additional_comment = ""
        merged = None  # index in modifiers of the last merged fuzzy_if
        os = self.get_os_in_condition(fuzzy_if_condition)
        fuzzy_if = f"fuzzy-if({fuzzy_if_condition},{d_min}-{d_max},{p_min}-{p_max})"
        first = j  # position of first fuzzy-if
        if self.fuzzy_if_rx is None:
            self.fuzzy_if_rx = re.compile(FUZZY_IF_REGEX)
        i = 0
        while i < j:
            m = self.fuzzy_if_rx.findall(modifiers[i])
            if len(m) != 1:  # NOT fuzzy-if
                if i > first:  # move before fuzzy-if's
                    modifier = modifiers[i]
                    del modifiers[i]
                    modifiers.insert(first, modifier)
                    first += 1
            else:  # fuzzy-if
                if i < first:
                    first = i
                condition = m[0][0]
                dmin = int(m[0][1])
                dmax = int(m[0][2])
                pmin = int(m[0][3])
                pmax = int(m[0][4])
                this_os = self.get_os_in_condition(condition)
                if this_os == os and (
                    condition == fuzzy_if_condition
                    or self.should_merge(condition, fuzzy_if_condition)
                ):
                    self.vinfo(f"CONDITION {i:2d} NOW {modifiers[i]}")
                    self.vinfo(f"PROPOSED         {fuzzy_if_condition}")
                    fuzzy_if_condition = self.merge(condition, fuzzy_if_condition)
                    d_min = min(dmin, d_min)  # dmin, if zero, is kept
                    d_max = max(dmax, d_max)
                    p_min = min(pmin, p_min)  # pmin, if zero, is kept
                    p_max = max(pmax, p_max)
                    fuzzy_if = f"fuzzy-if({fuzzy_if_condition},{d_min}-{d_max},{p_min}-{p_max})"
                    if (d_min == 0 and d_max == 0) or (p_min == 0 and p_max == 0):
                        additional_comment = f"fuzzy-if removed as calculated range is {d_min}-{d_max},{p_min}-{p_max}"
                        self.vinfo(f"ABANDONED MERGE  {fuzzy_if}")
                        del modifiers[i]
                        i -= 1
                        j -= 1
                        continue
                    if merged is not None:  # delete previous
                        self.vinfo(f"  Deleting previous: {merged}")
                        del modifiers[merged]
                        i -= 1
                        j -= 1
                    modifiers[i] = fuzzy_if
                    merged = i
                    self.vinfo(f"UPDATED MERGED   {fuzzy_if}")
            i += 1
        if (
            success
            and merged is None
            and ((d_min == 0 and d_max == 0) or (p_min == 0 and p_max == 0))
        ):
            if not additional_comment:  # this is NOT the result of merging to 0-0
                self.vinfo(f"ABANDONED ADD    {fuzzy_if}")
                additional_comment = f"fuzzy-if not added as calculated range is {d_min}-{d_max},{p_min}-{p_max}"
                success = False
            else:
                merged = i  # avoid adding below
        if success:
            if merged is None:
                self.vinfo(f"UPDATED ADDED    {fuzzy_if}")
                modifiers.insert(j, fuzzy_if)
                j += 1
            fuzzy_ifs = modifiers[first:j]
            if len(fuzzy_ifs) > 0:
                fuzzy_ifs = sorted(fuzzy_ifs, key=fuzzy_if_keyfn)
                a = j  # first fuzzy_if for os
                b = j  # last  fuzzy_if for os
                for i in range(len(fuzzy_ifs)):
                    modifiers[first + i] = fuzzy_ifs[i]
                    if fuzzy_ifs[i].startswith("fuzzy-if(" + os):
                        if a == j:
                            a = first + i
                        b = first + i
                if b > a:
                    additional_comment = f"NOTE: more than one fuzzy-if for the OS = {os} ==> may require manual review"
        return success, additional_comment

    def reftest_add_fuzzy_if(
        self,
        manifest_str,
        filename,
        fuzzy_if,
        differences,
        pixels,
        lineno,
        zero,
        bug_reference,
    ):
        """
        Edits a reftest manifest string to add disabled condition
        Returns additional_comment (if any)
        """

        result = ("", "")
        additional_comment = ""
        words = filename.split()
        if len(words) < 3:
            self.error(
                f"Expected filename in the form '[optional conditions] == url url_ref': {filename}"
            )
            return result
        test_type = words[-3]
        url = os.path.basename(words[-2])
        url_ref = os.path.basename(words[-1])
        lines = manifest_str.splitlines()
        if lineno == 0 or lineno > len(lines):
            self.error("cannot determine line to edit in manifest")
            return result
        line = lines[lineno - 1]
        comment = ""
        comment_start = line.find(" #")  # MUST NOT match anchors!
        if comment_start > 0:
            comment = line[comment_start + 1 :]
            line = line[0:comment_start].strip()
        words = line.split()
        n = len(words)
        if n < 3:
            self.error(f"line {lineno} does not match: {line}")
            return result
        if os.path.basename(words[n - 1]) != url_ref:
            self.error(f"words[n-1] not url_ref: {words[n-1]} != {url_ref}")
            return result
        if os.path.basename(words[n - 2]) != url:
            self.error(f"words[n-2] not url: {words[n-2]} != {url}")
            return result
        if words[n - 3] != test_type:
            self.error(f"words[n-3] not '{test_type}': {words[n-3]}")
            return result
        d_min = 0
        d_max = 0
        if len(differences) > 0:
            d_min = min(differences)
            d_max = max(differences)
        if d_min == 0 and d_max > 0:  # recalc minimum
            i = 0
            n = len(differences)
            while i < n:
                if differences[i] == 0:
                    del differences[i]
                    n -= 1
                else:
                    i += 1
            if n > 0:
                d_min = min(differences)
        p_min = 0
        p_max = 0
        if len(pixels) > 0:
            p_min = min(pixels)
            p_max = max(pixels)
        if p_min == 0 and p_max > 0:  # recalc minimum
            i = 0
            n = len(pixels)
            while i < n:
                if pixels[i] == 0:
                    del pixels[i]
                    n -= 1
                else:
                    i += 1
            if n > 0:
                p_min = min(pixels)
        if zero:
            d_min = 0
            p_min = 0
        d_max2 = int((1.0 + MARGIN) * d_max)
        if d_max2 > d_max:
            self.info(
                f"Increased max difference from {d_max} by {int(MARGIN*100)}% to {d_max2}"
            )
            d_max = d_max2
        p_max2 = int((1.0 + MARGIN) * p_max)
        if p_max2 > p_max:
            self.info(
                f"Increased differing pixels from {p_max} by {int(MARGIN*100)}% to {p_max2}"
            )
            p_max = p_max2
        if comment:
            bug = bug_reference.split()
            if comment.find(bug[1]) < 0:  # look for bug number only
                comment += ", " + bug_reference
        else:
            comment = "# " + bug_reference
        j = 0
        for i in range(n):
            if words[i].startswith("HTTP") or words[i] == test_type:
                j = i
                break
        success, additional_comment = self.calc_fuzzy_if(
            words, j, fuzzy_if, d_min, d_max, p_min, p_max
        )
        if success:
            words.append(comment)
            lines[lineno - 1] = " ".join(words)
            manifest_str = "\n".join(lines)
            if manifest_str[-1] != "\n":
                manifest_str += "\n"
        else:
            manifest_str = ""
        result = (manifest_str, additional_comment)
        return result


if __name__ == "__main__":
    sys.exit(ListManifestParser().run())
