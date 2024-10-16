Hunting Leaks
=============

.. contents:: Table of Contents
    :local:
    :depth: 2

Different tools and techniques are used to hunt leaks:

.. list-table::
   :header-rows: 1

   * - Tools
     - Description
   * - :ref:`Bloatview`
     - BloatView is a tool that shows information about cumulative memory usage and leaks.
   * - :ref:`Refcount Tracing and Balancing`
     - Refcount tracing and balancing are advanced techniques for tracking down leak of refcounted objects found with BloatView.
   * - `GC and CC logs </performance/memory/gc_and_cc_logs.html>`_
     - Garbage collector (GC) and cycle collector (CC) logs give information about why various JS and C++ objects are alive in the heap.
   * - `Using CC logs for leaked until shutdown leaks </performance/memory/leaked_until_shutdown.html>`_
     - An introduction to leaked until shutdown leaks and how to use CC logs to investigate them.
   * - :ref:`DMD Heap Scan Mode`
     - Heap profiler within Firefox
