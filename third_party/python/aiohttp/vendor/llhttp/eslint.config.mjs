// @ts-check

import eslint from "@eslint/js";
import tseslint from "typescript-eslint";
import stylistic from "@stylistic/eslint-plugin";

export default tseslint.config(
  eslint.configs.recommended,
  ...tseslint.configs.recommended,
  {
    ignores: ["build", "lib"],
  },
  {
    files: [
      "bin/**/*.ts",
      "bench/**/*.ts",
      "src/**/*.ts",
      "test/**/*.ts",
      "eslint.config.js",
    ],    
    plugins: {
      "@stylistic": stylistic,
    },
    languageOptions: {
      parser: tseslint.parser,
      parserOptions: {
        project: "./tsconfig.test.json",
      },
    },
    rules: {
      "@stylistic/max-len": [
        2,
        {
          code: 120,
          ignoreComments: true,
        },
      ],
      "@stylistic/array-bracket-spacing": ["error", "always"],
      "@stylistic/operator-linebreak": ["error", "after"],
      "@stylistic/linebreak-style": ["error", "unix"],
      "@stylistic/brace-style": ["error", "1tbs", { allowSingleLine: true }],
      "@stylistic/indent": [
        "error",
        2,
        {
          SwitchCase: 1,
          FunctionDeclaration: { parameters: "first" },
          FunctionExpression: { parameters: "first" },
        },
      ],
    },
  }
);
