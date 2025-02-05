# `<moz-input-color>`

A component that allows a user to select from a palette of pre-determined color values.

```html story
<moz-input-color
    value="#7293C9"
    label="Pick a color"
></moz-input-color>

```

## Code

The source for `moz-input-color` can be found under
[toolkit/content/widgets/moz-input-color](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-input-color/moz-input-color.mjs).

You can find an examples of `moz-input-color` used in the [Theme selection in Firefox's Reader View](https://searchfox.org/mozilla-central/source/toolkit/components/reader/AboutReader.sys.mjs).

## Importing the component

Like other custom elements, you should usually be able to rely on `moz-input-color` getting lazy loaded at the time of first use. See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

`moz-input-color` can be used as follows:

```html
<moz-input-color
    value="#7293C9"
    label="Pick a color"
></moz-input-color>
```

### Fluent usage

The `label` for `moz-input-color` will generally be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes). Your Fluent identifiers should be formatted like this:

```
my-input-color =
  .label = This is the text
```

You can then provide `my-input-color` as the `data-l10n-id` attribute to show the label text:

```html
<moz-input-color
  value="#7293C9"
  data-l10n-id="my-input-color"
></moz-input-color>
```
