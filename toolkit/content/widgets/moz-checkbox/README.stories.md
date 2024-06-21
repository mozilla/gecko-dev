# MozCheckbox

`moz-checkbox` is an element that lets users select multiple options from a list of choices.
It can also function as a toggle, allowing users to make a binary choice like turning a feature on or off.
It is a wrapper of `<input type="checkbox">` with built-in support for label and description text, as well as icons.

```html story
<moz-checkbox label="Toggles a control" description="A description about the control">
</moz-checkbox>
```

More information about this component including design, writing, and localization guidelines, as well as design assets, can be found on our [Acorn site](https://acorn.firefox.com/latest/components/checkbox/desktop-535kxbzV).

## Code

The source for `moz-checkbox` can be found under [toolkit/content/widgets/moz-checkbox/](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-checkbox)

## When to use `moz-checkbox`
- Use `moz-checkbox` when you want to allow a user to select at least one option from a list.

## When not to use `moz-checkbox`

- When only one choice can be active in a list of a few mutually exclusive options, use `moz-radio-group` instead.
- When only one choice can be active in a list of many mutually exclusive options, use a `select` element instead.
- If the intention is for a user to turn something off or on with their selection, and for their selection to have an immediate effect, consider using `moz-toggle` instead.

## How to use `moz-checkbox`

### Importing the element

Like other custom elements, you should usually be able to rely on `moz-checkbox` getting lazy loaded at the time of first use.
See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

### Setting the `checked` state

If you need a `moz-checkbox` to be rendered as a filled-in checkbox by default, you can use `checked=""` or `checked` as an attribute just as if the element was a HTML checkbox.

```html
<moz-checkbox label="a descriptive label" checked></moz-checkbox>
```
```html story
<moz-checkbox label="a descriptive label" checked></moz-checkbox>
```


After initial render, the `checked` value is the current state of the inner checkbox element.

### Setting the `disabled` state

In order to disable the `moz-checkbox`, add `disabled=""` or `disabled` to the markup with no value.

```html
<moz-checkbox label="a descriptive label" disabled></moz-checkbox>
```
```html story
<moz-checkbox label="a descriptive label" disabled></moz-checkbox>
```

### Setting an icon

In order to have an icon appear next to the checkbox element, use the `.iconSrc` property or `iconsrc` attribute.

```html
<moz-checkbox label="a descriptive label" iconsrc="chrome://global/skin/icons/edit-copy.svg"></moz-checkbox>
```

```html story
<moz-checkbox label="a descriptive label" iconsrc="chrome://global/skin/icons/edit-copy.svg"></moz-checkbox>
```

### Setting a description

In order to set a description, use the `discription` attribute.
In general, the description should be controlled by Fluent (and is the preferred way of handling descriptions).

```html
<moz-checkbox label="a descriptive label" description="Description text for your checkbox"></moz-checkbox>
```

```html story
<moz-checkbox label="a descriptive label" description="Description text for your checkbox"></moz-checkbox>
```

However, `moz-checkbox` does support a `slot` element if your use case is more complex.

```html
<moz-checkbox label="a descriptive label">
  <span slot="description">A more complex description</span>
</moz-checkbox>
```

```html story
<moz-checkbox label="a descriptive label">
  <span slot="description">A more complex description</span>
</moz-checkbox>
```


### Fluent usage

The `label` and `description` attributes of `moz-checkbox` will generally be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes).
The relevant `data-l10n-attrs` are set automatically, so to get things working you just need to supply a `data-l10n-id` as you would with any other element.

For example, the following Fluent messages:

```
first-moz-checkbox-id =
  .label = This is the first label
second-moz-checkbox-id =
  .label = This is the second label
  .description = This is additional description text for the checkbox
```

would be used to set a label or a label and a description on the different `moz-checkbox` elements as follows:

```html
<moz-checkbox data-l10n-id="first-moz-checkbox-id"></moz-checkbox>
<moz-checkbox data-l10n-id="second-moz-checkbox-id"></moz-checkbox>
```
