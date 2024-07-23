gczeal(0);

function address(s) {
  return JSON.parse(stringRepresentation(s)).address;
}

// Create a nursery string to deduplicate the test string to.
var dedup_to = newString("abcdefghijklmnopqrstuvwxyz01", {
  tenured: false,
  capacity: 28,
});

// ...and make it be processed first during a minorgc by putting something in the store buffer that points to it.
var look_at_me_first = newDependentString(dedup_to, 3, { tenured: true });

var orig_owner = newString("abcdefghijklmnopqrstuvwxyz", {
  capacity: 4000,
  tenured: true,
});
var trouble_dep = newDependentString(orig_owner, 1, { tenured: true });
var inner_rope = newRope(orig_owner, "0");
var flattened_inner_rope = ensureLinearString(inner_rope);

var outer_rope = newRope(flattened_inner_rope, "1");
var flattened_outer_rope = ensureLinearString(outer_rope);

minorgc();
assertEq(trouble_dep, "bcdefghijklmnopqrstuvwxyz");
