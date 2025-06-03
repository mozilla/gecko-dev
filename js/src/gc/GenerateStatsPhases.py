# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# flake8: noqa: F821

# Generate graph structures for GC statistics recording.
#
# Stats phases are nested and form a directed acyclic graph starting
# from a set of root phases. Importantly, a phase may appear under more
# than one parent phase.
#
# For example, the following arrangement is possible:
#
#            +---+
#            | A |
#            +---+
#              |
#      +-------+-------+
#      |       |       |
#      v       v       v
#    +---+   +---+   +---+
#    | B |   | C |   | D |
#    +---+   +---+   +---+
#              |       |
#              +---+---+
#                  |
#                  v
#                +---+
#                | E |
#                +---+
#
# This graph is expanded into a tree (or really a forest) and phases
# with multiple parents are duplicated.
#
# For example, the input example above would be expanded to:
#
#            +---+
#            | A |
#            +---+
#              |
#      +-------+-------+
#      |       |       |
#      v       v       v
#    +---+   +---+   +---+
#    | B |   | C |   | D |
#    +---+   +---+   +---+
#              |       |
#              v       v
#            +---+   +---+
#            | E |   | E'|
#            +---+   +---+

# NOTE: If you add new phases here the current next phase kind number can be
# found at the end of js/src/gc/StatsPhasesGenerated.inc

import collections
import re


class PhaseKind:
    def __init__(self, name, descr, children=[]):
        self.name = name
        self.descr = descr
        # For telemetry
        self.children = children


AllPhaseKinds = []
PhaseKindsByName = dict()


def addPhaseKind(name, descr, children=[]):
    assert name not in PhaseKindsByName
    phaseKind = PhaseKind(name, descr, children)
    AllPhaseKinds.append(phaseKind)
    PhaseKindsByName[name] = phaseKind
    return phaseKind


def getPhaseKind(name):
    return PhaseKindsByName[name]


PhaseKindGraphRoots = [
    addPhaseKind("MUTATOR", "Mutator Running"),
    addPhaseKind("GC_BEGIN", "Begin Callback"),
    addPhaseKind(
        "EVICT_NURSERY_FOR_MAJOR_GC",
        "Evict Nursery For Major GC",
        [
            addPhaseKind(
                "MARK_ROOTS",
                "Mark Roots",
                [
                    addPhaseKind("MARK_CCWS", "Mark Cross Compartment Wrappers"),
                    addPhaseKind("MARK_STACK", "Mark C and JS stacks"),
                    addPhaseKind("MARK_RUNTIME_DATA", "Mark Runtime-wide Data"),
                    addPhaseKind("MARK_EMBEDDING", "Mark Embedding"),
                ],
            )
        ],
    ),
    addPhaseKind("WAIT_BACKGROUND_THREAD", "Wait Background Thread"),
    addPhaseKind(
        "PREPARE",
        "Prepare For Collection",
        [
            addPhaseKind("UNMARK", "Unmark"),
            addPhaseKind("UNMARK_WEAKMAPS", "Unmark WeakMaps"),
            addPhaseKind("MARK_DISCARD_CODE", "Mark Discard Code"),
            addPhaseKind("RELAZIFY_FUNCTIONS", "Relazify Functions"),
            addPhaseKind("PURGE", "Purge"),
            addPhaseKind("PURGE_PROP_MAP_TABLES", "Purge PropMapTables"),
            addPhaseKind("PURGE_SOURCE_URLS", "Purge Source URLs"),
            addPhaseKind("JOIN_PARALLEL_TASKS", "Join Parallel Tasks"),
        ],
    ),
    addPhaseKind(
        "MARK",
        "Mark",
        [
            getPhaseKind("MARK_ROOTS"),
            addPhaseKind("MARK_DELAYED", "Mark Delayed"),
            addPhaseKind(
                "MARK_WEAK",
                "Mark Weak",
                [
                    getPhaseKind("MARK_DELAYED"),
                    addPhaseKind("MARK_GRAY_WEAK", "Mark Gray and Weak"),
                ],
            ),
            addPhaseKind("MARK_INCOMING_GRAY", "Mark Incoming Gray Pointers"),
            addPhaseKind("MARK_GRAY", "Mark Gray"),
            addPhaseKind(
                "PARALLEL_MARK",
                "Parallel marking",
                [
                    getPhaseKind("JOIN_PARALLEL_TASKS"),
                    # The following are only used for parallel phase times:
                    addPhaseKind("PARALLEL_MARK_MARK", "Parallel marking work"),
                    addPhaseKind("PARALLEL_MARK_WAIT", "Waiting for work"),
                    addPhaseKind("PARALLEL_MARK_OTHER", "Parallel marking overhead"),
                ],
            ),
        ],
    ),
    addPhaseKind(
        "SWEEP",
        "Sweep",
        [
            getPhaseKind("MARK"),
            addPhaseKind(
                "FINALIZE_START",
                "Finalize Start Callbacks",
                [
                    addPhaseKind("WEAK_ZONES_CALLBACK", "Per-Slice Weak Callback"),
                    addPhaseKind(
                        "WEAK_COMPARTMENT_CALLBACK", "Per-Compartment Weak Callback"
                    ),
                ],
            ),
            addPhaseKind("UPDATE_ATOMS_BITMAP", "Sweep Atoms Bitmap"),
            addPhaseKind("SWEEP_ATOMS_TABLE", "Sweep Atoms Table"),
            addPhaseKind(
                "SWEEP_COMPARTMENTS",
                "Sweep Compartments",
                [
                    addPhaseKind("SWEEP_JIT_SCRIPTS", "Sweep JitScripts"),
                    addPhaseKind("SWEEP_INNER_VIEWS", "Sweep Inner Views"),
                    addPhaseKind(
                        "SWEEP_CC_WRAPPER", "Sweep Cross Compartment Wrappers"
                    ),
                    addPhaseKind("SWEEP_BASE_SHAPE", "Sweep Base Shapes"),
                    addPhaseKind("SWEEP_INITIAL_SHAPE", "Sweep Initial Shapes"),
                    addPhaseKind("SWEEP_REGEXP", "Sweep Regexps"),
                    addPhaseKind("SWEEP_COMPRESSION", "Sweep Compression Tasks"),
                    addPhaseKind("SWEEP_WEAKMAPS", "Sweep WeakMaps"),
                    addPhaseKind("SWEEP_UNIQUEIDS", "Sweep Unique IDs"),
                    addPhaseKind("SWEEP_WEAK_POINTERS", "Sweep Weak Pointers"),
                    addPhaseKind(
                        "SWEEP_FINALIZATION_OBSERVERS",
                        "Sweep FinalizationRegistries and WeakRefs",
                    ),
                    addPhaseKind("SWEEP_JIT_DATA", "Sweep JIT Data"),
                    addPhaseKind("SWEEP_WEAK_CACHES", "Sweep Weak Caches"),
                    addPhaseKind("SWEEP_MISC", "Sweep Miscellaneous"),
                    getPhaseKind("JOIN_PARALLEL_TASKS"),
                ],
            ),
            addPhaseKind("SWEEP_PROP_MAP", "Sweep PropMap Tree"),
            addPhaseKind("FINALIZE_END", "Finalize End Callback"),
            addPhaseKind("DESTROY", "Deallocate"),
            getPhaseKind("JOIN_PARALLEL_TASKS"),
            addPhaseKind("FIND_DEAD_COMPARTMENTS", "Find Dead Compartments"),
        ],
    ),
    addPhaseKind(
        "COMPACT",
        "Compact",
        [
            addPhaseKind("COMPACT_MOVE", "Compact Move"),
            addPhaseKind(
                "COMPACT_UPDATE",
                "Compact Update",
                [
                    getPhaseKind("MARK_ROOTS"),
                    addPhaseKind("COMPACT_UPDATE_CELLS", "Compact Update Cells"),
                    getPhaseKind("JOIN_PARALLEL_TASKS"),
                ],
            ),
        ],
    ),
    addPhaseKind("DECOMMIT", "Decommit"),
    addPhaseKind("GC_END", "End Callback"),
    addPhaseKind(
        "MINOR_GC",
        "All Minor GCs",
        [
            getPhaseKind("MARK_ROOTS"),
        ],
    ),
    addPhaseKind(
        "EVICT_NURSERY",
        "Minor GCs to Evict Nursery",
        [
            getPhaseKind("MARK_ROOTS"),
        ],
    ),
    addPhaseKind(
        "TRACE_HEAP",
        "Trace Heap",
        [
            getPhaseKind("MARK_ROOTS"),
        ],
    ),
]


class Phase:
    # Expand the DAG into a tree, duplicating phases which have more than
    # one parent.
    def __init__(self, phaseKind, parent):
        self.phaseKind = phaseKind
        self.parent = parent
        self.depth = parent.depth + 1 if parent else 0
        self.children = []
        self.nextSibling = None
        self.nextInPhaseKind = None

        self.path = re.sub(r"\W+", "_", phaseKind.name.lower())
        if parent is not None:
            self.path = parent.path + "." + self.path


def expandPhases():
    phases = []
    phasesForKind = collections.defaultdict(list)

    def traverse(phaseKind, parent):
        ep = Phase(phaseKind, parent)
        phases.append(ep)

        # Update list of expanded phases for this phase kind.
        if phasesForKind[phaseKind]:
            phasesForKind[phaseKind][-1].nextInPhaseKind = ep
        phasesForKind[phaseKind].append(ep)

        # Recurse over children.
        for child in phaseKind.children:
            child_ep = traverse(child, ep)
            if ep.children:
                ep.children[-1].nextSibling = child_ep
            ep.children.append(child_ep)
        return ep

    for phaseKind in PhaseKindGraphRoots:
        traverse(phaseKind, None)

    return phases, phasesForKind


AllPhases, PhasesForPhaseKind = expandPhases()

# Name phases based on phase kind name and index if there are multiple phases
# corresponding to a single phase kind.

for phaseKind in AllPhaseKinds:
    phases = PhasesForPhaseKind[phaseKind]
    if len(phases) == 1:
        phases[0].name = "%s" % phaseKind.name
    else:
        for index, phase in enumerate(phases):
            phase.name = "%s_%d" % (phaseKind.name, index + 1)

# Find the maximum phase nesting.
MaxPhaseNesting = max(phase.depth for phase in AllPhases) + 1

# Generate code.


def writeList(out, items):
    if items:
        out.write(",\n".join("  " + item for item in items) + "\n")


def writeEnumClass(out, name, type, items, extraItems):
    items = ["FIRST"] + list(items) + ["LIMIT"] + list(extraItems)
    items[1] += " = " + items[0]
    out.write("enum class %s : %s {\n" % (name, type))
    writeList(out, items)
    out.write("};\n")


def generateHeader(out):
    #
    # Generate PhaseKind enum.
    #
    phaseKindNames = map(lambda phaseKind: phaseKind.name, AllPhaseKinds)
    extraPhaseKinds = [
        "NONE = LIMIT",
        "EXPLICIT_SUSPENSION = LIMIT",
        "IMPLICIT_SUSPENSION",
    ]
    writeEnumClass(out, "PhaseKind", "uint8_t", phaseKindNames, extraPhaseKinds)
    out.write("\n")

    #
    # Generate Phase enum.
    #
    phaseNames = map(lambda phase: phase.name, AllPhases)
    extraPhases = ["NONE = LIMIT", "EXPLICIT_SUSPENSION = LIMIT", "IMPLICIT_SUSPENSION"]
    writeEnumClass(out, "Phase", "uint8_t", phaseNames, extraPhases)
    out.write("\n")

    #
    # Generate MAX_PHASE_NESTING constant.
    #
    out.write("static const size_t MAX_PHASE_NESTING = %d;\n" % MaxPhaseNesting)


def generateCpp(out):
    #
    # Generate the PhaseKindInfo table.
    #
    out.write("static constexpr PhaseKindTable phaseKinds = {\n")
    for phaseKind in AllPhaseKinds:
        phase = PhasesForPhaseKind[phaseKind][0]
        out.write(
            '    /* PhaseKind::%s */ PhaseKindInfo { Phase::%s, "%s" },\n'
            % (phaseKind.name, phase.name, phaseKind.name)
        )
    out.write("};\n")
    out.write("\n")

    #
    # Generate the PhaseInfo tree.
    #
    def name(phase):
        return "Phase::" + phase.name if phase else "Phase::NONE"

    out.write("static constexpr PhaseTable phases = {\n")
    for phase in AllPhases:
        firstChild = phase.children[0] if phase.children else None
        phaseKind = phase.phaseKind
        out.write(
            '    /* %s */ PhaseInfo { %s, %s, %s, %s, PhaseKind::%s, %d, "%s", "%s" },\n'
            % (  # NOQA: E501
                name(phase),
                name(phase.parent),
                name(firstChild),
                name(phase.nextSibling),
                name(phase.nextInPhaseKind),
                phaseKind.name,
                phase.depth,
                phaseKind.descr,
                phase.path,
            )
        )
    out.write("};\n")
