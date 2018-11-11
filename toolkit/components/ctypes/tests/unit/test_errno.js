Components.utils.import("resource://gre/modules/ctypes.jsm");

// Scope used to relaunch the tests with |ctypes| opened in a limited scope.
var scope = {};
var ctypes = ctypes;

function run_test()
{
  // Launch the test with regular loading of ctypes.jsm
  main_test();

  // Relaunch the test with exotic loading of ctypes.jsm
  Components.utils.unload("resource://gre/modules/ctypes.jsm");
  Components.utils.import("resource://gre/modules/ctypes.jsm", scope);
  ctypes = scope.ctypes;
  main_test();
}

function main_test()
{
  "use strict";
  let library = open_ctypes_test_lib();
  let set_errno = library.declare("set_errno", ctypes.default_abi,
                                   ctypes.void_t,
                                   ctypes.int);
  let get_errno = library.declare("get_errno", ctypes.default_abi,
                                   ctypes.int);

  for (let i = 50; i >= 0; --i) {
    set_errno(i);
    let status = ctypes.errno;
    do_check_eq(status, i);

    status = get_errno();
    do_check_eq(status, 0);

    status = ctypes.errno;
    do_check_eq(status, 0);
  }

  let set_last_error, get_last_error;
  try { // The following test is Windows-specific
    set_last_error = library.declare("set_last_error", ctypes.default_abi,
                                     ctypes.void_t,
                                     ctypes.int);
    get_last_error = library.declare("get_last_error", ctypes.default_abi,
                                     ctypes.int);

  } catch (x) {
    do_check_eq(ctypes.winLastError, undefined);
  }

  if (set_last_error) {
    do_check_neq(ctypes.winLastError, undefined);
    for (let i = 0; i < 50; ++i) {
      set_last_error(i);
      let status = ctypes.winLastError;
      do_check_eq(status, i);

      status = get_last_error();
      do_check_eq(status, 0);

      status = ctypes.winLastError;
      do_check_eq(status, 0);
    }
  }

  library.close();
}
