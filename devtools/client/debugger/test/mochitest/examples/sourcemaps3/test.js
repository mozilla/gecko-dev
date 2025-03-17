import { fancySort } from "./sorted.js";

window.test = function originalTestName() {
  let test = ["b (30)", "a", "b (5)", "z"];
  let result = fancySort(test);
  console.log(result);
};

window.test2 = function () {
  const originalTestName2 = function() { 
    console.log(1);
  }
  originalTestName2();
  const originalTestName3 = () => { 
    console.log(2);
  }
  originalTestName3();
  function originalTestName4() { 
    run(() => { 
      console.log(3);
    })
    function run(func) { 
      func();
    }
  }
  originalTestName4();
  class OriginalFooBar { 
    constructor() { 
      console.log(4);
    }
  }
  new OriginalFooBar();
}