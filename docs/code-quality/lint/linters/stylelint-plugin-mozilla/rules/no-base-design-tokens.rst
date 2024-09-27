no-base-design-tokens
=====================

This rule checks that CSS declarations do not use `base color token variables
<https://firefox-source-docs.mozilla.org/toolkit/themes/shared/design-system/docs/README.design-tokens.stories.html#base>`_
directly. Instead, developers should reference higher-level, semantic tokens to
ensure color usage is consistent and maintainable.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: css

    a {
      color: var(--color-blue-60);
    }

.. code-block:: css

    .custom-button {
      background-color: var(--color-gray-90);
    }

Examples of correct code for this rule:
---------------------------------------

.. code-block:: css

    a {
      color: var(--text-color-link);
    }

.. code-block:: css

    :root {
      --custom-button-background-color: var(--color-gray-90);
    }

    .custom-button {
      background-color: var(--custom-button-background-color);
    }
