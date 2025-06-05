import figma, { html } from "@figma/code-connect/html";

// Desktop v3 (newest)
figma.connect(
  "https://www.figma.com/design/3WoKOSGtaSjhUHKldHCXbc/Desktop-Components-3?node-id=33-302&m=dev",
  {
    props: {
      labelProps: figma.nestedProps("Label", {
        description: figma.boolean("Show description", {
          true: figma.string("Description"),
        }),
        label: figma.string("Label"),
        supportPage: figma.boolean("Show support link", {
          true: "sumo-slug",
        }),
        iconSrc: figma.boolean("Show icon", {
          true: "chrome://example.svg",
        }),
      }),
      textInputProps: figma.nestedProps("Text input", {
        disabled: figma.enum("State", { Disabled: true }),
        placeholder: figma.boolean("Show placeholder", {
          true: figma.string("Placeholder"),
        }),
      }),
    },
    example: props => html`
      <moz-input-text
        label=${props.labelProps.label}
        description=${props.labelProps.description}
        support-page=${props.labelProps.supportPage}
        iconsrc=${props.labelProps.iconSrc}
        placeholder=${props.textInputProps.placeholder}
        disabled=${props.textInputProps.disabled}
      ></moz-input-text>
    `,
  }
);
