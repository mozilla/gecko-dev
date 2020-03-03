/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module defines the routines used when updating the graphics shown by a
// middleman process tab. Middleman processes have their own window/document
// which are connected to the compositor in the UI process in the usual way.
// We need to update the contents of the document to draw the raw graphics data
// provided by the child process.

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const CC = Components.Constructor;

// Create a sandbox with the resources we need. require() doesn't work here.
const sandbox = Cu.Sandbox(
  CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")()
);
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
    "addDebuggerToGlobal(this);",
  sandbox
);

// Windows in the middleman process are initially set up as about:blank pages.
// This method fills them in with a canvas filling the tab.
function setupContents(window) {
  // The middlemanCanvas element fills the tab's contents.
  const canvas = (window.middlemanCanvas = window.document.createElement(
    "canvas"
  ));
  canvas.style.position = "absolute";
  window.document.body.style.margin = "0px";
  window.document.body.prepend(canvas);
  window.document.setSuppressedEventListener(ev => mouseEventListener(window, ev));
}

function getCanvas(window) {
  if (!window.middlemanCanvas) {
    setupContents(window);
  }
  return window.middlemanCanvas;
}

function drawCursor(cx, x, y) {
  const scale = 3;
  const path = new Path2D(`
M ${x} ${y}
V ${y+10*scale}
L ${x+2*scale} ${y+8*scale}
L ${x+4*scale} ${y+13*scale}
L ${x+5.5*scale} ${y+12.6*scale}
L ${x+3.5*scale} ${y+7.6*scale}
L ${x+6.5*scale} ${y+7.8*scale}
Z
`);
  cx.fillStyle = "black";
  cx.fill(path);
  cx.strokeStyle = "white";
  cx.lineWidth = 1;
  cx.stroke(path);
}

function drawClick(cx, x, y) {
  cx.strokeStyle = "black";
  cx.lineWidth = 3;
  cx.beginPath();
  cx.arc(x, y, 50, 0, 2 * Math.PI);
  cx.stroke();
}

function drawSmallWarning(cx, canvas) {
  cx.lineWidth = 2;
  cx.strokeStyle = "black";
  cx.fillStyle = "red";
  cx.beginPath();
  cx.arc(canvas.width - 35, 35, 20, 0, 2 * Math.PI);
  cx.fill();
  cx.stroke();

  /*
  cx.fillStyle = "black";
  cx.font = "35px sans-serif";
  cx.fillText("!", canvas.width - 40, 45);
  */

  cx.lineWidth = 5;
  cx.beginPath();
  cx.moveTo(canvas.width - 35, 40);
  cx.lineTo(canvas.width - 35, 20);
  cx.stroke();

  cx.fillStyle = "black";
  cx.beginPath();
  cx.arc(canvas.width - 35, 46, 3, 0, 2 * Math.PI);
  cx.fill();
}

function drawFullWarning(cx, canvas) {
  cx.lineWidth = 2;
  cx.strokeStyle = "black";
  cx.fillStyle = "red";
  cx.fillRect(canvas.width - 340, 15, 325, 40);
  cx.strokeRect(canvas.width - 340, 15, 325, 40);

  cx.fillStyle = "black";
  cx.font = "28px sans-serif";
  cx.fillText("Graphics might be wrong", canvas.width - 330, 45, 305);
}

function mouseEventListener(window, event) {
  if (window.lastUpdate && !window.lastUpdate.warning) {
    return;
  }

  const canvas = getCanvas(window);

  if (event.type == "mousemove") {
    // Get coordinates relative to the canvas origin.
    const x = event.clientX * window.devicePixelRatio;
    const y = event.clientY * window.devicePixelRatio;

    if (window.mouseHoveringOverWarning) {
      if (x < canvas.width - 340 ||
          y < 15 ||
          x > canvas.width - 15 ||
          y > 55) {
        Services.cpmm.sendAsyncMessage("RecordReplayHidePointer");
        window.mouseHoveringOverWarning = false;
        refreshCanvas(window);
      }
    } else {
      const distanceX = x - (canvas.width - 35);
      const distanceY = y - 35;
      const distance = Math.sqrt(distanceX * distanceX + distanceY * distanceY);
      if (distance < 20) {
        Services.cpmm.sendAsyncMessage("RecordReplayShowPointer");
        window.mouseHoveringOverWarning = true;
        refreshCanvas(window);
      }
    }
  } else if (event.type == "mouseup") {
    if (window.mouseHoveringOverWarning) {
      Services.cpmm.sendAsyncMessage("RecordReplayWarningClicked");
      window.mouseHoveringOverWarning = false;
      refreshCanvas(window);
    }
  }
}

function updateWindowCanvas(window, buffer, width, height,
                            cursorX, cursorY, clickX, clickY, warning) {
  window.lastUpdate = {
    buffer, width, height, cursorX, cursorY, clickX, clickY, warning
  };

  // Make sure the window has a canvas filling the screen.
  const canvas = getCanvas(window);

  canvas.width = width;
  canvas.height = height;

  // If there is a scale for this window, then the graphics will already have
  // been scaled in the child process. To avoid scaling the graphics twice,
  // transform the canvas to undo the scaling.
  const scale = window.devicePixelRatio;
  if (scale != 1) {
    canvas.style.transform = `
      scale(${1 / scale})
      translate(-${width / scale}px, -${height / scale}px)
    `;
  }

  const cx = canvas.getContext("2d");

  const graphicsData = new Uint8Array(buffer);
  const imageData = cx.getImageData(0, 0, width, height);
  imageData.data.set(graphicsData);
  cx.putImageData(imageData, 0, 0);

  if (cursorX >= 0 && cursorY >= 0) {
    drawCursor(cx, cursorX, cursorY);
  }

  if (clickX >= 0 && clickY >= 0) {
    drawClick(cx, clickX, clickY);
  }

  if (warning) {
    if (window.mouseHoveringOverWarning) {
      drawFullWarning(cx, canvas);
    } else {
      drawSmallWarning(cx, canvas);
    }
  }
}

function refreshCanvas(window) {
  const {
    buffer, width, height, cursorX, cursorY, clickX, clickY, warning
  } = window.lastUpdate;

  updateWindowCanvas(window, buffer, width, height,
                     cursorX, cursorY, clickX, clickY, warning);
}

function clearWindowCanvas(window, message) {
  const canvas = getCanvas(window);

  const cx = canvas.getContext("2d");
  cx.clearRect(0, 0, canvas.width, canvas.height);

  if (message) {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;

    const scale = window.devicePixelRatio;
    if (scale != 1) {
      canvas.style.transform = `scale(${1 / scale})`;
    }

    cx.font = `${25 * scale}px sans-serif`;
    const messageWidth = cx.measureText(message).width;
    cx.fillText(message, (canvas.width - messageWidth) / 2, canvas.height / 2);
  }
}

// Entry point for when we have some new graphics data from the child process
// to draw.
// eslint-disable-next-line no-unused-vars
function UpdateCanvas(buffer, width, height,
                      cursorX, cursorY, clickX, clickY, warning) {
  try {
    // Paint to all windows we can find. Hopefully there is only one.
    for (const window of Services.ww.getWindowEnumerator()) {
      updateWindowCanvas(window, buffer, width, height,
                         cursorX, cursorY, clickX, clickY, warning);
    }
  } catch (e) {
    console.error(`Middleman Graphics UpdateCanvas Exception: ${e}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
function ClearCanvas(message) {
  try {
    for (const window of Services.ww.getWindowEnumerator()) {
      clearWindowCanvas(window, message);
    }
  } catch (e) {
    console.error(`Middleman Graphics ClearCanvas Exception: ${e}\n`);
  }
}

function RestoreSuppressedEventListener() {
  try {
    for (const window of Services.ww.getWindowEnumerator()) {
      window.document.setSuppressedEventListener(ev => mouseEventListener(window, ev));
    }
  } catch (e) {
    console.error(`Middleman Graphics RestoreSuppressedEventListener Exception: ${e}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = ["UpdateCanvas", "ClearCanvas", "RestoreSuppressedEventListener"];
