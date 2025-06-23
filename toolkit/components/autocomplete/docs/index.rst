Autocomplete
=============

This document describes the autocomplete component. This is a mechanism to display a popup of suggested results when a user has partially filled in a text field. The autocomplete searches for results after each change is made to the value of the text field, and the popup will also appear if the user presses a cursor or tab key while a field is focused.

Autocomplete can be triggered in the browser chrome UI as well as in several cases for web pages, all managed by a single system with additional components tailored to each case to perform the specific search used to determine results.

Due to this need to work in several different contexts, some of the autocomplete components may seem a bit unweildly to the novice reader. Although there are some historical complexities that may no longer be needed, mainly the autocomplete handling needs to work both for web page fields, where the input field is in a different process than the autocomplete popup itself, as well as in browser UI fields.

You might also be interested in learning more about :doc:`address and credit card autofill </browser/extensions/formautofill/docs/index>`, which uses autocomplete, and is described separately.

Autocomplete Types in the Browser UI
------------------------------------

There are several autocomplete usages in the browser UI, for example, the autcomplete in the search field that can be optionally added to the main toolbar. If a text field in the browser UI wishes to participate in autocomplete, it needs to be based off of the ``autocomplete-input`` type defined in `autocomplete-input.js <https://searchfox.org/mozilla-central/source/toolkit/content/widgets/autocomplete-input.js>`_. This will handle opening the popup based on various key events in a consistent way across autocomplete fields. Some of this work is performed by a separate `nsAutocompleteController <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsAutoCompleteController.cpp>`_ object which is shared between browser UI and web page autocomplete.

Each type of autocomplete has a search component that is used to perform the actual search for results. Autocomplete search components have the contract id of the form ``@mozilla.org/autocomplete/search;1?name=AAA`` where ``AAA`` is a search type. The search type can be specified by the placing an ``autocompletesearch`` attribute on the text field. For example, assigning a value ``search-autocomplete`` will create the component ``@mozilla.org/autocomplete/search;1?name=search-autocomplete``.

The autocomplete components implement the `nsIAutoCompleteSearch <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsIAutoCompleteSearch.idl>`_ interface which contains a ``startSearch`` function to start a new search given a search term.

Multiple search types may be specified in the ``autocompletesearch`` attribute by separating them with spaces.

Note also that Thunderbird has some additional autocomplete search types that are used for searching address books and the like.

How Autocomplete is Triggered
-----------------------------

For autocomplete in the browser UI, the ``autocomplete-input`` gets the ``nsAutoCompleteController`` object - of which there is only one -- and this manages opening the popup, selecting values from the popup and filling in the input field when an item from the popup is selected.

For autocomplete in web pages, there is a separate form controller object. This form controller is implemeted in `nsFormFillController.cpp <https://searchfox.org/mozilla-central/source/toolkit/components/satchel/nsFormFillController.cpp>`_ and is the starting point where it is decided whether autocomplete and form fill apply to a particular form field. This component is loaded at application startup, and there is one of these form fill controllers per child process. This component is intended to be the equivalent to ``autocomplete-input`` for use in web pages, and is used because the real input field cannot be accessed from both the content and browser processes.

It is this form controller which conforms to the ``nsIAutoCompleteSearch`` interface and is implemented using the ``@mozilla.org/autocomplete/search;1?name=form-fill-controller`` component as described above. In this way, it operates similar to autocomplete in the browser UI using shared code.

The form controller listens for windows to be created by listening for the ``chrome-event-target-created`` observer notification. When a new page is loaded, it adds some event listeners (focus events, key events, mouse events) to the page which will be used to determine whether an autocomplete popop should be displayed in different circumstances.

In most circumstances, an autocomplete action begins when typing into an input field, or, by pressing the cursor or tab keys. For browser UI, this happens in event listeners within the ``autocomplete-input`` implementation. For web pages, this happens in the event listeners added within the `nsFormFillController <https://searchfox.org/mozilla-central/source/toolkit/components/satchel/nsFormFillController.cpp>`_. An autocomplete popup can also trigger when clicking the mouse in a field.

A special case occurs for password fields as the autocomplete popup can be opened when the input field is focused without having to type into the field. This is done in the handling for the focus event in `nsFormFillController.cpp <https://searchfox.org/mozilla-central/source/toolkit/components/satchel/nsFormFillController.cpp>`_. This is where changes would be made to determine whether a popup should be opened on different events. However, at this point, it has not been determined which type of autocomplete search is to be used, so any changes would be general to all autocomplete types. This is probably a good thing, so that the autocomplete behaviour is consistent across all types.

When a suitable key or mouse  event occurs which should trigger a possible autocomplete, the form controller passes this on to ``nsAutoCompleteController``.

The ``nsFormFillController`` and ``nsAutoCompleteController`` coordinate to determine whether an autocomplete popup should appear, where the former tends to focus more on the events related to the input field, and the latter tends to focus more on interacting with the autocomplete popup itself, but there is some overlap.

Note that both ``nsFormFillController`` and ``nsAutoCompleteController`` operate within the child process of the web page, and there is only one of each object per child process. For autocomplete in text boxes in the browser UI, there is a separate ``nsAutoCompleteController`` running in the browser process.

.. container::

  Some interfaces you might see being used within the formfill and autocomplete controllers and actors:

  `nsIAutoCompleteInput <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsIAutoCompleteInput.idl>`_
    This is an abstraction around the input field that autocomplete might apply to. For browser UI, this is implemented directly by ``autocomplete-input``. For form fill in web pages, this is implemented by the ``nsFormFillController``.

  `nsIAutoCompletePopup <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsIAutoCompletePopup.idl>`_
    This is an abstraction around the popup that appears for autocomplete. It is implemented by `autocomplete-popup.js <https://searchfox.org/mozilla-central/source/toolkit/content/widgets/autocomplete-popup.js>`_ for browser UI and supports displaying a number of different types of autocomplete rows. For form fill in web pages, it is implemented by the ``AutoCompleteChild`` actor which redirects through the process boundary to the actual popup also implemented by `autocomplete-popup.js <https://searchfox.org/mozilla-central/source/toolkit/content/widgets/autocomplete-popup.js>`_.

  `nsIAutoCompleteSearch <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsIAutoCompleteSearch.idl>`_
    This is an interface used to start and stop the search for specific results. Each search type will implement this to perform the search. For web pages, this is also implemented by the nsFormFillController.

  `nsIAutoCompleteResult <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsIAutoCompleteResult.idl>`_
    This interface is implemented by search results. For the form controller, these are returned by ``searchResultToAutoCompleteResult``. For other search types, the search is expected to return objects which implement ``nsIAutoCompleteResult`` directly.

Here is an example code flow for a field on a web page:

1. The user focuses an <input> text field. The form controller nsFormFillController looks to see if this is a password field. If not, it does some initialization to be ready for an autocomplete popup that could appear when the field is modified. This is done in `nsFormFillController::MaybeStartControllingInput <https://searchfox.org/mozilla-central/source/toolkit/components/satchel/nsFormFillController.cpp>`_. If the field is a password field, the popup can appear directly on focus.
2. The user types into the field. The form controller calls into the autocomplete `nsAutocompleteController::HandleText <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsAutoCompleteController.cpp>`_ method to determine when and how the popup should appear based on what was entered into the field. Note that the popup may already be visible and the user is continuing to type and further refine the results.
3. The autocomplete controller looks for a search component of the form ``@mozilla.org/autocomplete/search;1?name=AAA`` as described above. For web page form fill, this is back to the nsFormFillController which redirects to the AutoCompleteChild actor.
4. The AutoCompleteChild actor looks for a provider for the input field. Search providers are described in more detail in the :ref:`section on child actors<childactor>`. The provider is an object which is able to perform a search for autcomplete results and return them.
5. The search results are supplied to the listener passed to `nsIAutoCompleteSearch::StartSearch <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsIAutoCompleteSearch.idl>`_. This listener is the autocomplete controller ``nsAutocompleteController`` which takes the results and uses them to display an autocomplete popup if needed.

To summarize, if you wish to modify the events which cause an autocomplete popup to show, you would need to modify the `autocomplete-input.js <https://searchfox.org/mozilla-central/source/toolkit/content/widgets/autocomplete-input.js>`_ for browser UI and the `nsFormController <https://searchfox.org/mozilla-central/source/toolkit/components/satchel/nsFormFillController.cpp>`_ for web page fields. The `nsAutocompleteController <https://searchfox.org/mozilla-central/source/toolkit/components/autocomplete/nsAutoCompleteController.cpp>`_ contains code which is shared between both.

Autocomplete Types in Web Pages (Form Fill)
-------------------------------------------

For autocomplete in fields in web pages, there are three implementations in use today:

  * Login Manager, used for passwords.
  * Form Autofill, used for addresses and credit cards.
  * Form History, used as a default autocomplete for other types of fields. It also handles the <datalist> if there is one assigned to an input field.

Each of these is implemented by a pair of actors, one in each process.

There is an actor pair for each of these three types:

  ``Login Manager``:
    * `toolkit/components/passwordmgr/LoginManagerChild.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/passwordmgr/LoginManagerChild.sys.mjs>`_
    * `toolkit/components/passwordmgr/LoginManagerParent.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/passwordmgr/LoginManagerParent.sys.mjs>`_

  ``Form Autofill``:
    * `toolkit/components/formautofill/FormAutofillChild.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/FormAutofillChild.sys.mjs>`_
    * `toolkit/components/formautofill/FormAutofillParent.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/FormAutofillParent.sys.mjs>`_

  ``Form History``:
    * `toolkit/components/satchel/FormHistoryChild.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/satchel/FormHistoryChild.sys.mjs>`_
    * `toolkit/components/satchel/FormHistoryParent.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/satchel/FormHistoryParent.sys.mjs>`_

There is also a fourth more general actor pair used for the autocomplete system itself:

  ``AutoComplete``:
    * `toolkit/actors/AutoCompleteChild.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/actors/AutoCompleteChild.sys.mjs>`_
    * `toolkit/actors/AutoCompleteParent.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/actors/AutoCompleteParent.sys.mjs>`_

Since the autocomplete UI widgets for the popup only appear in the browser process, this last set of actors are used to abstract this out so that the autocomplete system can operate as if it was in the same process.

While support for other types of autocomplete for web pages can be added, in practice over time, some features have been hard-coded so only the three types of autocomplete above work directly. However, it would be simple to add other types or make support more general again.

.. _childactor:

Child Actor
-----------

The specifics of each autocomplete are handled by an autcomplete provider, managed by the autocomplete child actor ``AutoCompleteChild``. Each provider is able to handle autocomplete for a particular form field.

A custom provider may be specified by calling ``markAsAutoCompletableField(input, provider)`` to associate a provider with a particular form element. Multiple providers can be added for each element. However, currently there are only three types of autocomplete providers that exist, as listed above.

These providers are implemented by the child actor. For example, the form autofill provider is implemented by the ``FormAutofillChild`` actor. Form autofill adds a custom provider for an input field if it believes the field is an address or credit card field.

However, the other two are actually supplied by default by the ``AutoCompleteChild`` child actor, and are included in addition to any custom providers. If the field is a password field, or has been a password field in the past, the child autocomplete actor adds a default provider to handle logins called "LoginManager", which corresponds to the ``LoginManagerChild`` actor. For form history for all input fields, the "FormHistory" provider is used, corresponding to the ``FormHistoryChild`` actor.

When multiple providers are used, they are currently searched according to a priority, the order of which is
hard-coded in the ``AUTOCOMPLETE_PROVIDERS`` constant within `toolkit/actors/AutoCompleteParent.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/actors/AutoCompleteParent.sys.mjs>`_.

The provider has three important functions defined:

.. container::

  Child Actor interface:

  `shouldSearchForAutoComplete(input)`
    Should return true if the provider can provide search results for the given input field. This is where the provider might disable autocomplete if the field is of the wrong type or if a user preference is disabled.

  `getAutoCompleteSearchOption(input, string)`
    Used to retrieve any options or information from the form on the page that are specific to the search.

  `searchResultToAutoCompleteResult(string, input, results)`
    Convert the search results for display in the autocomplete popup. This happens after the search for results is complete and will be described later. This function returns objects which implement the ``nsIAutoCompleteResult`` interface.

Parent Actor
------------

While the search provider implements some functionality for autocomplete in the child process, it is the parent actor which performs the actual searching of data using the search term. This way the child process doesn't contain any information that might be private. The search happens by calling the parent actor's ``searchAutoCompleteEntries()`` function which is done by the parent actor ``AutocompleteParent``. This function will return a list of the search results. The results are an array of objects which are then returned back to the child actor / provider again.

.. container::

  Parent Actor interface:

  `searchAutoCompleteEntries(string, options)`
    Given a search string, return a list of search results that match. The options are the same as those returned from calling ``getAutoCompleteSearchOption`` in the child, described above.

Each of the three autocomplete types implements the ``searchAutoCompleteEntries`` function to search for results in a different way. For example the LoginManager looks up passwords that match the current hostname and adds a special item if needed to allow a password to be generated.

The options might be used to further define the search. For example, the login manager actors retrieve the maximum length of the input field (input.maxLength) and use this to calculate the maximum allowable password length for generated passwords.

The ``searchAutoCompleteEntries`` function returns a list of fairly opaque result objects that it is free to interpret in any way. The child actor / provider takes these results objects, and then passes them to the third of the provider functions ``searchResultToAutoCompleteResult``, for formatting. The formatted results implement the ``nsIAutoCompleteResult`` interface which the autocomplete components can use for display.

Note that if no provider provides a suitable result from its ``searchAutoCompleteEntries()`` function, ``searchResultToAutoCompleteResult`` may be called with a null records argument to indicate to display a default item in the popup, for example, a 'no results found' type message. This is really a choice made on a case by case basis -- one could also just return a search result that is formatted to look like an empty results item rather than relying on a default result. For this reason, search results don't necessarily correspond to found items -- some results may correspond to special actions or warning messages. The autocomplete popup has support for formatting result rows in a number of different ways.

Formatting Results
------------------

Each result object contains a number of fields that may be displayed.

Result Object Fields:

 :label: the label that appears in the autocomplete dropdown
 :value: the value that will be filled into the input field, which may be different than the label
 :image: image to appear in the dropdown row
 :style: a string name of the style that the row should be displayed in
 :comment: a serialized JSON object containing additional properties that might be used by the row

The autocomplete popup supports around 10 or so different styles for rows to appear in, for example: "status" for status rows, and "autofill" for form fill items. Each style uses the label, image and comment in a different way. You can add more styles by adding an implementation to
`autocomplete-richlistitem.js <https://searchfox.org/mozilla-central/source/toolkit/content/widgets/autocomplete-richlistitem.js>`_ and then adding a corresponding reference within `autocomplete-popup.js <https://searchfox.org/mozilla-central/source/toolkit/content/widgets/autocomplete-popup.js>`_ to match.

By default, the "value" field will be filled into the textbox when the item is selected from the popup. However, if the comment field contains ``fillMessageName`` and ``fillMessageData`` properties, these can be used to send custom messages to the actor which implements the autocomplete. The message name must match the actor, for example a message name ``FormAutofill:SaveAddress`` will map to the FormAutofill actor which was described earlier. This allows custom actions to be performed when selecting items from the autocomplete popup. The actor's ``onAutoCompleteEntrySelected`` function will be used to implement this. This behaviour is used when you need a custom action to be performed when selecting an item. For example, to open a settings panel.

The form fill item also supports previewing an item when the user hovers the mouse over an item or selects an item with the cursor keys without pressing enter. This operates in a similar way but uses the ``onAutoCompleteEntrySelected`` function. Form Autofill uses this to temporaily highlight the value while the item is selected from the dropdown.

A autofilled textbox is generally highlighted in yellow. This is done by setting the ``autofillstate`` property of the <input> element when the value is filled or previewed, and cleared when the user modifies the value.

See Also
--------

.. toctree::
   :maxdepth: 1

   /browser/extensions/formautofill/docs/index
