/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jeff Walden <jwalden+code@mit.edu>
 */

options("strict");
for (var i = 0; i < 5; i++)
  Boolean.prototype = 42;

reportCompare(true, true);
