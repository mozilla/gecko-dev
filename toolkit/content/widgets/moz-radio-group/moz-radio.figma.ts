import figma, { html } from "@figma/code-connect/html";

// Desktop Components v3 (newest)
figma.connect(
  "https://www.figma.com/design/3WoKOSGtaSjhUHKldHCXbc/Desktop-Components-3?node-id=32-984&m=dev",
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
      radioProps: figma.nestedProps("Radio Button", {
        checked: figma.boolean("Checked"),
        disabled: figma.enum("State", { Disabled: true }),
      }),
    },
    example: props => html`
      <moz-radio
        checked=${props.radioProps.checked}
        disabled=${props.radioProps.disabled}
        description=${props.labelProps.description}
        label=${props.labelProps.label}
        support-page=${props.labelProps.supportPage}
        iconsrc=${props.labelProps.iconSrc}
      ></moz-radio>
    `,
  }
);

// Desktop Components (deprecated)
figma.connect(
  "https://www.figma.com/design/2ruSnPauajQGprFy6K333u/%E2%9A%A0%EF%B8%8F-DEPRECATED---Desktop-Components?node-id=800-14976&m=dev",
  {
    props: {
      checked: figma.boolean("Checked"),
      description: figma.boolean("Description", {
        true: "Description to edit",
      }),
      disabled: figma.boolean("Disabled"),
      label: figma.textContent("✏️ Label"),
    },
    example: props =>
      html` <moz-radio
        checked=${props.checked}
        description=${props.description}
        disabled=${props.disabled}
        label=${props.label}
      ></moz-radio>`,
  }
);
