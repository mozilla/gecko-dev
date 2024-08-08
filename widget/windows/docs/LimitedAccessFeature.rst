===============================
Windows Limited Access Features
===============================

--------
Overview
--------

`Limited Access Features (LAF)
<https://learn.microsoft.com/en-us/uwp/api/windows.applicationmodel.limitedaccessfeatures?view=winrt-26100>`_ are
features which require a special token and attestation to unlock them before
their corresponding APIs can be called. These usually take the form
``com.microsoft.windows.featureFamily.name``. This is most relevant to Firefox in
the context of pinning to the Windows taskbar as the new Windows pinning APIs require
Firefox to first unlock the corresponding ``com.microsoft.windows.taskbar.pin``
LAF.

If we need to use a new Limited Access Feature we should notify Microsoft
if requested in the feature's documentation.

-------------------
Unlocking Procedure
-------------------

Applications which exist in a packaged context, such as MSIX installs,
have something called a Package Family Name (PFN). The PFN is generated
at build time for MSIX installs and varies between channels. This can be
accessed through Windows API calls on MSIX. For non-MSIX installs we are
provided a specific PFN by Microsoft which lives in the rc file in
the final install and can be modified in ``create_rc.py``.

The registry key corresponding to the LAF can be read at
``HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModel\LimitedAccessFeatures\<lafId>``
This key's default value contains a string which is the "key" for the
Limited Access Feature.

To get the complete unlocking token, the LAF identifier, LAF key, and PFN
can be combined in the format ``"<lafId>!<lafKey>!<PFN>"`` and then
encoded in SHA256. Taking the first 16 characters of this output and
converting to Base64 yields the final token. The overall process is
as follows:

.. code:: text

  Base64(SHA256Encode("<lafId>!<lafKey>!<PFN>")[0..16])

The other piece of unlocking is the attestation. We first need
the publisher identifier, which consists of the last 13 characters
of the PFN. With that, the attestation is assembled using the boilerplate
below:

.. code:: text

  <PFN[-13]> has registered their use of <lafId>
  with Microsoft and agrees to the terms of use.

The token and attestation can then be passed into ``LimitedAccessFeature.TryUnlockFeature()``
to unlock the corresponding APIs for use.
