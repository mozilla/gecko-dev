Layout Code Reviewer Checklist
==============================

General
-------
- Follow the general `reviewer checklist
  <https://firefox-source-docs.mozilla.org/contributing/reviewer_checklist.html>`__.

Security issues
---------------

- **Watch for raw pointers that may have their data deleted out from under
  them**. Examples:

  - If you ever have a raw pointer to a dynamically allocated object, it's good
    to scrutinize whether the object might be destroyed before the last
    possible use of the raw pointer. For example: if you have a local variable
    that points to an object that's owned by a `frame's property table
    <https://searchfox.org/mozilla-central/source/layout/base/FrameProperties.h>`__,
    then consider whether the frame might remove/replace the property-table
    entry (or whether the frame itself might be destroyed) inside any of the
    function calls that happen while the local pointer is in scope.
  - Be aware that layout flushes
    (e.g. ``doc->FlushPendingNotifications(FlushType::Layout)``) can
    synchronously cause the frame tree (and even the document!) to be
    destroyed. Specifically: a layout flush can synchronously cause resize
    events to fire; and the event-listeners for those events can run arbitrary
    script, which could e.g. remove the iframe element that's hosting the
    document whose layout we're in the midst of flushing; and that can cause
    that document to be immediately destroyed, if there aren't any other strong
    references keeping it alive.
