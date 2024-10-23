# Console Tool Architecture

The Console panel is responsible for rendering all logs coming from the current page.

## Architecture

Internal architecture of the Console panel (the client side) is described
on the following diagram.

```{mermaid}
flowchart TB
DevTools["DevTools<br/>[client/framework/devtools.js]"]
-- "openBrowserConsole() or<br/>toggleBrowserConsole()"
--> BrowserConsoleManager["BrowserConsoleManager<br/>[browser-console-manager.js]"]
-- "{_browserConsole}"
--> BrowserConsole["BrowserConsole<br/>[browser-console.js]"];
WebConsolePanel["WebConsolePanel<br/>[panel.js]"] -- "{hud}" --> WebConsole["WebConsole[webconsole.js]"]
-- "{ui}" --> WebConsoleUI["WebConsoleUI<br/>[webconsole-ui.js]"]
-- "{wrapper}" --> WebConsoleWrapper["WebConsoleWrapper<br/>[webconsole-wrapper.js]"]
-- "ReactDOM.render" --> App;
BrowserConsole -- extends --> WebConsole;
```


## Components

The Console panel UI is built on top of [React](../frontend/react.md). It defines set of React components in `components` directory
The React architecture is described on the following diagram.

```{mermaid}
flowchart TB
subgraph React Components
direction TB
App --> SideBar & NotificationBox & ConfirmDialog & FilterBar & ReverseSearchInput & ConsoleOutput & EditorToolbar & JSTerm
FilterBar --> FilterButton & ConsoleSettings
ConsoleOutput --> MessageContainer --> Message
Message --> MessageIndent & MessageIcon & CollapseButton & GripMessageBody & ConsoleTable & MessageRepeat
end
subgraph Reps
direction TB
ObjectInspector["ObjectInspector"] --> ObjectIspectorItem --> Rep["Rep<br/>[client/shared/components/reps/rep.js"] --> Rep
end
SideBar & ConsoleTable & GripMessageBody --> Reps
Message --> Frame["Frame<br/>[client/shared/components/Frame.js]"] & SmartTrace["SmartTrace<br/>[client/shared/components/SmartTrace.js]"] & TabboxPanel["TabboxPanel<br/>[client/netmonitor/src/components/TabboxPanel.js]"]
JSTerm -- editor --> Editor["Editor<br/>[client/shared/sourceeditor/editor.js]"]
```

There are several external components we use from the WebConsole:
- ObjectInspector/Rep: Used to display a variable in the console output and handle expanding the variable when it's not a primitive.
- Frame: Used to display the location of messages.
- SmartTrace: Used to display the stack trace of messages and errors
- TabboxPanel: Used to render a network message detail. This is directly using the component from the Netmonitor so we are consistent in how we display a request internals.

## Actions

The Console panel implements a set of actions divided into several groups.

- **Filters** Actions related to content filtering.
- **Messages** Actions related to list of messages rendered in the panel.
- **UI** Actions related to the UI state.

### State

The Console panel manages the app state via [Redux](../frontend/redux.md).

There are following reducers defining the panel state:

- `reducers/filters.js` state for panel filters. These filters can be set from within the panel's toolbar (e.g. error, info, log, css, etc.)
- `reducers/messages.js` state of all messages rendered within the panel.
- `reducers/prefs.js` Preferences associated with the Console panel (e.g. logLimit)
- `reducers/ui.js` UI related state (sometimes also called a presentation state). This state contains state of the filter bar (visible/hidden), state of the time-stamp (visible/hidden), etc.
