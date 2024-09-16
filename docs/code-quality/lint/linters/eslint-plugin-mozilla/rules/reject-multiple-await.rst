reject-multiple-await
=====================

Disallows using `await await somePromise`. While this is valid syntax, this
usually unintentional and can have a slightly different behavior from using a
single `await`. Using `await await` should ideally be explained with a comment.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    await await somePromise;
    await await await somePromise;
    await (await somePromise);


Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    await somePromise;
