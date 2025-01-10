# MozPageNav

`moz-page-nav` is a grouping of navigation buttons that is displayed at the page level,
intended to change the selected view, provide a heading, and have links to external resources.

```html story
<moz-page-nav heading="This is a nav" style={{ '--page-nav-margin-top': 0, '--page-nav-margin-bottom': 0, height: '275px' }}>
  <moz-page-nav-button
    view="view-one"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
  >
    <p style={{ margin: 0 }}>Test 1</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    view="view-two"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
  >
    <p style={{ margin: 0 }}>Test 2</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    view="view-three"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
  >
    <p style={{ margin: 0 }}>Test 3</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    support-page="test"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
    slot="secondary-nav"
  >
    <p style={{ margin: 0 }}>Support Link</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    href="https://www.example.com"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
    slot="secondary-nav"
  >
   <p style={{ margin: 0 }}>External Link</p>
  </moz-page-nav-button>
</moz-page-nav>
```

## When to use

* Use moz-page-nav for single-page navigation to switch between different views.
* moz-page-nav also supports footer buttons for external and support links
* This component is intended to be used in about: pages such as about:firefoxview, about:preferences, about:addons, about:debugging, etc.

## When not to use

* If you need a navigation menu that does not switch between views within a single page

## Code

The source for `moz-page-nav` and `moz-page-nav-button` can be found under
[toolkit/content/widgets/moz-page-nav](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-page-nav).
You can find an examples of `moz-page-nav` in use in the Firefox codebase in
[about:firefoxview](https://searchfox.org/mozilla-central/rev/9783996dbd86f999cab50ea426079a7b10f28a2f/browser/components/firefoxview/firefoxview.html#53-88).

`moz-page-nav` can be imported into `.html`/`.xhtml` files:

```html
<script type="module" src="chrome://global/content/elements/moz-page-nav.mjs"></script>
```

And used as follows:

```html
<moz-page-nav heading="This is a nav">
  <moz-page-nav-button
    data-l10n-id="id-1"
    view="A name for the first view"
    iconSrc="A url for the icon for the first navigation button">
  </moz-page-nav-button>
  <moz-page-nav-button
    data-l10n-id="id-2"
    view="A name for the second view"
    iconSrc="A url for the icon for the second navigation button">
  </moz-page-nav-button>
  <moz-page-nav-button
    data-l10n-id="id-3"
    view="A name for the third view"
    iconSrc="A url for the icon for the third navigation button">
  </moz-page-nav-button>

  <!-- Footer Links -->

  <!-- Support Link -->
  <moz-page-nav-button
    support-page="A name for a support link"
    iconSrc="A url for the icon for the third navigation button"
    slot="secondary-nav">
  </moz-page-nav-button>

  <!-- External Link -->
  <moz-page-nav-button
    href="A url for an external link"
    iconSrc="A url for the icon for the third navigation button"
    slot="secondary-nav">
  </moz-page-nav-button>
</moz-page-nav>
```

### Iconless variant

The `iconSrc` property of `moz-page-nav-button` is optional. This is intended to support more internal facing `about:` pages where the addition of icons is not necessary. In these cases, the code will look a bit simpler:

```html
<moz-page-nav heading="This is a nav">
  <moz-page-nav-button
    data-l10n-id="id-1"
    view="A name for the first view">
  </moz-page-nav-button>
  <moz-page-nav-button
    data-l10n-id="id-2"
    view="A name for the second view">
  </moz-page-nav-button>
  <moz-page-nav-button
    data-l10n-id="id-3"
    view="A name for the third view">
  </moz-page-nav-button>
</moz-page-nav>
```

And will render like this:

```html story
<moz-page-nav heading="This is a nav" style={{ '--page-nav-margin-top': 0, '--page-nav-margin-bottom': 0, height: '150px' }}>
  <moz-page-nav-button view="A name for the first view">
    <p style={{ margin: 0 }}>First iconless button</p>
  </moz-page-nav-button>
  <moz-page-nav-button view="A name for the second view">
    <p style={{ margin: 0 }}>Second iconless button</p>
  </moz-page-nav-button>
  <moz-page-nav-button view="A name for the third view">
    <p style={{ margin: 0 }}>Third iconless button</p>
  </moz-page-nav-button>
</moz-page-nav>
```
Please note that `moz-page-nav` does not currently support mixing `moz-page-nav-button`s with icons and iconless `moz-page-nav-button`s. To avoid display issue your nav should pick one type for primary and secondary nav buttons and stick with it.

### Fluent usage

Generally the `heading` property of
`moz-page-nav` will be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes).
To get this working you will need to format your Fluent message like this:

```
with-heading =
  .heading = Heading text goes here
```
The `data-l10n-attrs` will be set up automatically via `MozLitElement`, so you can just specify `data-l10n-id` on your moz-page-nav as you would with any other markup:

 ```html
 <moz-page-nav data-l10n-id="with-heading"></moz-page-nav>
 ```

You also need to specify a `data-l10n-id` for each `moz-page-nav-button`:

```html
<moz-page-nav-button data-l10n-id="with-button-text"></moz-page-nav-button>
```

```
with-button-text = button text goes here
```
