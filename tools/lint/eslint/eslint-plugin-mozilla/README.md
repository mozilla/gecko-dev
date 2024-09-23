# eslint-plugin-mozilla

A collection of rules that help enforce JavaScript coding standard in the Mozilla project.

These are primarily developed and used within the Firefox build system ([mozilla-central](https://hg.mozilla.org/mozilla-central/)), but are made available for other related projects to use as well.

## Notes

If you use prettier in your setup, you may need to extend from eslint-config-prettier
to ensure that any rules that conflict with prettier are disabled.
See [here for more information](https://github.com/prettier/eslint-config-prettier?tab=readme-ov-file#installation).

## Installation

### Within mozilla-central:

```
$ ./mach eslint --setup
```

### Outside mozilla-central:

Install ESLint [ESLint](http://eslint.org):

```
$ npm i eslint --save-dev
```

Next, install `eslint-plugin-mozilla`:

```
$ npm install eslint-plugin-mozilla --save-dev
```

## Documentation

For details about the rules, please see the [firefox documentation page](http://firefox-source-docs.mozilla.org/tools/lint/linters/eslint-plugin-mozilla.html).

## Source Code

The sources can be found at:

* Code: https://searchfox.org/mozilla-central/source/tools/lint/eslint/eslint-plugin-mozilla
* Documentation: https://searchfox.org/mozilla-central/source/docs/code-quality/lint/linters

## Bugs

Please file bugs in Bugzilla in the Lint component of the Testing product.

* [Existing bugs](https://bugzilla.mozilla.org/buglist.cgi?resolution=---&query_format=advanced&component=Lint&product=Testing)
* [New bugs](https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=Lint)

## Tests

The tests can only be run from within mozilla-central. To run the tests:

```
./mach eslint --setup
cd tools/lint/eslint/eslint-plugin-mozilla
npm run test
```
