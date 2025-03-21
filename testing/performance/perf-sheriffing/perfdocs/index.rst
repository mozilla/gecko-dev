======================
Performance Sheriffing
======================

.. contents::
    :depth: 3

1 Overview
----------

Performance sheriffs are responsible for making sure that performance changes in Firefox are detected
and dealt with. They look at data and performance metrics produced by the performance testing frameworks
and find regressions, determine the root cause, and file bugs to track all issues. The workflow we
follow is shown below in our flowchart.

1.1 Flowchart
~~~~~~~~~~~~~

.. image:: ./flowchart.png
   :alt: Sheriffing Workflow Flowchart
   :align: center

The workflow of a sheriff is backfilling jobs to get the data, investigating that data, filing
bugs/linking improvements based on the data, and following up with developers if needed.

1.2 Contacts and the Team
~~~~~~~~~~~~~~~~~~~~~~~~~
In the event that you have an urgent issue and need help what can you do?

If you have a question about a bug that was filed and assigned to you reach out to the sheriff who filed the bug on
Matrix. If a performance sheriff is not responsive or you have a question about a bug
send a message to the `Performance Sheriffs Matrix channel <https://chat.mozilla.org/#/room/#perfsheriffs:mozilla.org>`_
and tag the sheriff. If you still have no-one responding you can message any of the following people directly
on Slack or Matrix:

- `@afinder <https://people.mozilla.org/p/afinder>`_
- `@andra <https://people.mozilla.org/p/andraesanu>`_
- `@beatrice <https://people.mozilla.org/p/bacasandrei>`_
- `@florin.bilt <https://people.mozilla.org/p/fbilt>`_
- `@sparky <https://people.mozilla.org/p/sparky>`_ (reach out to only if all others unreachable)

All of the team is in EET (Eastern European Time) except for @sparky who is in EST (Eastern Standard Time).

1.3 Regression and Improvement Definition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Whenever we get a performance change we classify it as one of two things, either a regression (worse performance) or
an improvement (better performance).

2 How to Investigate Alerts
---------------------------
In this section we will go over how performance sheriffs investigate alerts.

2.1 Filtering and Reading Alerts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
On the `Perfherder <https://treeherder.mozilla.org/perfherder/alerts>`_ page you should see something like below:

.. image:: ./Alerts_view.png
  :alt: Alerts View Toolbar
  :align: center

After accessing the Perfherder alerts page make sure the filter (located in the top middle of the screenshot)
is set to show the correct alerts for sheriffing. The new alerts can be found when
the **untriaged** option from the left-most dropdown is selected. As shown in the screenshot below:

.. image:: ./Alerts_view_toolbar.png
  :alt: Alerts View Toolbar
  :align: center

The rest of the dropdowns from left to right are as follows:

- **Testing harness**: altering this will take you to alerts generated on different harnesses
- **The filter input**, where you can type some text and press enter to narrow down the alerts view
- **"Hide downstream / reassigned to / invalid"**: enable this (recommended) to reduce clutter on the page
- **"My alerts"**: only shows alerts assigned to you.

Below is a screenshot of an alert:

.. image:: ./single_alert.png
  :alt: Alerts View Toolbar
  :align: center

You can tell an alert by looking at the bold text, it will say "Alert #XXXXX", in each alert you have groupings of
summaries of tests, and those tests:

- Can run on different platforms
- Can share suite name (like tp5o)
- Measure various metrics
- Share the same framework

Going from left to right of the columns inside the alerts starting with test, we have:

- A blue hyperlink that links to the test documentation (if available)
- The **platform's** operating system
- **Information** about the historical data distribution of that
- Tags and options related to the test

2.2 Regressions vs Improvements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
First thing to note about how we investigate alerts is that **we prioritize handling regressions**! Unlike the
**improvements,** regressions ship bugs to users, which, if not addressed, make our products worse and drive users away.
After acknowledging an alert:

- Regressions go through multiple status changes (TODO: link to sections with multiple status changes) until they are finally resolved
- An improvement has a single status of improvement

2.3 Framework Thresholds
~~~~~~~~~~~~~~~~~~~~~~~~
Different frameworks test different things, and the thresholds for triggering alerts and considering
performance changes differ based on the harness:

- AWSY >= 0.25%
- Build metrics installer size >= 100kb
- Talos, Browsertime, Build Metrics >= 2%

3 How to Handle Inactive Alerts
-------------------------------

Inactive performance alerts are those alerts which have had no activity in 1 week. This section covers how performance sheriffs should handle inactive performance alerts that are found in the daily email sent to the `perfalert-activity group <https://groups.google.com/a/mozilla.com/g/perfalert-activity/about>`_.

3.1 Process
~~~~~~~~~~~

The following is the general process that needs to be taken for the alerts in the email:

 #. Open the email titled ``[bugbot][autofix] PerfAlert regressions with 1 week(s) of inactivity for the DATE`` to find bugs that are inactive.

    - These occur at most daily.

 #. Open one of the bugs mentioned in the email.

 #. Check if the developer has previously responded to the bug.

 #. Find the developer (regression author) being needinfo’ed by the BugBot.

 #. (Optional) Check on `people.mozilla.org <https://people.mozilla.org>`_ to find the person’s Matrix/Slack information if needed.

 #. Find the developer in a public channel.

    - ``#developers`` on Matrix is the most likely place you can find them.

 #. Reach out to them with a message like the following:

    - **If the patch has had a response from the regressor author:**

      ::

       Hello, could you provide an update on this performance regression or close it if it makes sense to (with a follow-up bug if needed)? <PERFORMANCE-ALERT-BUG-LINK>

    - **If the patch has never had a response from the regressor author:**

      ::

       Hello, could you provide an update on this performance regression or close it if it makes sense to (with a follow-up bug if needed)? In accordance with our `regression policy <https://www.mozilla.org/en-US/about/governance/policies/regressions/>`_, we're considering backing out your patch due to a lack of comments/activity: <PERFORMANCE-ALERT-BUG-LINK>

3.2 Handling Responses
~~~~~~~~~~~~~~~~~~~~~~

For Bugs with a Response from the Regressor Author
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Depending on the developer's response, one of four things may happen:

 #. **Developer provides an update on the alert bug:**

    - No other action is needed. If this has happened multiple times on the bug, you can add the ``backlog-deferred`` keyword to prevent the BugBot rule from triggering again on the alert.

 #. **Developer asks for clarification on the process or isn’t sure what to do:**

    - Point them to this documentation. Explain the possible resolutions and what we expect of them.

 #. **Developer does not respond:**

    - Wait for 1 full business day for the response. If there is still no response, find and ping their manager (can be in private) from `people.mozilla.org <https://people.mozilla.org>`_.

      - If there is a response from the manager, you can proceed with one of the other options.

 #. **Developer does not want to close the bug and needs time to investigate:**

    - Add the ``backlog-deferred`` keyword to prevent BugBot from triggering on this bug again in the future.

For Bugs with No Previous Response from the Regressor Author
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Depending on the developer's response, one of five things may happen:

 #. **Developer agrees to a backout:**

    - Reach out to a sheriff in ``#sheriffs`` on Matrix to request the backout.

      - Ensure that they understand that if they’re actively working on it, they can provide an update on the alert bug to prevent a backout.
      - Ensure that they understand that they can close the bug with ``WONTFIX``/``INCOMPLETE`` if they aren’t actively working on it, or they think it isn’t a big issue. They can file a follow-up bug to look into the issue further in the future.

 #. **Developer provides an update on the alert bug:**

    - No other action is needed. If this has happened multiple times on the bug, you can add the ``backlog-deferred`` keyword to prevent the BugBot rule from triggering again on the alert.

 #. **Developer asks for clarification on the process or isn’t sure what to do:**

    - Point them to this documentation. Explain the possible resolutions and what we expect of them.

 #. **Developer does not respond:**

    - Wait for 1 full business day for the response. If there is still no response, find and ping their manager (can be in private) from `people.mozilla.org <https://people.mozilla.org>`_.

      - If there is a response from the manager/developer, you can proceed with one of the other options. If not, request a backout.

 #. **Developer does not want to close the bug and needs time to investigate:**

    - Ask them to provide a comment in the bug stating this. Add the ``backlog-deferred`` keyword to prevent the BugBot from triggering on this bug again in the future.
