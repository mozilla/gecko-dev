import figma, { html } from "@figma/code-connect/html"

// Desktop v3 (newest)
figma.connect(
  "https://www.figma.com/design/3WoKOSGtaSjhUHKldHCXbc/Desktop-v3?node-id=32-996&m=dev",
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
      checkboxProps: figma.nestedProps("Checkbox", {
        checked: figma.boolean("Checked"),
        disabled: figma.enum("State", { Disabled: true}),
      }),
    },
    example: (props) => html`
    <moz-checkbox
      checked=${props.checkboxProps.checked}
      disabled=${props.checkboxProps.disabled}
      description=${props.labelProps.description}
      label=${props.labelProps.label}
      support-page=${props.labelProps.supportPage}
      iconcrc=${props.labelProps.iconSrc}
    ></moz-checkbox>
    `,
  },
)

// Desktop Components (deprecated)
figma.connect(
  "https://www.figma.com/design/2ruSnPauajQGprFy6K333u/Desktop-Components?node-id=800-12337&m=dev",
  {
    props: {
      checked: figma.boolean("Checked"),
      description: figma.boolean("Description", { true: "Description to edit" }),
      disabled: figma.boolean("Disabled"),
      label: figma.textContent("✏️ Label")
    },
    example: (props) => html`
    <moz-checkbox
      checked=${props.checked}
      description=${props.description}
      disabled=${props.disabled}
      label=${props.label}
    ></moz-checkbox>
    `
  },
)
