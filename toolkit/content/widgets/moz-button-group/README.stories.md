# MozButtonGroup

`moz-button-group` displays multiple related buttons.

```html story
<moz-card style={{ width: "400px" }}>
  <p>The button group is below. Card for emphasis.</p>
  <moz-button-group>
    <moz-button
      type="primary"
      label="OK"
    ></moz-button>
    <moz-button label="Cancel"></moz-button>
  </moz-button-group>
</moz-card>
```

## Usage guidelines

### When to use

* Use `moz-button-group` to group together actions that have a relationship.

## Code

The source for `moz-button-group` can be found under [toolkit/content/widgets/moz-button-group/](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-button-group)

## How to use `moz-button-group`

### Importing the element

Like other custom elements, you should usually be able to rely on `moz-button-group` getting lazy loaded at the time of first use.
See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

### A grouping of buttons

Primary button order will be set automatically based on `class="primary"`, `type="submit"` or `autofocus` attributes. Set `slot="primary"` on a primary button that does not have primary styling to set its position. The primary button's position varies by platform: on Windows, it appears on the left, while on Linux and macOS, it appears on the right.

1. Setting button order automatically:

```html
<moz-button-group>
  <moz-button label="Secondary 1"></moz-button>
  <moz-button type="primary" label="Primary"></moz-button>
  <moz-button label="Secondary 2"></moz-button>
</moz-button-group>
```

```html story
<moz-card style={{ width: "450px" }}>
  <moz-button-group>
    <moz-button label="Secondary 1"></moz-button>
    <moz-button type="primary" label="Primary"></moz-button>
    <moz-button label="Secondary 2"></moz-button>
  </moz-button-group>
</moz-card>
```

2. Setting button order with `slot="primary"`

```html
<moz-button-group>
  <moz-button label="Secondary 1"></moz-button>
  <moz-button slot="primary" label="Primary slot"></moz-button>
  <moz-button label="Secondary 2"></moz-button>
</moz-button-group>
```

```html story
<moz-card style={{ width: "450px" }}>
  <moz-button-group>
    <moz-button label="Secondary 1"></moz-button>
    <moz-button slot="primary" label="Primary slot"></moz-button>
    <moz-button label="Secondary 2"></moz-button>
  </moz-button-group>
</moz-card>
```
