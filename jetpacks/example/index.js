

console.log("loading the add-on!!");

var cm = require("sdk/context-menu");


cm.Item({
  label: "This Page Has Images",
  contentScript: 'self.on("context", function (node) {' +
                 '  return !!document.querySelector("img");' +
                 '});'
});
