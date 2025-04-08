# MozInputFolder

`moz-input-folder` is a reusable component that provides the ability to browse and pick a folder from the file system. It displays the path and icon of the selected folder to the user. It can also be configured to display custom text if needed.

```html story
<div style={{width: "500px"}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder
    label="Save files to"
    placeholder="Select folder">
  </moz-input-folder>
</div>
```

## Usage guidelines

### When to use

* Use `moz-input-folder` when you want to allow a user to select a directory.

### When not to use

* When users need to select individual files rather than folders.

## Code

The source for `moz-input-folder` can be found under [toolkit/content/widgets/moz-input-folder/](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-input-folder)

## How to use `moz-input-folder`

### Importing the element

Like other custom elements, you should usually be able to rely on `moz-input-folder` getting lazy loaded at the time of first use.
See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

### Setting the `label`

Providing a label for the moz-input-folder component is crucial for usability and accessibility:
* Helps users understand the purpose of the folder picker.
* Improves accessibility by ensuring screen readers can announce the function of the input.

To set a label, use the `label` attribute. In general, the label should be controlled by Fluent.

```html
<moz-input-folder label="Label text"></moz-input-folder>
```

```html story
<div style={{width: '500px'}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder label="Label text"></moz-input-folder>
</div>
```

### Setting the `description`

In order to set a description, use the `description` attribute.
In general, the description should be controlled by Fluent. This is the the preferred way of setting  descriptions since it ensures consistency across instances of `moz-input-folder`.

```html
<moz-input-folder label="Label" description="Description text"></moz-input-folder>
```

```html story
<div style={{width: '500px'}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder
    label="Label"
    description="Description text">
  </moz-input-folder>
</div>
```

However, `moz-input-folder` does support a `slot` element if your use case is more complex.

```html
<moz-input-folder label="Label">
  <span slot="description">A more complex description</span>
</moz-input-folder>
```

```html story
<div style={{width: "500px"}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder label="Label">
    <span slot="description">A more <b>complex</b> description</span>
  </moz-input-folder>
</div>
```

### Setting the `value`

The `value` attribute of `moz-input-folder` sets the initial folder path displayed in the input field. When a new folder is selected, the `value` gets updated with that folder's path.

```html
<moz-input-folder label="Save files to:" value="/User/Downloads"></moz-input-folder>
```

```html story
<div style={{width: '500px'}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder label="Save files to:" value="/User/Downloads"></moz-input-folder>
</div>
```

### Setting the `displayValue`

Use `displayValue` property to display something other than a folder path in the input element. Listen to the `moz-input-folder` `change` event to set a `displayValue` after the new folder was selected. The `folder` property represents the selected folder in the file system (`nsIFile` object). You can use properties of the `folder` when setting a `displayValue` (e.g., `folder.path`, `folder.displayName` or `folder.leafName`).

```html
<moz-input-folder label="Save files to:" value="/User/Downloads" displayvalue="Downloads"></moz-input-folder>
```

```js
// Code example
let mozInputFolder = document.querySelector("moz-input-folder");
mozInputFolder.addEventListener(
  "change",
  () => {
    mozInputFolder.displayValue = mozInputFolder.folder.displayName;
  }
);
```

```html story
<div style={{width: '500px'}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder label="Save files to:" value="/User/Downloads" displayvalue="Downloads"></moz-input-folder>
</div>
```

### Setting the `disabled` state

In order to disable the `moz-input-folder`, add `disabled=""` or `disabled` to the markup with no value.

```html
<moz-input-folder label="Label" disabled></moz-input-folder>
```

```html story
<div style={{width: '500px'}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder label="Label" disabled></moz-input-folder>
</div>
```

### Setting the `accesskey`

`accesskey` defines an keyboard shortcut for the input.

```html
<moz-input-folder label="Label with accesskey" accesskey="L"></moz-input-folder>
```

```html story
<div style={{width: '500px'}} onClickCapture={e => e.stopPropagation()}>
  <moz-input-folder label="Label with accesskey" accesskey="L"></moz-input-folder>
</div>
```

### Fluent usage

The `label`, `description`, `placeholder` and `accesskey` attributes of `moz-input-folder` will generally be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes).
The relevant `data-l10n-attrs` are set automatically, so to get things working you just need to supply a `data-l10n-id` as you would with any other element.

For example, the following Fluent messages:

```
moz-input-folder-label =
  .label = Save files to
moz-input-folder-placeholder =
  .label = Save files to
  .placeholder = Select folder
moz-input-folder-description =
  .label = Save files to
  .description = Description text
  .placeholder = Select folder
moz-input-folder-with-accesskey =
  .label = Save files to
  .accesskey = v
```

would be used to set text and attributes on the different `moz-input-folder` elements as follows:

```html
<moz-input-folder data-l10n-id="moz-input-folder-label"></moz-input-folder>
<moz-input-folder data-l10n-id="moz-input-folder-placeholder"></moz-input-folder>
<moz-input-folder data-l10n-id="moz-input-folder-description"></moz-input-folder>
<moz-input-folder data-l10n-id="moz-input-folder-with-accesskey"></moz-input-folder>
```
