// There's a number of properties added by MozXULElement but for our purposes we
// don't need them. We do this merely to accurately describe what MozSearchbar
// extends.
declare class MozXULElement extends XULElement {}

// Largely just here to fix SearchUIUtils errors. This should be removed once
// searchbar becomes a exportable module.
declare class MozSearchbar extends MozXULElement {
  select(): void;
}
