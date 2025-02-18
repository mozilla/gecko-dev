"use strict";

console.log("message in parent worker");
throw new Error("error in parent worker");
