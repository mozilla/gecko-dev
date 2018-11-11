UA Widgets
==========

Introduction
------------

UA Widgets are intended to be a replacement of our usage of XBL bindings in web content. These widgets run JavaScript inside extended principal per-origin sandboxes. They insert their own DOM inside of a special, closed Shadow Root inaccessible to the page, called a UA Widget Shadow Root.

UA Widget lifecycle
-------------------

UA Widgets are generally constructed when the element is appended to the document and destroyed when the element is removed from the tree. Yet, in order to be fast, specialization was made to each of the widgets.

When the element is appended to the tree, a chrome-only ``UAWidgetBindToTree`` event is dispatched and is caught by a frame script, namely UAWidgetsChild.

UAWidgetsChild then grabs the sandbox for that origin (lazily creating it as needed), loads the script as needed, and initializes an instance by calling the JS constructor with a reference to the UA Widget Shadow Root created by the DOM. We will discuss the sandbox in the latter section.

When the element is removed from the tree, ``UAWidgetUnbindFromTree`` is dispatched so UAWidgetsChild can destroy the widget, if it exists. If so, the UAWidgetsChild calls the ``destructor()`` method on the widget, causing the widget to destruct itself.

When a UA Widget initializes, it should create its own DOM inside the passed UA Widget Shadow Root, including the ``<link>`` element necessary to load the stylesheet, add event listeners, etc. When destroyed (i.e. the destructor method is called), it should do the opposite.

**Specialization**: for video controls, we do not want to do the work if the control is not needed (i.e. when the ``<video>`` or ``<audio>`` element has no "controls" attribute set), so we forgo dispatching the event from HTMLMediaElement in the BindToTree method. Instead, a ``UAWidgetAttributeChanged`` event will cause the sandbox and the widget instance to construct when the attribute is set to true. The same event is also responsible for triggering the ``onattributechange()`` method on UA Widgets if the widget is already initialized.

Likewise, the datetime box widget is only loaded when the ``type`` attribute of an ``<input>`` is either `date` or `time`.

The specialization does not apply to the lifecycle of the UA Widget Shadow Root. It is always constructed in order to suppress children of the DOM element from the web content from receiving a layout frame.

UA Widget Shadow Root
---------------------

The UA Widget Shadow Root is a closed shadow root, with the UA Widget flag turned on. As a closed shadow root, it may not be accessed by other scripts. It is attached on host element which the spec disallow a shadow root to be attached.

The UA Widget flag enables the security feature covered in the next section.

The JavaScript sandbox
----------------------

The sandboxes created for UA Widgets are per-origin and set to the expanded principal. This allows the script to access other DOM APIs unavailable to the web content, while keeping its principal tied to the document origin. They are created as needed, backing the lifecycle of the UA Widgets as previously mentioned. These sandbox globals are not associated with any window object because they are shared across all same-origin documents. It is the job of the UA Widget script to hold and manage the references of the window and document objects the widget is being initiated on, by accessing them from the UA Widget Shadow Root instance passed.

While the closed shadow root technically prevents content from accessing the contents, we want a stronger guarantee to protect against accidental leakage of references to the UA Widget shadow tree into content script. Access to the UA Widget DOM is restricted by having their reflectors set in the UA Widgets scope, as opposed to the normal scope. To accomplish this, we avoid having any script (UA Widget script included) getting a hold of the reference of any created DOM element before appending to the Shadow DOM. Once the element is in the Shadow DOM, the binding mechanism will put the reflector in the desired scope as it is being accessed.

To avoid creating reflectors before DOM insertion, the available DOM interfaces is limited. For example, instead of ``createElement()`` and ``appendChild()``, the script would have to call ``createElementAndAppendChildAt()`` available on the UA Widget Shadow Root instance, to avoid receiving a reference to the DOM element and thus triggering the creation of its reflector in the wrong scope, before the element is properly associated with the UA Widget shadow tree. To find out the differences, search for ``Func="IsChromeOrXBLOrUAWidget"`` and ``Func="IsNotUAWidget"`` in in-tree WebIDL files.

Other things to watch out for
-----------------------------

As part of the implementation of the Web Platform, it is important to make sure the web-observable characteristics of the widget correctly reflect what the script on the web expects.

* Do not dispatch non-spec compliant events on the UA Widget Shadow Root host element, as event listeners in web content scripts can access them.
* The layout and the dimensions of the widget should be ready by the time the constructor returns, since they can be detectable as soon as the content script gets the reference of the host element (i.e. when ``appendChild()`` returns). In order to make this easier we load ``<link>`` elements load chrome stylesheets synchronously when inside a UA Widget Shadow DOM.
