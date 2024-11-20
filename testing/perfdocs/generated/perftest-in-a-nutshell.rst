=================================
Performance Testing in a Nutshell
=================================

.. contents::
    :depth: 3

.. note::
  This page is still under construction, and is missing the videos.

In this document, you'll find all the information needed regarding the basics of performance testing our products. A set of videos, and instructions can be found to guide you across the various subjects related to performance testing. If you're new to performance testing, starting from the first section would be the most helpful for you.

Help! I have a regression. What do I do?
----------------------------------------

Most people will first be introduced to performance testing at Mozilla through a bug like this:

 .. image:: ./perf_alert_bug.png
   :alt: Performance Alert Bug
   :scale: 75%
   :align: center


The title of the bug will include the tests, and subtests that caused the largest change, the platform(s) the changes were detected on, and the date. It will also be prefixed with a range of the size (in percentage) of the changes. In the bug details, the bug which triggered the alert will also be found in the ``Regressed by`` field, and there will be a needinfo for the patch author.

**Bug overview and the first comment (aka comment 0):**

An alert summary table will always be found in the first comment (comment 0) of a performance regression bug, it provides all the information required to understand what regressed.

 .. image:: ./perf_alert_comment_zero.png
   :alt: Comment Zero from a Performance Alert Bug
   :scale: 75%
   :align: center


 * In the first sentence of the alert, a link to the actual push/commit that caused it is provided.
 * Below that, a table that summarizes the regressions, and improvements is shown.
 * Each row of the table provides a single regression/improvement that was detected.

    * ``Ratio``: Provides the size of the change, and links to a graph for the metric.
    * ``Test``: The name of the test, and metric that changed. It links to the documentation for the test.
    * ``Platform``: The name of the platform that the change was detected on.
    * ``Options``: Specify the conditions in which the test was run (e.g. cold, warm, bytecode-cached).
    * ``Absolute Values``: The absolute values of the change.
    * ``Performance Profiles``: Before, and after Gecko profiles of the test (this is not available for all test frameworks yet).

 * Below the table, an ``alert summary`` link can be found which provides a different view of this table, a full in-depth explanation of the alert summary link can be found here `Perfherder Alerts View`_. Some additional debugging information such as ``side-by-side`` videos can be found here. It will also include any additional tests that alerted after this bug was filed.
 * Then some information on what we expect from the patch author is provided regarding how long they have to respond to the alert before it gets backed out along with links to the guidelines for handling regressions.
 * Finally, a helpful command to run these tests on try is provided using ``./mach try perf --alert <ALERT-NUM>``. :ref:`See here for more information about mach try perf <Mach Try Perf>`.

**From the alert summary comment, there are multiple things that could be verified:**
 * Check the graphs to ensure that the regression/improvements are very visible.
 * Look at the alert summary to see if there are any available side-by-side videos to visualize.
 * Check the test description to see what the test is doing, as well as what the metrics that changed are measuring.
 * Compare the before/after performance profiles to see what might have changed. See the section on `Using the Firefox Profiler`_ for more information on this.

With all of the information found from those, there are two main things that can be done. The first is investigating the profiles (or using some other tools), and finding an issue that needs to be fixed. During the process of investigating and verifying the fix, it will become necessary to verify the fix by running the test. Proceed to the `Running Performance Tests`_ section for information about this, and then the `Performance Comparisons`_ section for help with doing performance comparisons in CI which is needed for verifying if a fix will resolve the alert.

The second is requesting a confirmation from the performance sheriff that the patch which caused the alert is definitely the correct one. This can happen when the metric is very noisy, and the change is small (in the area of 2-3%, our threshold of detection). The sheriff will conduct more retriggers on the test, and may ask some clarifying questions about the patch.

**There are 3 main resolutions for these alert bugs which depend on what you find in your investigations:**
 #. A ``WONTFIX`` resolution which implies that a change was detected, but it won't be fixed. It's possible to have this resolution on a bug which produces regressions, but the improvements outweigh those regressions. Harness-related changes are often resolved this way as well since we consider them baseline changes.
 #. An ``INVALID`` resolution which implies that the detection was invalid, and there wasn't a change to performance metrics. These are generally rare as performance sheriffs tend to invalidate the alerts before a bug is produced, and tend to be related to infrastructure changes or very noisy tests where a culprit can't be determined accurately.
 #. A ``FIXED`` resolution which implies that a change was detected, and a fix was made to resolve it.

If there are any questions about the alert, or additional help is needed with debugging the alert feel free to needinfo the performance sheriff that reported the bug. The performance sheriff most suitable for adding a needinfo to can be identified on the regression bug via the user who added a ``status-firefox [X]: --- → affected`` comment. In the future, this person `will be identified in comment zero <ttps://bugzilla.mozilla.org/show_bug.cgi?id=1914174>`_.

Perfherder Alerts View
----------------------
When you click on the "Alerts Summary" hyperlink it will take you to an alert summary table on Perfherder which looks like the following screenshot:

 .. image:: ./perfherder_alertsview.png
   :alt: Sample Perfherder Alert Summary
   :scale: 75%
   :align: center

 * The table has 1 performance metric per row that has either improved or regressed a metric.
 * From left to right, the columns and icons you need to be concerned about as a developer are:

    * ``Graph icon``: Takes you to a graph that shows the history of the metric.
    * ``Test``: A hyperlink to all the test settings, test owner, and their contact information. As well as the name of the subtest (in our case SpeedIndex, and loadtime).
    * ``Platform``: Platform of metric which regressed.
    * ``Debug Tools``: Tools available to help visualize and debug regressions.
    * ``Information``: Historical data distribution (modal, ok, or n/a if not enough information is  available).
    * ``Tags & Options``: Specify the conditions in which the test was run (e.g. cold, warm, bytecode-cached).
    * ``Magnitude of Change``: How much the metric improved or regressed (green colour indicates an improvement and red indicates a regression).
    * ``Confidence``: Confidence value of metric (number is not out of 100) higher number means higher confidence.

Running Performance Tests
-------------------------

Performance tests can either be run locally, or in CI using try runs. In general, it's recommended to use try runs to verify the performance changes your patch produces (if any). This is because the hardware that we run tests on may not have the same characteristics as local machines so local testing may not always produce the same performance differences. Using try runs also allows you to use our performance comparison tooling such as `Compare View <https://treeherder.mozilla.org/perfherder/comparechooser>`_ and `PerfCompare <https://perf.compare/>`_. See the `Performance Comparisons`_ section for more information on that.

It's still possible that a local test can reproduce a change found in CI though, but it's not guaranteed. To run a test locally, you can look at the tests listed in either of the harness documentation test lists such as this one for `Raptor tests <raptor.html#raptor-tests>`_. There are four main ways that you'll find to run these tests:

 * ``./mach raptor`` for :ref:`Raptor`
 * ``./mach talos-test`` for :ref:`Talos`
 * ``./mach perftest`` for :ref:`MozPerftest`
 * ``./mach awsy`` for :ref:`AWSY`

It's also possible to run all the alerting tests using ``./mach perftest``. To do this, find the alert summary ID/number, then use it in the following command::

   ./mach perftest <ALERT-NUMBER>

To run the exact same commands as what is run in CI, add the ``--alert-exact`` option. The test(s) to run can also be specified by using the ``--alert-tests`` option.

Performance Comparisons
-----------------------

Comparing performance metrics across multiple try runs is an important step in the performance testing process. It's used to ensure that changes don't regress our metrics, to determine if a performance improvement is produced from a patch, and among other things, used to verify that a fix resolves a performance alert.

We currently use PerfCompare for comparing performance numbers. Landing on PerfCompare, two search comparison workflows are available: Compare with a base or Compare over time. Compare with a base allows up to three new revisions to compare against a base revision. Although talos is set at the default, any other testing framework or harness can also be selected before clicking the Compare button. :ref:`You can find more information about using PerfCompare here <PerfCompare>`.

 .. image:: ./perfcomparehomescreen.png
   :alt: PerfCompare Selection Interface for Revisions/Pushes to Compare
   :scale: 50%
   :align: center

Our old tool for comparing perfomance numbers, `Compare View <https://treeherder.mozilla.org/perfherder/comparechooser>`_, will be replaced by PerfCompare early next year. The first interface that's seen in that process is the following which is used to select two pushes (based on the revisions) to compare.

 .. image:: ./compare_view_selection.png
   :alt: Selection Interface for Revisions/Pushes to Compare
   :scale: 50%
   :align: center

At the same time, the framework to compare will need to be selected. By default, the Talos framework is selected, but this can be changed after the Compare button is pressed.

After the compare button is pressed, a visualization of the comparisons is shown. More information on what the various columns in the comparison mean can be found in `this documentation <standard-workflow.html#compareview>`_.


Using the Firefox Profiler
--------------------------

The Firefox Profiler can be used to help with debugging performance issues in your code. `See here for documentation <https://profiler.firefox.com/docs/#/>`_ on how it can be used to better understand where the regressing code is, and what might be causing the regression. Profiles are provided on most alert summary bugs from before, and after the regression (see first section above).

If those are not provided in the alert summary, they can always be generated for a test by clicking on the graphs link (the percent-change ratio in an alert summary), selecting a dot in the graph from before or after a change, and clicking the job link. Then, once the job panel opens up in Treeherder, select ``Generate performance profile`` to start a new task that produces a performance profile. See the following graphic illustrating this process:

 .. image:: ./perf_alert_profile_from_graph.png
   :alt: Getting a Profile from an Alerting Test
   :scale: 75%
   :align: center

Additionally you can also use the overflow menu and generate a profile:

 .. image:: ./create_profile_triple_dot.png
   :alt: Creating a profile through the overflow menu
   :scale: 50%
   :align: center

Most Raptor/Browsertime tests produce a performance profile by default at the end of their test run, but Talos, MozPerftest, and AWSY tests do not. As previously mentioned, for regression/improvement alerts, you can find a before and after link of these profiles in Comment 0:

 .. image:: ./perf_alert_comment_zero_before-after.png
   :alt: View before/after profiles from alerts
   :scale: 50%
   :align: center

You can also find the profiles in the artifacts tab of the Raptor test:

 .. image:: ./raptor_extra_profiler_run.png
   :alt: Find extra profiler run profiles in treeherder task
   :scale: 50%
   :align: center

To generate the profiles locally, you can pass the flags ``--extra-profiler-run`` or ``--gecko-profile`` which repeat the test for an extra iteration with the profiler enabled, or run the test from the beginning with the profiler enabled for three iterations, respectively.


Adding Performance Tests
------------------------

This section is under construction.


Additional Help
---------------

Reach out to the Performance Testing, and Tooling team in the `#perftest channel on Matrix <https://matrix.to/#/#perftest:mozilla.org>`_, or the #perf-help channel on Slack.
