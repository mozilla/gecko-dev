import figma, { html } from "@figma/code-connect/html";

// Desktop v3 (newest)
figma.connect(
  "https://www.figma.com/design/3WoKOSGtaSjhUHKldHCXbc/Desktop-v3?node-id=243-1299&m=dev",
  {
    props: {
      heading: figma.boolean("Show heading", {
        true: figma.string("Heading"),
        false: undefined,
      }),
      message: figma.boolean("Show message", {
        true: figma.string("Message"),
        false: undefined,
      }),
      type: figma.enum("Type", {
        Success: "success",
        Warning: "warning",
        Critical: "critical",
        Information: "information",
      }),
      supportPage: figma.boolean("Show support link", {
        true: "sumo-slug",
      }),
      action: figma.boolean("Show action", {
        true: html`<moz-button slot="actions" label="Label"></moz-button
          ><moz-button slot="actions" label="Label"></moz-button>`,
        false: undefined,
      }),
      buttonGroupProps: figma.nestedProps("Button group", {
        additionalAction: figma.boolean("Show 3rd button", {
          true: html`<moz-button slot="actions" label="Label"></moz-button>`,
          false: undefined,
        }),
      }),
    },
    example: props =>
      html`<moz-message-bar
        type=${props.type}
        message=${props.message}
        heading=${props.heading}
        support-page=${props.supportPage}
        >${props.action}${props.buttonGroupProps
          .additionalAction}</moz-message-bar
      >`,
  }
);

// Desktop Components (deprecated)
figma.connect(
  "https://www.figma.com/design/2ruSnPauajQGprFy6K333u/Desktop-Components?node-id=8922-12259&m=dev",
  {
    props: {
      message: figma.boolean("Message", {
        true: "Your message here",
        false: undefined,
      }),
      action: figma.boolean("Action", {
        true: html`<moz-button slot="actions" label="Label"></moz-button>`,
        false: undefined,
      }),
      supportPage: figma.boolean("hasLearnMore", {
        true: "sumo-slug",
        false: undefined,
      }),
      heading: figma.boolean("hasHeading", {
        true: figma.string("Heading"),
        false: undefined,
      }),
      type: figma.enum("Type", {
        Success: "success",
        Warning: "warning",
        Critical: "critical",
      }),
      theme: figma.enum("Theme", {
        Light: "light",
        Dark: "dark",
      }),
    },
    example: props =>
      html`<moz-message-bar
        type=${props.type}
        message=${props.message}
        heading=${props.heading}
        support-page=${props.supportPage}
        >${props.action}</moz-message-bar
      >`,
  }
);
