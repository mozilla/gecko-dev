# MozButton

`moz-button` is a reusable, accessible, and customizable button component designed to indicate an available action for the user.
It is a wrapper of the `<button>` element with built-in support for label and title, as well as icons.
It supports various types (`default`, `primary`, `destructive`, `icon`, `icon ghost`, `ghost`) and sizes (`default`, `small`).

```html story
<div style={{ display: 'flex', justifyContent: 'center', gap: '1rem', flexWrap: 'wrap' }}>
  <moz-button label="Default">"Default"</moz-button>
  <moz-button type="primary" label="Primary"></moz-button>
  <moz-button type="destructive" label="Destructive"></moz-button>
  <moz-button iconSrc="chrome://global/skin/icons/more.svg"
              tooltipText="Icon">
  </moz-button>
  <moz-button iconSrc="chrome://global/skin/icons/more.svg"
              tooltipText="Icon Ghost" type="ghost">
  </moz-button>
  <moz-button type="ghost" label="Ghost"></moz-button>
</div>
```

More information about this component including design, writing, and localization guidelines, as well as design assets, can be found on our [Acorn site](https://acorn.firefox.com/latest/components/button/desktop-udQAPeGf).

## Usage guidelines

### When to use

* Use `moz-button` for actions that require user interaction, such as submitting forms or triggering commands.
* Use the `type` property to indicate the button's purpose (e.g., `primary`, `destructive`).
* Use an icon button when the purpose of your button may be easily understood or when space is limited.

### When not to use

* Don't use buttons in place of links.
* Don't use buttons to navigate in-page or to new pages.

### Avoid these common mistakes

* Do not change button types (e.g., `primary` to `destructive`) based on hover or interaction.
* Do not change button text or labels based on user interaction (e.g. don't change the button text on hover).
* Do not create "[Schrödinger’s Buttons](https://docs.google.com/presentation/d/1YZ0S9cl6Gd7H468-86YfnTrGkbkdH-lawRCVAGnWJDs/edit#slide=id.g2ff3e0a4a35_0_0)," i.e. buttons that only become visible when hovered or focused via keyboard.

## Code

The source for `moz-button` can be found under [toolkit/content/widgets/moz-button/](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-button)

## How to use `moz-button`

### Importing the element

Like other custom elements, you should usually be able to rely on `moz-button` getting lazy loaded at the time of first use.
See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

### Setting the `type`

##### Default

If you won't explicitly set the `type` of `moz-button`, it will be set to `default`. In this case you will get a regular button, also known as a "Secondary" button.
A regular button may appear beside a primary button, or alone. Use it for secondary actions (e.g., Cancel) or for multiple actions of equal importance.

```html
<moz-button label="Button"></moz-button>
```
```html story
<moz-button label="Button"></moz-button>
```

##### Primary

Primary buttons represent the most important action on a page and stand out with distinct styling. To avoid visual clutter, only one primary button should be used per page.

```html
<moz-button type="primary" label="Primary"></moz-button>
```
```html story
<moz-button type="primary" label="Primary"></moz-button>
```

##### Destructive

Destructive (Danger) buttons typically appear in dialogs to indicate that the user is about to make a destructive and irreversible action such as deleting or removing a file.

```html
<moz-button type="destructive" label="Destructive"></moz-button>
```
```html story
<moz-button type="destructive" label="Destructive"></moz-button>
```

##### Icon

Icon buttons are used for actions that can be easily represented with a symbol instead of text. They are ideal for saving space, simplifying the interface, or providing quick access to common functions like search, settings, or closing a window. However, they should include accessible tooltips to ensure clarity and usability.
There are two ways of providing an icon/image to `moz-button`:

1) You can either specify `type="icon"` and provide a background-image for the `::part`:

```html
<moz-button type="icon" title="I am an icon button" aria-label="I am an icon button"></moz-button>
```

```css
moz-button::part(button) {
  background-image: url("chrome://global/skin/icons/more.svg");
}
```
2) Or you can provide an icon URI via `iconsrc`, in which case setting `type="icon"` is redundant:

```html
<moz-button iconSrc="chrome://global/skin/icons/more.svg"
            title="I am an icon button"
            aria-label="I am an icon button">
</moz-button>
```

```html story
<moz-button iconSrc="chrome://global/skin/icons/more.svg"
            title="I am an icon button"
            aria-label="I am an icon button">
</moz-button>
```
You can also use `iconsrc` together with `label` to get a button with both icon and text.

```html
<moz-button iconSrc="chrome://global/skin/icons/edit-copy.svg" label="Button"></moz-button>
```

```html story
<moz-button iconSrc="chrome://global/skin/icons/edit-copy.svg" label="Button"></moz-button>
```

##### Ghost

Ghost buttons are used for secondary or less prominent actions. They are ideal for optional actions, alternative choices, or when a clean, subtle look is desired.

```html
<moz-button type="ghost" label="I am a ghost button"></moz-button>
```
```html story
<moz-button type="ghost" label="👻 I am a ghost button"></moz-button>
```

### Setting the `size`

There are 2 options for the `moz-button` size: `default` and `small`. Small buttons are only used for smaller UI surfaces. (e.g., Infobar).

```html
<moz-button label="Default"></moz-button>
<moz-button label="Small" size="small"></moz-button>
```
```html story
<div style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
  <moz-button label="Default"></moz-button>
  <moz-button label="Small" size="small"></moz-button>
</div>
```

### Setting the `disabled` state

In order to disable the `moz-button`, add `disabled=""` or `disabled` to the markup with no value.

```html
<moz-button label="Button" disabled></moz-button>
```
```html story
<moz-button label="Button" disabled></moz-button>
```

### Setting the `accesskey`

`accesskey` defines an keyboard shortcut for the button.

```html
<moz-button label="Button" accesskey="t"></moz-button>
```
```html story
<moz-button label="Button" accesskey="t"></moz-button>
```

### Fluent usage

The `label`, `tooltiptext`, `title`, `aria-label` and `accesskey` attributes of `moz-button` will generally be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes).
The relevant `data-l10n-attrs` are set automatically, so to get things working you just need to supply a `data-l10n-id` as you would with any other element.

For example, the following Fluent messages:

```
moz-button-ftl-id = This is the visible text content!
moz-button-labelled =
  .label = Button
moz-button-titled = Button
  .tooltiptext = Button with title
moz-button-titled-2 =
  .label = Button
  .title = Another button with title
moz-button-aria-labelled =
  .aria-label = Button with aria-label
moz-button-with-accesskey = Button
  .accesskey = t
```

would be used to set text and attributes on the different `moz-button` elements as follows:

```html
<moz-button data-l10n-id="moz-button-ftl-id"></moz-button>
<moz-button data-l10n-id="moz-button-labelled"></moz-button>
<moz-button data-l10n-id="moz-button-titled"></moz-button>
<moz-button data-l10n-id="moz-button-titled-2"></moz-button>
<moz-button data-l10n-id="moz-button-aria-labelled"></moz-button>
<moz-button data-l10n-id="moz-button-with-accesskey"></moz-button>
```
