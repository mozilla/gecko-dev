Searchfox Query Language Documentation
======================================

Searchfox provides a powerful query language for searching code
repositories. The system is designed with simplicity in mind - you can
start with basic searches and refine them interactively, while also
supporting advanced query syntax for power users.

.. contents:: Table of Contents

Basic Search
------------

The simplest way to search is to just type what you're looking for. No
special syntax is required:

https://searchfox.org/mozilla-central/search?q=AudioContext

This will search for:

- Text occurrences in files
- Symbol/identifier matches (prefix-based)
- File names containing the term

The path filter input on the right hand side supports glob patterns that
supports the following features:

- ``*`` : matches any characters except path separators (``/``)
- ``**`` : matches any characters including path separators
- ``?`` : matches any single character
- ``{a,b,c}``: matches any of the comma-separated alternatives
- ``^`` and ``$``: regex anchors (preserved as-is)
- Literal parentheses, pipes, and dots are escaped

The same syntax is used whenever globbing is expected in searchfox.

Query Parameters
----------------

Searches can be customized using URL parameters:

Case Sensitivity
~~~~~~~~~~~~~~~~

-  **Parameter**: ``case``
-  **Values**: ``true`` for case-sensitive, anything else for
   case-insensitive (default)

https://searchfox.org/mozilla-central/search?q=AudioContext&case=true

Regular Expressions
~~~~~~~~~~~~~~~~~~~

-  **Parameter**: ``regexp``
-  **Values**: ``true`` to treat query as regex, anything else for
   literal search
-  **Note**: Regex mode only performs textual content search, not semantic search
   unless ``symbol:`` or ``id:`` prefixes are used

https://searchfox.org/mozilla-central/search?q=AudioContext%3A%3A.*Panner*&regexp=true

Path Filtering
~~~~~~~~~~~~~~

-  **Parameter**: ``path``
-  **Description**: Filter results by file paths using glob patterns
-  **Example**: ``?q=MyFunction&path=src/main/*``

For example, finding media playback tests (not Web Audio tests that are in
``dom/media/webaudio/tests``) that use an ``AudioContext``:

https://searchfox.org/mozilla-central/search?q=path%3Adom%2Fmedia%2Ftest+AudioContext&path=&case=false&regexp=false

Advanced Query Syntax
---------------------

The query language supports ``term:value`` syntax for more precise searches.

**Important**: ``term:value`` syntax must be placed before any search terms.
The search endpoint stops parsing once it encounters an unrecognized term.

``path:``
~~~~~~~~~

Filters results by file paths using glob patterns (same as path
parameter):

::

   path:src/components/* MyFunction

``pathre:``
~~~~~~~~~~~

Filters results using regular expressions for paths:

::

    pathre:^src/(main|test)/.*\.js$ MyFunction

Example, finding all tests for the ``PannerNode``, in WPT and Mochitests:

https://searchfox.org/mozilla-central/search?q=pathre%3Atesting%5C%2Fwpt%7Cdom%5C%2Fmedia%5C%2Fwebaudio%5C%2Ftest+PannerNode

Context
~~~~~~~

Allows displaying the result and surrounding context. A current limitation is
that this only works with fulltext search via ``text:`` or ``re:`` and if you
forget to use one, you may get semantic results without any context.

::

    context:3 re:AudioContext::.*Create

Search for all factory methods of an AudioContext, with 3 lines of context,
above and below the search hit:

https://searchfox.org/mozilla-central/search?q=context%3A3+re%3AAudioContext%3A%3A.*Create

Search Type Terms
~~~~~~~~~~~~~~~~~

``symbol:``
^^^^^^^^^^^

Search only for symbols/identifiers

::

   symbol:cubeb_stream_init

-  Multiple symbols can be comma-separated: ``symbol:Foo,Bar``
-  Dot notation is normalized to hash: ``symbol:obj.method`` becomes
   ``symbol:obj#method``
- Note: in C++, this requires the mangled symbol name, and so it is best access by clicking on a member

``id:``
^^^^^^^

Exact-match identifier search (not prefix-based like the default search):

::

   id:main

This means ``id:creategain`` won't match ``createGainNode()`` calls, that are
also present indexed code.

``text:``
^^^^^^^^^

Exact text match, this escapes regexp characters

::

   text:function main()

``re:``
^^^^^^^

Treat remainder of query as regular expression

::

   re:get\w+Value

Diagramming Features
--------------------

**Important**: These diagramming features use the ``/query`` endpoint, not
``/search``. If you type this syntax in the regular search box, it won't work.
It's easiest to access these features through context menus.

**Enabling Diagramming**: To use diagramming features, visit the `settings page
<https://searchfox.org/mozilla-central/pages/settings.html>`_ and change the
"Default feature gate" from "Release" to "Alpha", or use the "Diagramming
feature gate" setting.

**Language Support**: Diagramming currently works for C++ and languages with
SCIP indexing support (Java/Kotlin/Python), but not JavaScript/TypeScript.

**Accessibility Note**: The diagrams currently do not generate a usable
accessibility tree and are considered alpha quality.

Basic Diagramming Queries
~~~~~~~~~~~~~~~~~~~~~~~~~

Inheritance diagram (alpha)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

    inheritance-diagram:'nsIInputStream' depth:4

https://searchfox.org/mozilla-central/query/default?q=inheritance-diagram%3A%27nsIInputStream%27%20depth%3A4

Class diagram (alpha)
^^^^^^^^^^^^^^^^^^^^^

::

    class-diagram:'mozilla::GraphDriver' depth:3

https://searchfox.org/mozilla-central/query/default?q=class-diagram%3A%27mozilla%3A%3AGraphDriver%27+depth%3A3

Function call diagram (alpha)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This works both directions.

::

    calls-from:'mozilla::dom::AudioContext::CreateDynamicsCompressor' depth:2
    calls-to:'mozilla::dom::AudioContext::CreateDynamicsCompressor' depth:4

https://searchfox.org/mozilla-central/query/default?q=calls-from%3A%27mozilla%3A%3Adom%3A%3AAudioContext%3A%3ACreateDynamicsCompressor%27+depth%3A2
https://searchfox.org/mozilla-central/query/default?q=calls-to%3A%27mozilla%3A%3Adom%3A%3ADynamicsCompressorNode%3A%3AThreshold%27+depth%3A4

**Note**: ``calls-from`` now avoids traversing into methods like
``NS_DebugBreak`` that would otherwise clutter diagrams. Similarly, ``calls-to``
and ``calls-between`` avoid problematic interfaces like ``nsIObserver::Observe``
and ``nsISupports`` methods.

Class layout (alpha)
^^^^^^^^^^^^^^^^^^^^

Displays the layout of a class or struct, including inherited members, and holes.

::

    field-layout:'nsTString'

https://searchfox.org/mozilla-central/query/default?q=field-layout%3A%27nsTString%27

Advanced Diagramming: Calls Between
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``calls-between`` functionality allows you to discover how different classes
or methods interact with each other. This is particularly useful for
understanding complex code relationships.

Basic calls-between
^^^^^^^^^^^^^^^^^^^

Find paths between any methods of two classes:

::

    calls-between:'mozilla::ProcessPriorityManager' calls-between:'nsTimer'

https://searchfox.org/mozilla-central/query/default?q=calls-between-source%3A%27mozilla%3A%3AProcessPriorityManager%27%20calls-between-target%3A%27nsTimer%27

Directional calls-between
^^^^^^^^^^^^^^^^^^^^^^^^^

For more precise control, use ``calls-between-source`` and ``calls-between-target``:

::

    calls-between-source:'nsDocShell' calls-between-target:'nsExternalHelperAppService' depth:10

https://searchfox.org/mozilla-central/query/default?q=calls-between-source%3AnsDocShell%20calls-between-target%3AnsExternalHelperAppService%20depth%3A10

Specific method targeting
^^^^^^^^^^^^^^^^^^^^^^^^^

When you know specific methods, you can target them directly:

::

    calls-between-source:'nsGlobalWindowInner::SetTimeout' calls-between-source:'nsGlobalWindowInner::ClearTimeout' calls-between-target:'nsTimer' depth:9

https://searchfox.org/mozilla-central/query/default?q=calls-between-source%3A%27nsGlobalWindowInner%3A%3ASetTimeout%27+calls-between-source%3A%27nsGlobalWindowInner%3A%3AClearTimeout%27+calls-between-target%3A%27nsTimer%27+depth%3A9+paths-between-node-limit%3A12000

**Note**: You must now provide absolute pretty identifiers. If your class is
``foo::Bar``, you can't just use ``Bar`` - you need the full path to avoid
ambiguity.

Include Graph Visualization
^^^^^^^^^^^^^^^^^^^^^^^^^^^

There's a synthetic "(file symbol)" at the end of file path breadcrumbs.
Diagrams triggered on this symbol visualize the header include file graph. This
is most useful with ``calls-between`` queries.

Diagram Customization Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Hierarchy Control
^^^^^^^^^^^^^^^^^

- ``hier:pretty`` - Default hierarchy based on pretty symbol name
- ``hier:flat`` - Disable hierarchy, use old flat layout
- ``hier:subsystem`` - Group by bugzilla component mapping
- ``hier:file`` - Fine-grained file-level hierarchy
- ``hier:dir`` - Group by directories

Graph Layout
^^^^^^^^^^^^

- ``graph-layout:dot`` - Default orderly layout (recommended)
- ``graph-layout:neato`` - Force-directed layout for less orderly appearance
- ``graph-layout:fdp`` - Force-directed with variable edge lengths

Limits and Depth
^^^^^^^^^^^^^^^^

- ``depth:N`` - Limit graph traversal to N levels of depth (1-based)
- ``node-limit:N`` - Maximum nodes in resulting graph (up to 1k)
- ``path-limit:N`` - Nodes with more than N in-edges will be excluded (default: 96)
- ``paths-between-node-limit:N`` - Maximum nodes for path-finding algorithm (up to 16k)

Advanced Options
^^^^^^^^^^^^^^^^

- ``fmus-through-depth:N`` - Include "field member uses" for pointer relationships (use 0 for depth 0 nodes only)

Sharing and Collaboration
-------------------------

All non-default searches are encoded in URLs, making them easy to
bookmark for later use, sharing with team members, include in
documentation or bug reports, and building into automated tools.

When including a searchfox link in source code, consider using a
permalink to a revision when it makes sense.

**Update Schedule**: It takes up to 12 hours for trees other than
the Firefox tree to receive the latest enhancements and fixes.
