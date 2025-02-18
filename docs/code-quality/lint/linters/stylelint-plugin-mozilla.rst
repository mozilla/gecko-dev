========================
Mozilla Stylelint Plugin
========================

This is the documentation of Mozilla Stylelint Plugin.

Rules
=====

The plugin implements the following rules:

.. toctree::
   :maxdepth: 1
   :glob:

   stylelint-plugin-mozilla/rules/*

Tests
=====

The tests for stylelint-plugin-mozilla are written using
`stylelint-test-rule-node`_ as recommended by the `Stylelint docs`_.

.. _stylelint-test-rule-node: https://www.npmjs.com/package/stylelint-test-rule-node
.. _Stylelint docs: https://stylelint.io/developer-guide/plugins#testing

Running Tests
-------------

The stylelint plugin has some self tests, these can be run via:

.. code-block:: shell

   $ cd tools/lint/stylelint/stylelint-plugin-mozilla
   $ npm ci
   $ npm run test
