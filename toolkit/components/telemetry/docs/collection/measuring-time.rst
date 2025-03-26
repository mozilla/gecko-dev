======================
Measuring elapsed time
======================

To make it easier to measure how long operations take, we have helpers for C++.
These helpers record the elapsed time into histograms, so you have to create suitable :doc:`histograms` for them first.

API:

.. code-block:: cpp

    void AccumulateTimeDelta(HistogramID id, TimeStamp start, TimeStamp end = TimeStamp::Now());
    void AccumulateTimeDelta(HistogramID id, const nsCString& key, TimeStamp start, TimeStamp end = TimeStamp::Now());
