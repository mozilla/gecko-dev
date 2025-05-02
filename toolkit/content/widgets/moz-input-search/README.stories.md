# MozInputSearch

`moz-input-search` is a reusable component that is used for search inputs.

```html story
<moz-input-search style={{width: "500px"}}>
</moz-input-search>
```

## Usage guidelines

### When to use

- Use `moz-input-search` when you need a search input to help users filter through the relevant information.


## Code

The source for `moz-input-search` can be found under [toolkit/content/widgets/moz-input-search](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-input-search)

## How to use `moz-input-search`

### Importing the element

Like other custom elements, you should usually be able to rely on `moz-input-search` getting lazy-loaded at the time of first use.
See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

### Setting the `label`

Providing a label for the `moz-input-search` component is crucial for usability and accessibility:
- Helps users understand the purpose of this search input.
- Improves accessibility by ensuring screen readers can announce the function of the search input.

To set a label, use the `label` attribute.
In general, the label should be controlled by Fluent.

```html
<moz-input-search label="Label text not using Fluent"></moz-input-search>
```

```html story
<div style={{width: '500px'}}>
  <moz-input-search label="Label text not using Fluent"></moz-input-search>
</div>
```

### Setting the `aria-label`

If your use case calls for no visible label, then providing an `aria-label` for the `moz-input-search` is required for usability and accessibility:
- Improves accessibility by ensuring screen readers can announce the function of the search input.

To set the `aria-label`, use the `aria-label` attribute.
In general, the aria-label should be controlled by Fluent.

```html
<moz-input-search aria-label="non-visible label text not using Fluent"></moz-input-search>
```

```html story
<div style={{width: '500px'}}>
  <moz-input-search aria-label="non-visible label text not using Fluent"></moz-input-search>
</div>
```

### Setting the `description`

In order to set a description, use the `description` attribute.
In general, the description should be controlled by Fluent.
This is the preferred way of setting descriptions since it ensures consistency across multiple `moz-input-search` elements.

```html
<moz-input-search
  label="Label text not using Fluent"
  description="Description text not using Fluent">
</moz-input-search>
```

```html story
<div style={{width: '500px'}}>
  <moz-input-search
  label="Label text not using Fluent"
  description="Description text not using Fluent">
  </moz-input-search>
</div>
```

However `moz-input-search` does support a `slot` element if your use case is more complex.

```html
<moz-input-search label="Label text not using Fluent">
  <span slot="description">A more complex description via <slot></span>
</moz-input-search>
```

```html story
<div style={{width: '500px'}}>
  <moz-input-search label="Label text not using Fluent">
    <span slot="description">A more complex description via `<slot>`</span>
  </moz-input-search>
</div>
```

### Setting the `value`

The `value` attribute of `moz-input-search` sets the initial search term displayed in the search input field.
When this search term is modified, the `value` is updated with the modified text.
Note: do not use `value` as a placeholder!
Use the `placeholder` attribute instead if you need placeholder text.

```html
<moz-input-search label="Search preferences" value="privacy"></moz-input-search>
```

```html story
<div style={{width: '500px'}}>
<moz-input-search label="Search preferences" value="privacy"></moz-input-search>
</div>
```

### Setting the `placeholder`

The `placeholder` attribute of `moz-input-search` sets the placeholder text for the search input field.
In general, the placeholder should be controlled by Fluent.
This is the preferred way of setting placeholders since it ensures consistency across multiple `moz-input-search` elements.

```html
<moz-input-search label="Search preferences" placeholder="Enter search term"></moz-input-search>
```

```html story
<div style={{width: '500px'}}>
  <moz-input-search label="Search preferences" placeholder="Enter search term"></moz-input-search>
</div>
```

### Setting the `support-page`

The `support-page` attribute of `moz-input-search` sets the SUMO page to link out to for more information.
Use the SUMO slug for the value of this attribute.

```html
<moz-input-search label="Search" placeholder="Search terms" support-page="addons"></moz-input-search>
```

```html story
<div style={{width: '500px'}}>
<moz-input-search label="Search" placeholder="Search terms" support-page="addons"></moz-input-search>
</div>
```

### Setting the `accesskey`

The `accesskey` attribute of `moz-input-search` defines a keyboard shortcut for the search input.

```html
<moz-input-search label="Search" placeholder="Search terms" accesskey="S"></moz-input-search>
```

```html story
<div style={{width: '500px'}}>
<moz-input-search label="Search" placeholder="Search terms" accesskey="S"></moz-input-search>
</div>
```

### Setting the `disabled` state

In order to disable the `moz-input-search`, add `disabled=""` or `disabled` to the markup.

```html
<moz-input-search label="Search" disabled></moz-input-search>
```

```html story
<div style={{width: '500px'}}>
<moz-input-search label="Search" disabled></moz-input-search>
</div>
```

### Fluent usage

The `label`, `aria-label`, `description`, `placeholder`, and `accesskey` attributes of `moz-input-search` will generally be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes).
The relevant `data-l10n-attrs` are set automatically, so supply a `data-l10n-id` to get things working, as you would with any other element.

For example, if you have the following Fluent messages:

```
moz-input-search-label =
  .label = Label text
moz-input-search-label-placeholder =
  .label = Label text
  .placeholder = Placeholder text
moz-input-search-label-description-placeholder =
  .label = Label text
  .description = Description text
  .placeholder = Placeholder text
moz-input-search-with-accesskey =
  .label = Label text
  .accesskey = L
```
you can use those messages to set text and attributes on the different `moz-input-search` elements as follows:

```html
<moz-input-search data-l10n-id="moz-input-search-label"></moz-input-search>
<moz-input-search data-l10n-id="moz-input-search-label-placeholder"></moz-input-search>
<moz-input-search data-l10n-id="moz-input-search-label-description-placeholder"></moz-input-search>
<moz-input-search data-l10n-id="moz-input-search-with-accesskey"></moz-input-search>
```
