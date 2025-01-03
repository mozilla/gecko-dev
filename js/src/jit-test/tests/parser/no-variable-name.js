function assertThrowsMsgEndsWith(f, msg) {
  try {
    f();
    assertEq(0, 1);
  } catch (e) {
    assertEq(e instanceof SyntaxError, true);
    assertEq(e.message.endsWith(msg), true);
  }
}

assertThrowsMsgEndsWith(() => {
  Reflect.parse("var break;");
}, "missing variable name, got keyword 'break'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var case;");
}, "missing variable name, got keyword 'case'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var catch;");
}, "missing variable name, got keyword 'catch'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var class;");
}, "missing variable name, got keyword 'class'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var const;");
}, "missing variable name, got keyword 'const'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var continue;");
}, "missing variable name, got keyword 'continue'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var debugger;");
}, "missing variable name, got keyword 'debugger'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var default;");
}, "missing variable name, got keyword 'default'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var delete;");
}, "missing variable name, got keyword 'delete'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var do;");
}, "missing variable name, got keyword 'do'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var else;");
}, "missing variable name, got keyword 'else'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var enum;");
}, "missing variable name, got reserved word 'enum'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var export;");
}, "missing variable name, got keyword 'export'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var extends;");
}, "missing variable name, got keyword 'extends'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var false;");
}, "missing variable name, got boolean literal 'false'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var finally;");
}, "missing variable name, got keyword 'finally'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var for;");
}, "missing variable name, got keyword 'for'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var function;");
}, "missing variable name, got keyword 'function'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var if;");
}, "missing variable name, got keyword 'if'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var import;");
}, "missing variable name, got keyword 'import'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var in;");
}, "missing variable name, got keyword 'in'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var instanceof;");
}, "missing variable name, got keyword 'instanceof'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var new;");
}, "missing variable name, got keyword 'new'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var null;");
}, "missing variable name, got null literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var return;");
}, "missing variable name, got keyword 'return'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var super;");
}, "missing variable name, got keyword 'super'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var switch;");
}, "missing variable name, got keyword 'switch'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var this;");
}, "missing variable name, got keyword 'this'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var throw;");
}, "missing variable name, got keyword 'throw'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var true;");
}, "missing variable name, got boolean literal 'true'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var try;");
}, "missing variable name, got keyword 'try'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var typeof;");
}, "missing variable name, got keyword 'typeof'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var var;");
}, "missing variable name, got keyword 'var'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var void;");
}, "missing variable name, got keyword 'void'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var while;");
}, "missing variable name, got keyword 'while'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var with;");
}, "missing variable name, got keyword 'with'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var;");
}, "missing variable name, got ';'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var a, , b;");
}, "missing variable name, got ','");

assertThrowsMsgEndsWith(() => {
  Reflect.parse("for (var else of arr) {}");
}, "missing variable name, got keyword 'else'");

// Object and Array Binding Tests
var o = { a: 10 };
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a: /a/} = o;");
}, "missing variable name, got regular expression literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a:} = o;");
}, "missing variable name, got '}'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a:1} = o;");
}, "missing variable name, got numeric literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a:'a'} = o;");
}, "missing variable name, got string literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a: , b} = o;");
}, "missing variable name, got ','");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a: `template`} = o;");
}, "missing variable name, got template literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a: ()=>10} = o;");
}, "missing variable name, got '('");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a: !true} = o;");
}, "missing variable name, got '!'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var [/a/] = [];");
}, "missing variable name, got '/'");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var [1] = [];");
}, "missing variable name, got numeric literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var [, 1] = [];");
}, "missing variable name, got numeric literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a: [{b: 'str'}]} = o;");
}, "missing variable name, got string literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {a: 10n} = o;");
}, "missing variable name, got bigint literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var { [10]: 10 } = o;");
}, "missing variable name, got numeric literal");
assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {...42} = o;");
}, "missing variable name, got numeric literal");

assertThrowsMsgEndsWith(() => {
  Reflect.parse("var { [propName]: } = o;");
}, "missing variable name, got '}'");

assertThrowsMsgEndsWith(() => {
  Reflect.parse("var {f(): x = 10} = 0");
}, "missing variable name, got identifier");
