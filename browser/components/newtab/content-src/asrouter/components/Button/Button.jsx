import React from "react";

const ALLOWED_STYLE_TAGS = ["color", "backgroundColor"];

export const Button = props => {
  const style = {};

  // Add allowed style tags from props, e.g. props.color becomes style={color: props.color}
  for (const tag of ALLOWED_STYLE_TAGS) {
    if (typeof props[tag] !== "undefined") {
      style[tag] = props[tag];
    }
  }
  // remove border if bg is set to something custom
  if (style.backgroundColor) {
    style.border = "0";
  }

  return (<button onClick={props.onClick}
    className={props.className || "ASRouterButton secondary"}
    style={style}>
    {props.children}
  </button>);
};
