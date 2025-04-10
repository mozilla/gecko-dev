# MozCard

`moz-card` is a reusable component that can show a set of predetermined content in a container that looks like a "card".

```html story
<moz-card style={{width: "500px"}}>
    Here is my content inside of a card.
</moz-card>
```

## Code

The source for `moz-card` can be found under [toolkit/content/widgets/moz-card/](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-card)

## How to use `moz-card`

### Importing the element

Like other custom elements, you should usually be able to rely on `moz-card` getting lazy-loaded at the time of first use.
See [this documentation](https://firefox-source-docs.mozilla.org/browser/components/storybook/docs/README.reusable-widgets.stories.html#using-new-design-system-components) for more information on using design system custom elements.

### Setting the `heading`

Provide a heading for the moz-card component for emphasis on what the card is about.

```html
<moz-card heading="The heading">The content under the heading</moz-card>
```

```html story
<moz-card
    style={{width: '500px'}}
    heading="The heading"
>The content under the heading</moz-card>
```

You can also set an icon along with the heading by providing an `icon` attribute.


```html
<moz-card heading="The heading" icon="">The content under the heading</moz-card>
```

```html story
<moz-card
    style={{width: '500px'}}
    heading="The heading"
    icon=""
>The content under the heading</moz-card>
```

### Setting the `type`

A type of `accordion` can be provided to moz-card component to generate an accordion-version of the component.

```html
<moz-card
    type="accordion"
    heading="This is an accordion"
>The expanded content in the accordion</moz-card>
```

```html story
<moz-card
    style={{width: '500px'}}
    type="accordion"
    heading="This is an accordion"
>The expanded content in the accordion</moz-card>
```


### Fluent usage

Generally the `heading` property of `moz-card` will be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes). To get this working you will need to format your Fluent message like this:

```
with-heading =
  .heading = Heading text goes here
```
The `data-l10n-attrs` will be set up automatically, so you can just specify `data-l10n-id` on your moz-card as you would with any other markup:

 ```html
 <moz-card data-l10n-id="with-heading"></moz-card>
 ```
