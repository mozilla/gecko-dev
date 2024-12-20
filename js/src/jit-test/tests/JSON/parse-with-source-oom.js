// |jit-test| --enable-json-parse-with-source

oomTest(() => {
    JSON.parse(
      '{"":0,"1":1,"2":2,"3":3,"4":4,"5":5,"6":6,"7":7,"1111":8}',
      (function () {})
    );
  });

oomTest(() => {
    JSON.parse(
      '["1", "", 2.718, {}, [1,2,3], {"":1, "1122": 8}]',
      (function () {})
    );
  });
