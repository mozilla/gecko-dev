var result = "NOTDONE";
console.log("loaded script");
fetch("http://localhost:21555")
  .then(_res => {
    result = "OK";
    console.log("OK");
  })
  .catch(_err => {
    result = "FAIL";
    console.log("FAIL");
  });
