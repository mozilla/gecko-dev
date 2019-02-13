// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1146136;
var summary =
  'Parenthesized "destructuring patterns" are not usable as destructuring ' +
  'patterns';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Don't pollute the top-level script with eval references.
var savedEval = this[String.fromCharCode(101, 118, 97, 108)];

function checkError(code, nonstrictErr, strictErr)
{
  function helper(exec, prefix, err)
  {
    var fullCode = prefix + code;
    try
    {
      var f = exec(fullCode);

      var error =
        "no early error, parsed code <" + fullCode + "> using " + exec.name;
      if (typeof f === "function")
      {
        try
        {
          f();
          error += ", and the function can be called without error";
        }
        catch (e)
        {
          error +=", and calling the function throws " + e;
        }
      }

      throw new Error(error);
    }
    catch (e)
    {
      assertEq(e instanceof err, true,
               "expected " + err.name + ", got " + e + " " +
               "for code <" + fullCode + "> when parsed using " + exec.name);
    }
  }

  helper(Function, "", nonstrictErr);
  helper(Function, "'use strict'; ", strictErr);
  helper(savedEval, "", nonstrictErr);
  helper(savedEval, "'use strict'; ", strictErr);
}

// Parenthesized destructuring patterns don't trigger grammar refinement, so we
// get the currently-usual ReferenceError for an invalid assignment target, per
// 12.14.1 second bullet.
checkError("var a, b; ([a, b]) = [1, 2];", ReferenceError, ReferenceError);
checkError("var a, b; ({a, b}) = { a: 1, b: 2 };", ReferenceError, ReferenceError);

// *Nested* parenthesized destructuring patterns, on the other hand, do trigger
// grammar refinement.  But subtargets in a destructuring pattern must be
// either object/array literals that match the destructuring pattern refinement
// *or* valid simple assignment targets.  Parenthesized object/array patterns
// are neither.  And so 12.14.5.1 third bullet requires an early SyntaxError.
checkError("var a, b; ({ a: ({ b: b }) } = { a: { b: 42 } });", SyntaxError, SyntaxError);
checkError("var a, b; ({ a: ([b]) } = { a: [42] });", SyntaxError, SyntaxError);

Function("var a, b; [(a), b] = [1, 2];")();
Function("var a, b; [(a) = 5, b] = [1, 2];")();
Function("var a, b; [(arguments), b] = [1, 2];")();
Function("var a, b; [(arguments) = 5, b] = [1, 2];")();
Function("var a, b; [(eval), b] = [1, 2];")();
Function("var a, b; [(eval) = 5, b] = [1, 2];")();

var repair = {}, demolition = {};

Function("var a, b; [(repair.man), b] = [1, 2];")();
Function("var a, b; [(demolition['man']) = 'motel', b] = [1, 2];")();
Function("var a, b; [(demolition['man' + {}]) = 'motel', b] = [1, 2];")(); // evade constant-folding

function classesEnabled()
{
  try
  {
    new Function("class B { constructor() { } }; class D extends B { constructor() { super(); } }");
    return true;
  }
  catch (e if e instanceof SyntaxError)
  {
    return false;
  }
}

if (classesEnabled())
{
  Function("var a, b; var obj = { x() { [(super.man), b] = [1, 2]; } };")();
  Function("var a, b; var obj = { x() { [(super[8]) = 'motel', b] = [1, 2]; } };")();
  Function("var a, b; var obj = { x() { [(super[8 + {}]) = 'motel', b] = [1, 2]; } };")(); // evade constant-folding
}

// In strict mode, assignment to funcall *immediately* triggers ReferenceError
// before we can recognize this doesn't even match the destructuring grammar to
// begin with.  Bleh.  :-(  Probably they should all be made SyntaxError in the
// specs; see <https://bugs.ecmascript.org/show_bug.cgi?id=4375>.
checkError("var a, b; [f() = 'ohai', b] = [1, 2];", SyntaxError, ReferenceError);
checkError("var a, b; [(f()) = 'kthxbai', b] = [1, 2];", SyntaxError, ReferenceError);

Function("var a, b; ({ a: (a), b} = { a: 1, b: 2 });")();
Function("var a, b; ({ a: (a) = 5, b} = { a: 1, b: 2 });")();


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
