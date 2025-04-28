import figma, { html } from "@figma/code-connect/html"

// Desktop V3 (newest)
figma.connect(
  "https://www.figma.com/design/3WoKOSGtaSjhUHKldHCXbc/Desktop-v3?node-id=32-1001&t=2gbnIP6ylasS3WuO-4",
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
      toggleProps: figma.nestedProps("Toggle Switch", {
        disabled: figma.enum("State", {
          Disabled: true,
        }),
        pressed: figma.boolean("Checked"),
      }),
    },
    example: (props) => html`\
<moz-toggle
      label=${props.labelProps.label}
      description=${props.labelProps.description}
      iconsrc=${props.labelProps.iconSrc}
      support-page=${props.labelProps.supportPage}
      disabled=${props.toggleProps.disabled}
      pressed=${props.toggleProps.pressed}
></moz-toggle>`,
  },
)

// Desktop Components (deprecated)
figma.connect(
  "https://www.figma.com/design/2ruSnPauajQGprFy6K333u/Desktop-Components?node-id=801-7224&m=dev",
  {
    props: {
      description: figma.boolean("Description", {
        true: "Description to edit",
      }),
      disabled: figma.boolean("Disabled"),
      label: "Label to edit",
      toggleProps: figma.nestedProps("Toggle Input", {
        pressed: figma.boolean("Switch"),
      }),
    },
    example: (props) => html`\
<moz-toggle
      label=${props.label}
      description=${props.description}
      disabled=${props.disabled}
      pressed=${props.toggleProps.pressed}
></moz-toggle>`,
  },
)
