assertEq(new SuppressedError(1, 2, 3, { cause: 4 }).cause, undefined);
