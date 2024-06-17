# MozRadioGroup

`moz-radio-group` is an element that allows users to select one option from a list of related options. It is designed to be used in tandem with our `moz-radio` custom element, which is a wrapper around an `<input type="radio">` HTML element.

```html story
<moz-radio-group name="contact" label="Select a contact method">
  <moz-radio value="email" label="Email" checked></moz-radio>
  <moz-radio value="phone" label="Phone"></moz-radio>
  <moz-radio value="mail" label="Mail"></moz-radio>
</moz-radio-group>
```

More information about this component including design, writing, and localization guidelines, as well as design assets, can be found on our [Acorn site](https://acorn.firefox.com/latest/components/radio/desktop-A9fsJE6U).

## Code

The source for `moz-radio-group` and `moz-radio` can be found under [toolkit/content/widgets/moz-radio-group/](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-radio-group/).


## When to use `moz-radio-group`

* Use a radio group when you are trying to get a user to select only one option from a relatively short list of options.
* Use a radio group over a select when it is advantageous to have all possible options to be immediately visible to the user.

## When not to use `moz-radio-group`

* If it should be possible for the user to select multiple options, use `moz-checkbox`.
* If the intention is for a user to turn something off or on with their selection, and for their selection to have an immediate effect, consider using `moz-toggle` instead.

## How to use `moz-radio-group`

### Importing the element

Like other custom elements, you should usually be able to rely on `moz-radio` and `moz-radio-group` getting lazy loaded at the time of first use. See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

### Combining `moz-radio` and `moz-radio-group`

`moz-radio` and `moz-radio-group` must be used together in order for the elements to behave as expected. `moz-radio` elements must also be direct children of `moz-radio-group`; they should not need to be wrapped in additional elements for purposes of positioning or spacing as those style attributes are provided by `moz-radio-group`.

If you are using the elements improperly you may see the following [console error](https://searchfox.org/mozilla-central/rev/a215fbd85843a91fcd8fdc33aa9cd9a357403f35/toolkit/content/widgets/moz-radio-group/moz-radio-group.mjs#260):

```sh
moz-radio can only be used in moz-radio-group element.
```

### Setting `name` for the group

The `name` attribute used to associate multiple `moz-radio` elements can only be set on the containing `moz-radio-group` element. This is different from HTML `<input type="radio">` elements where `name` can be set on each of the individual inputs. With `moz-radio-group` the `name` propagates down the the child `moz-radio` elements.

### Setting the `disabled` state

The `disabled` state can be set on `moz-radio-group` to disable all of its child `moz-radio` elements:

```html
<moz-radio-group name="group-disabled" disabled>
  <moz-radio value="disabled1" label="I'm disabled"></moz-radio>
  <moz-radio value="disabled2" label="I'm disabled too"></moz-radio>
</moz-radio-group>
```

`disabled` can also be set on `moz-radio` elements to control their state independently of the rest of the group:

```html
<moz-radio-group name="button-disabled">
  <moz-radio value="enabled" label="I'm enabled"></moz-radio>
  <moz-radio value="disabled" label="I'm disabled" disabled></moz-radio>
</moz-radio-group>
```

The one caveat to this is that it is not possible to programmatically enable an individual `moz-radio` element when the containing group is `disabled`.

### Fluent usage

The `label` properties of `moz-radio-group` and `moz-radio` will generally be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes). `data-l10n-attrs` are set automatically, so to get things working you just need to supply a `data-l10n-id` as you would with any other element.

For example the following Fluent messages:

```
moz-radio-group-id =
  .label = This is the label for the group
first-moz-radio-id =
  .label = This is the label for the first radio button
second-moz-radio-id =
  .label = This is the label for the second radio button
```

Would be used to set labels on the different elements as follows:

```html
<moz-radio-group data-l10n-id="moz-radio-group-id">
  <moz-radio data-l10n-id="first-moz-radio-id"></moz-radio>
  <moz-radio data-l10n-id="second-moz-radio-id"></moz-radio>
</moz-radio-group>
```
