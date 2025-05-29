// Just for the purpose of accessing the browser object, we define the tab.
// This should be removed once the tab is defined globally.
declare class MozTabbrowserTab {
  // Bug 1957641
  linkedBrowser: XULBrowserElement;
}
