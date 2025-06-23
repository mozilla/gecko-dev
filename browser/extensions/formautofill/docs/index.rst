Form Autofill
=============

Introduction
------------

Form Autofill saves users time and effort when making online purchases by storing their personal information in a profile and automatically populating form fields when the user requires it.

Our objective is to increase user engagement, satisfaction and retention for frequent online shoppers (those who make an online purchase at least once per month). We believe this can be achieved by enabling users to complete forms and “check out” in e-commerce flows as quickly and securely as possible.

Form Autofill handles filling in addresses and credit cards into forms. While the filling aspect of this is straightforward and uses the same mechanism as other autocomplete types, much of the additional work involves analyzing and classifying the form to determine which form field corresponds to which type of data. For instance, text boxes are classified into different types of address data.

For historical reasons, part of the form autofill is implemented using an extension, located in `browser/extensions/formautofill <https://searchfox.org/mozilla-central/source/browser/extensions/formautofill>`_. It is this extension which adds the form autofill actors used to handle the search for possible results. However, most of the work is done via code in `toolkit/components/formautofill <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill>`_.

Form Autofill uses the autocomplete popup mechanism to show a list of suggestions to the user based on what they have typed so far. The user can also press the cursor key or tab key to open the autocomplete popup while the focus is within a field.

For more information about autocomplete, see :doc:`Autocomplete </toolkit/components/autocomplete/docs/index>`.


How Form Autofill Works
-----------------------

The form fill actor ``FormAutofillChild`` waits for form fields to be focused. When a field is focused, it starts the process of analyzing the form and picking out which fields might be address or credit card related. This process is primarily done by the ``getFormInfo`` function within `FormAutofillHeuristics.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/shared/FormAutofillHeuristics.sys.mjs>`_. This process is separate from the mechanism used to trigger an autocomplete popup.

Form Autofill can handle filling in <input>, <textarea> and <select> elements.

There are three mechanisms that can be used to identify a field:

1. **autocomplete attributes**: authors may place an autocomplete attribute on a form field to identify its purpose. This is most likely to be correct although sometimes authoring errors will occur and we attempt to fix some common cases.
2. **fathom**: Fathom is a simple learning system that uses a selection of rules to identify credit card related fields. It currently is used to detect the credit card name and number fields (``cc-name`` and ``cc-number``) but does not detect other credit card fields or operate on address fields.
3. **regular expressions**: a large list of regular expressions located in `HeuristicsRegExp.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/shared/HeuristicsRegExp.sys.mjs>`_ that is used to match the ids, names, placeholders and labels associated with form elements. This is where most of the analysis of form fields is done for addresses.

The ``element.getAutocompleteInfo()`` function provides the parsed result of ``autocomplete`` attribute which includes the field name and section information defined in `autofill spec <https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#autofill>`_

Each field in a form is classified into a number of different field types, for example: ``email``, ``address-line1``, ``country`` and so on. Many fields will not be able to be placed into a definite type, and many forms will have multiple fields classified with the same type. Work is ongoing to reduce the number of errors made in classifying fields.

After fields are classified, some additional heuristics are run (within the ``parseAndUpdateFieldNamesParent`` function in `FormAutofillHeuristics.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/shared/FormAutofillHeuristics.sys.mjs>`_). These are responsible for further checking for common field classification issues. For example, when multiple address line fields appear in a row, it is likely that they represent a separate line of the address (street, apartment number, building, etc) and appear in order. Another example is that it is common on Japanese web sites to have a separate set of name fields to indicate the pronounciation of one's name; heuristics are included to recognize this situation.

Note that the heuristics within ``parseAndUpdateFieldNamesParent`` are done within the parent browser process so no longer have access to the form fields directly, so any information needed will need to be stored in the field data (usually called ``fieldDetails`` within the code).

However, there are a few heuristics in ``parseAndUpdateFieldNamesContent`` that are done in the web page process beforehand. Most processing happens in the parent process afterwards because all of the form fields across different subframes (iframes) can be searched and collected together as a single form. There are a number of web sites that break up each field into separate child iframes.

Once fields have been identified, they are split into sections. Some sites will have multiple sections for addresses, one for a billing address and one for a shipping address. This handling is done by groupFields within `FormAutofillSection.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/shared/FormAutofillSection.sys.mjs>`_. The ``autocomplete`` attribute may be used on a field in a web page to be explicit about which section to use for that field, but otherwise the heuristics try to determine this automatically.

Once the fields are identified, a message is sent back to the child process so that ``markAsAutoCompletableField()`` may be called on each detected field, allowing the autocomplete actor to recognize that the form fill actor is able to provide results for that form field. In addition, the section and form field details are saved in the field ``sectionsByRootId`` within ``FormAutofillParent`` to that they may be retreived when the autocomplete is performed.

Once ``markAsAutoCompletableField()`` is called for a field, form autofill is available for that field and the user's saved address and credit card information will be examined to see if they apply as possible suggestions in the autocomplete dropdown popup when the user enters or types into that field.

Additional Cases
-----------------

* Some fields do not exist in <form> elements. In this case, the entire document is used as if it was a form.
* Sometimes a set of fields will be split across multiple child iframes. These forms are processed together and explains why some heuristics are performed in the browser process after all of the frames have been analyzed, instead of in the web page child process.
* Often a web site will change the form in response to a user change. For example, changing the country in a form will change other fields to correspond to fields specific to that country. This is referred to as a dynamic form and is handled by rescanning the form and picking out any differences.

Country Meta Data
-----------------

The specifics for each region is defined in the file `AddressMetaData.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/shared/AddressMetaData.sys.mjs>`_. This file contains information about the regions in each country, postal code formats, languages used in each region and so forth. This file contains links to documentation about the format of this data and is a good place to look for information that might be country specific. For example, when we look at data for Brazil, we can see a list of states (federative units), and also see that Brazil has defined a "sublocality_name_type" field which indicates a third level of region besides state and city, used for the neighborhood. The neighborhood appears in the address preference dialog when adding Brazilian addresses and can be autofilled as needed. Most other regions do not include a neighborhood field.

Form Submission
-----------------

When a form is submitted, a prompt may be show asking the user if they wish to save the address or credit card information. This helps the user realize that the autofill capability is available. The filled in data on the form can optionally be automatically saved as a new address or credit card. This is done within the ``onFormSubmit`` function of `FormAutofillParent <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/FormAutofillParent.sys.mjs>`_.

In general, however, we do not show the prompt to save a new address or credit card if the user has not filled in enough fields, fillin is disabled in the user's region, or the form does not contain the right fields. Which fields are necessary is dependent on the region. For example, typically, a street, city and postal code is required. The specific data needed to save the form data is determined from the `AddressMetaData.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/shared/AddressMetaData.sys.mjs>`_ as described above.

If an address is determined to be the same as an existing saved address, the prompt does not appear. Some heuristics within
`AddressParser.sys.mjs <https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/shared/AddressParser.sys.mjs>`_ are used to determine if the address is the same as an existing saved address, because this can be inexact matching, for example, 'Floor 2' and '2nd floor' are different strings but refer to the same thing. This ensures that the amount of times the user is prompted is reduced when the address is similar.

Report Issues
-------------

If you find any issues about filling a form with incorrect values, please file a `new bug <https://bugzilla.mozilla.org/enter_bug.cgi?product=Toolkit&component=Form%20Autofill>`_ to Toolkit::Form Autofill component.
