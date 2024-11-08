# MozLabel

`moz-label` is an extension of the built-in `HTMLLabelElement` that provides accesskey styling and formatting as well as some click handling logic.

```html story
<label is="moz-label" accesskey="c" for="check" style={{ display: "inline-block" }}>
    This is a label with an accesskey:
</label>
<input id="check" type="checkbox" defaultChecked style={{ display: "inline-block" }} />
```

Accesskey underlining is enabled by default on Windows and Linux. It is also enabled in Storybook on Mac for demonstrative purposes, but is usually controlled by the `ui.key.menuAccessKey` preference.
