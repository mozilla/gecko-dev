import figma, { html } from "@figma/code-connect/html";

figma.connect(
  "https://www.figma.com/design/3WoKOSGtaSjhUHKldHCXbc/Desktop-Components-3?node-id=479-5966&m=dev",
  {
    props: {
      label: figma.string("Label"),
      iconSrc: figma.boolean("Show icon", {
        true: "chrome://example.svg",
      }),
      description: figma.boolean("Show description", {
        true: figma.string("Description"),
      }),
    },
    example: props =>
      html`<moz-box-link
        label=${props.label}
        description=${props.description}
        iconsrc=${props.iconSrc}
      ></moz-box-link>`,
  }
);
