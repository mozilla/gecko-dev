// Uses the debugger interface to inject members into new global objects (like `window`)
Components.utils.import('resource://gre/modules/jsdebugger.jsm');
addDebuggerToGlobal(globalThis);

const dbg = new Debugger();
dbg.onNewGlobalObject = g => {
	g.setProperty('__IS_RECORD_REPLAY_RUNTIME__', true);
};
