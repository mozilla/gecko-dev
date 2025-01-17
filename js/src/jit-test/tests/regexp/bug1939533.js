assertEq(/(?i:[A-Z]{3})/.exec("abcdefg")[0], "abc");
assertEq(/(?i:[A-Z]{4})/.exec("abcdefg")[0], "abcd");
assertEq(/(?i:[A-Z]{5})/.exec("abcdefg")[0], "abcde");
assertEq(/(?i:[A-Z]{6})/.exec("abcdefg")[0], "abcdef");
