# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import math
import os
import re
import sys

# histogram_tools.py is used by scripts from a mozilla-central build tree
# and also by outside consumers, such as the telemetry server.  We need
# to ensure that importing things works in both contexts.  Therefore,
# unconditionally importing things that are local to the build tree, such
# as buildconfig, is a no-no.
try:
    import buildconfig

    # Need to update sys.path to be able to find usecounters.
    sys.path.append(os.path.join(buildconfig.topsrcdir, 'dom/base/'))
except ImportError:
    # Must be in an out-of-tree usage scenario.  Trust that whoever is
    # running this script knows we need the usecounters module and has
    # ensured it's in our sys.path.
    pass

from collections import OrderedDict

def table_dispatch(kind, table, body):
    """Call body with table[kind] if it exists.  Raise an error otherwise."""
    if kind in table:
        return body(table[kind])
    else:
        raise BaseException, "don't know how to handle a histogram of kind %s" % kind

class DefinitionException(BaseException):
    pass

def check_numeric_limits(dmin, dmax, n_buckets):
    if type(dmin) != int:
        raise DefinitionException, "minimum is not a number"
    if type(dmax) != int:
        raise DefinitionException, "maximum is not a number"
    if type(n_buckets) != int:
        raise DefinitionException, "number of buckets is not a number"

def linear_buckets(dmin, dmax, n_buckets):
    check_numeric_limits(dmin, dmax, n_buckets)
    ret_array = [0] * n_buckets
    dmin = float(dmin)
    dmax = float(dmax)
    for i in range(1, n_buckets):
        linear_range = (dmin * (n_buckets - 1 - i) + dmax * (i - 1)) / (n_buckets - 2)
        ret_array[i] = int(linear_range + 0.5)
    return ret_array

def exponential_buckets(dmin, dmax, n_buckets):
    check_numeric_limits(dmin, dmax, n_buckets)
    log_max = math.log(dmax);
    bucket_index = 2;
    ret_array = [0] * n_buckets
    current = dmin
    ret_array[1] = current
    for bucket_index in range(2, n_buckets):
        log_current = math.log(current)
        log_ratio = (log_max - log_current) / (n_buckets - bucket_index)
        log_next = log_current + log_ratio
        next_value = int(math.floor(math.exp(log_next) + 0.5))
        if next_value > current:
            current = next_value
        else:
            current = current + 1
        ret_array[bucket_index] = current
    return ret_array

always_allowed_keys = ['kind', 'description', 'cpp_guard', 'expires_in_version',
                       'alert_emails', 'keyed', 'releaseChannelCollection']

class Histogram:
    """A class for representing a histogram definition."""

    def __init__(self, name, definition):
        """Initialize a histogram named name with the given definition.
definition is a dict-like object that must contain at least the keys:

 - 'kind': The kind of histogram.  Must be one of 'boolean', 'flag',
   'count', 'enumerated', 'linear', or 'exponential'.
 - 'description': A textual description of the histogram.

The key 'cpp_guard' is optional; if present, it denotes a preprocessor
symbol that should guard C/C++ definitions associated with the histogram."""
        self.check_name(name)
        self.verify_attributes(name, definition)
        self._name = name
        self._description = definition['description']
        self._kind = definition['kind']
        self._cpp_guard = definition.get('cpp_guard')
        self._keyed = definition.get('keyed', False)
        self._extended_statistics_ok = definition.get('extended_statistics_ok', False)
        self._expiration = definition.get('expires_in_version')
        self.compute_bucket_parameters(definition)
        table = { 'boolean': 'BOOLEAN',
                  'flag': 'FLAG',
                  'count': 'COUNT',
                  'enumerated': 'LINEAR',
                  'linear': 'LINEAR',
                  'exponential': 'EXPONENTIAL' }
        table_dispatch(self.kind(), table,
                       lambda k: self._set_nsITelemetry_kind(k))
        datasets = { 'opt-in': 'DATASET_RELEASE_CHANNEL_OPTIN',
                     'opt-out': 'DATASET_RELEASE_CHANNEL_OPTOUT' }
        value = definition.get('releaseChannelCollection', 'opt-in')
        if not value in datasets:
            raise DefinitionException, "unknown release channel collection policy for " + name
        self._dataset = "nsITelemetry::" + datasets[value]

    def name(self):
        """Return the name of the histogram."""
        return self._name

    def description(self):
        """Return the description of the histogram."""
        return self._description

    def kind(self):
        """Return the kind of the histogram.
Will be one of 'boolean', 'flag', 'count', 'enumerated', 'linear', or 'exponential'."""
        return self._kind

    def expiration(self):
        """Return the expiration version of the histogram."""
        return self._expiration

    def nsITelemetry_kind(self):
        """Return the nsITelemetry constant corresponding to the kind of
the histogram."""
        return self._nsITelemetry_kind

    def _set_nsITelemetry_kind(self, kind):
        self._nsITelemetry_kind = "nsITelemetry::HISTOGRAM_%s" % kind

    def low(self):
        """Return the lower bound of the histogram.  May be a string."""
        return self._low

    def high(self):
        """Return the high bound of the histogram.  May be a string."""
        return self._high

    def n_buckets(self):
        """Return the number of buckets in the histogram.  May be a string."""
        return self._n_buckets

    def cpp_guard(self):
        """Return the preprocessor symbol that should guard C/C++ definitions
associated with the histogram.  Returns None if no guarding is necessary."""
        return self._cpp_guard

    def keyed(self):
        """Returns True if this a keyed histogram, false otherwise."""
        return self._keyed

    def dataset(self):
        """Returns the dataset this histogram belongs into."""
        return self._dataset

    def extended_statistics_ok(self):
        """Return True if gathering extended statistics for this histogram
is enabled."""
        return self._extended_statistics_ok

    def ranges(self):
        """Return an array of lower bounds for each bucket in the histogram."""
        table = { 'boolean': linear_buckets,
                  'flag': linear_buckets,
                  'count': linear_buckets,
                  'enumerated': linear_buckets,
                  'linear': linear_buckets,
                  'exponential': exponential_buckets }
        return table_dispatch(self.kind(), table,
                              lambda p: p(self.low(), self.high(), self.n_buckets()))

    def compute_bucket_parameters(self, definition):
        table = {
            'boolean': Histogram.boolean_flag_bucket_parameters,
            'flag': Histogram.boolean_flag_bucket_parameters,
            'count': Histogram.boolean_flag_bucket_parameters,
            'enumerated': Histogram.enumerated_bucket_parameters,
            'linear': Histogram.linear_bucket_parameters,
            'exponential': Histogram.exponential_bucket_parameters
            }
        table_dispatch(self.kind(), table,
                       lambda p: self.set_bucket_parameters(*p(definition)))

    def verify_attributes(self, name, definition):
        global always_allowed_keys
        general_keys = always_allowed_keys + ['low', 'high', 'n_buckets']

        table = {
            'boolean': always_allowed_keys,
            'flag': always_allowed_keys,
            'count': always_allowed_keys,
            'enumerated': always_allowed_keys + ['n_values'],
            'linear': general_keys,
            'exponential': general_keys + ['extended_statistics_ok']
            }
        table_dispatch(definition['kind'], table,
                       lambda allowed_keys: Histogram.check_keys(name, definition, allowed_keys))

        Histogram.check_expiration(name, definition)

    def check_name(self, name):
        if '#' in name:
            raise ValueError, '"#" not permitted for %s' % (name)

    @staticmethod
    def check_expiration(name, definition):
        expiration = definition.get('expires_in_version')

        if not expiration:
            return

        if re.match(r'^[1-9][0-9]*$', expiration):
            expiration = expiration + ".0a1"
        elif re.match(r'^[1-9][0-9]*\.0$', expiration):
            expiration = expiration + "a1"

        definition['expires_in_version'] = expiration

    @staticmethod
    def check_keys(name, definition, allowed_keys):
        for key in definition.iterkeys():
            if key not in allowed_keys:
                raise KeyError, '%s not permitted for %s' % (key, name)

    def set_bucket_parameters(self, low, high, n_buckets):
        def try_to_coerce_to_number(v):
            try:
                return eval(v, {})
            except:
                return v
        self._low = try_to_coerce_to_number(low)
        self._high = try_to_coerce_to_number(high)
        self._n_buckets = try_to_coerce_to_number(n_buckets)

    @staticmethod
    def boolean_flag_bucket_parameters(definition):
        return (1, 2, 3)

    @staticmethod
    def linear_bucket_parameters(definition):
        return (definition.get('low', 1),
                definition['high'],
                definition['n_buckets'])

    @staticmethod
    def enumerated_bucket_parameters(definition):
        n_values = definition['n_values']
        return (1, n_values, "%s+1" % n_values)

    @staticmethod
    def exponential_bucket_parameters(definition):
        return (definition.get('low', 1),
                definition['high'],
                definition['n_buckets'])

# We support generating histograms from multiple different input files, not
# just Histograms.json.  For each file's basename, we have a specific
# routine to parse that file, and return a dictionary mapping histogram
# names to histogram parameters.
def from_Histograms_json(filename):
    with open(filename, 'r') as f:
        try:
            histograms = json.load(f, object_pairs_hook=OrderedDict)
        except ValueError, e:
            raise BaseException, "error parsing histograms in %s: %s" % (filename, e.message)
    return histograms

def from_UseCounters_conf(filename):
    return usecounters.generate_histograms(filename)

FILENAME_PARSERS = {
    'Histograms.json': from_Histograms_json,
}

# Similarly to the dance above with buildconfig, usecounters may not be
# available, so handle that gracefully.
try:
    import usecounters

    FILENAME_PARSERS['UseCounters.conf'] = from_UseCounters_conf
except ImportError:
    pass

def from_files(filenames):
    """Return an iterator that provides a sequence of Histograms for
the histograms defined in filenames.
    """
    all_histograms = OrderedDict()
    for filename in filenames:
        parser = FILENAME_PARSERS[os.path.basename(filename)]
        histograms = parser(filename)

        # OrderedDicts are important, because then the iteration order over
        # the parsed histograms is stable, which makes the insertion into
        # all_histograms stable, which makes ordering in generated files
        # stable, which makes builds more deterministic.
        if not isinstance(histograms, OrderedDict):
            raise BaseException, "histogram parser didn't provide an OrderedDict"

        for (name, definition) in histograms.iteritems():
            if all_histograms.has_key(name):
                raise DefinitionException, "duplicate histogram name %s" % name
            all_histograms[name] = definition

    # We require that all USE_COUNTER_* histograms be defined in a contiguous
    # block.
    use_counter_indices = filter(lambda x: x[1].startswith("USE_COUNTER_"),
                                 enumerate(all_histograms.iterkeys()));
    if use_counter_indices:
        lower_bound = use_counter_indices[0][0]
        upper_bound = use_counter_indices[-1][0]
        n_counters = upper_bound - lower_bound + 1
        if n_counters != len(use_counter_indices):
            raise DefinitionException, "use counter histograms must be defined in a contiguous block"

    for (name, definition) in all_histograms.iteritems():
        yield Histogram(name, definition)
