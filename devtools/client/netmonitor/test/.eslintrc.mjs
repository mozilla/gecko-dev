"use strict";

module.exports = {
  env: {
    jest: true,
  },
  overrides: [
    {
      // eslint-plugin-html doesn't automatically detect module sections in
      // html files. Enable these as a module here.
      files: ["html_module-script-cache.html"],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
};
