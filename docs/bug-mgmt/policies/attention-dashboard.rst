What Needs My Attention
=======================

Bugzilla’s `What needs my attention?`_ dashboard helps us to focus on the top-most important or urgent engineering tasks for releasing Firefox.

The dashboard is not designed for including everything on a person’s plate. It doesn’t attempt to prioritize normal work - it is just for the things that are more important than the normal stuff. It’s about individual engineer prioritization rather than team prioritization and it doesn’t claim to include all sources of high priority work (e.g. triage, responding to requests from HR, etc).

The dashboard is available to Mozilla engineers, using the icon

	.. image:: ../assets/icon_engineering.png
	  :alt: Engineering Icon

in the top right hand corner, after you log into Bugzilla.

The dashboard is a collection of some (sometimes non-obvious) Bugzilla searches. **Web Platform engineers** should check this page once per day, and ideally keep the list empty, so we can focus on our normal or planned work.


.. _What needs my attention?: https://bugzilla.mozilla.org/page.cgi?id=whats_next.html


Rules of thumb
--------------

Here are some rules of thumb about our priorities when it comes to bug fixing. Specific requests from a manager take precedence over these instructions.


Code review requests are not visible on this dashboard; please visit `Phabricator`_ to view those. In general it’s reasonable and important to prioritize Review Requests to unblock others.


Follow these general principles and use your best judgment with the help of a manager when necessary.


.. _Phabricator: https://phabricator.services.mozilla.com/


Highest priority tasks
~~~~~~~~~~~~~~~~~~~~~~

These are the things you should drop everything else for. Generally, work where you block others should be addressed as higher priority than non-blocking work.

#. Critical needinfos

	* Bugs that are needinfo? you and are marked as Severity = S1 defects or with the “sec-critical” keyword.

	* Bugs that are needinfo? you and are marked as being tracked against or blocking the current beta release. These bugs are potentially S2 once they are triaged and important. If we don’t act on these bugs, we’re in danger of delaying a release.

	* Bugs that are needinfo? you and are marked as a security issue without rating. These bugs are potentially sec-critical or sec-high once they are triaged and important.


#. Critical bugs assigned to you

	* Severity = S1 defects and bugs with the “sec-critical” keyword.


High priority tasks
~~~~~~~~~~~~~~~~~~~

High priority tasks are also “drop everything”, except that in this case “everything” doesn’t include anything in the “Highest priority” list. Generally, work where you block others should be addressed as higher priority than non-blocking work.

#. Important needinfos

	* Bugs that are needinfo? you and are marked as Severity = S2 defects or with the “sec-high” keyword.


#. Important bugs assigned to you

	* Severity = S2 defects and bugs with the “sec-high” keyword (both for things that are not disabled in the current release)
	* Note: Some teams have very long lists of S2 defects - see notes below on “Long High Priority task lists”

#. Your other needinfos (except for things that are self-needinfo).


Handling needinfos
------------------

TL;DR: Don’t leave people hanging.

When setting or clearing needinfos, please bear in mind the intended semantics of "needinfo": a bug is considered to be unactionable if it has an open needinfo request. A prompt response to a needinfo request on a bug is expected to ensure appropriate action can be taken.
Some people have long lists of needinfos. Please don’t ignore them. Here’s how we suggest you burn them down, and keep them down.


Old needinfos
~~~~~~~~~~~~~

Any needinfo older than 3 months has probably been forgotten about by the requester. It’s okay to declare needinfo bankruptcy. Consider replying something like:

* `Sorry that I didn't get to this needinfo earlier. Please request again if this needinfo is still needed.`

This clears the needinfo and offers that the requester can ask again if there is still a problem.


If you’re concerned about annoying the requester by clearing the needinfo, feel free to point them at this document.


New needinfos
~~~~~~~~~~~~~

For newer requests, don’t leave someone or something, .e.g BugBot hanging. If you can take action in the short term, do so. If you can’t do it straight away to come with a detailed response, consider replying something that might help to move forward the discussion, like:

* `I don't know, but you could do X in order to find out.`
* `Can you please provide more information about Y?`
* `I believe Z (someone else) can help.`

Providing visibility of your current plan helps move the needle, too. You can also consider replying something like:

* `Sorry that I don't have a quick thought or I don't expect to have bandwidth to help it now. Please request again if this should deserve a higher priority.`

This clears the needinfo.


A note on personal bug bookmarks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you want to keep track of some bugs you are not the assignee of and need more than yet another cc-mail in your bugzilla inbox, you can now use the new reminder system of bugzilla. It enables you to get reminded of a bug after some time and to see the list of bugs that currently have a reminder set for you. Keep in mind, that this is only visible to your bmo account and cannot be shared. If you set a "When" that is far enough in the future, you can use this as your personal bookmark list. In particular reminders can largely substitute the former habit of adding a "needinfo to myself", so please consider transforming your existing self-ni? to reminders over time.

In other words: we recommend to keep your "What needs my attention?" dashboard clear from your personal bookmarks, as they most likely do not need your immediate attention.

For some more details about the reminder feature please refer to this announcement: https://discourse.mozilla.org/t/happy-bmo-push-day-20240611-2/131062.


Review requests
---------------

This list doesn’t include Review Requests as as we are still investigating the feasibility of including them and applying these strict rules, but we might consider adding this to a future revision.

In the meanwhile, it’s worthwhile considering the use of peer review groups set up in Phabricator so that multiple engineers can assist in reviews.


Other notes
-----------

* Long lists of High Priority tasks: For some people and teams, the list of “High Priority” tasks is so long that you would never do normal work. If this is you then you should schedule these tasks alongside normal work. However making your task list manageable should still be a priority.

* `Severity`_ is defined, but things get a bit hazy when it comes to how we define severity for enhancements; this list is for serious defects only.

.. _Severity: https://firefox-source-docs.mozilla.org/bug-mgmt/guides/severity.html


Everything else
---------------

This list is not designed for including everything or prioritizing your normal work. Over time we’d like to bring teams’ practices for prioritizing new work more in line with each other, but that’s not the job of this note.

If you find that most of your time is spent on high or highest priority tasks, then it’s time to ask some questions to work out why - there’s likely to be a problem behind this and it sounds like a recipe for burnout, and we should do everything we can to even things out.
