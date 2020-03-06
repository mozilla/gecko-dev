# Bug-1531370


## Pre-requisites

* Mozilla Devtools
* Github

## Setup

> git clone https://github.com/thasitathan/gecko-dev.git

> git checkout bug-1531370

> cd mozilla-central

> ./mach build faster

> ./mach run

## Test Cases

**Dynamically generated elements (Default Developer Tools)**

1. Right click &rarr; inspect element (or F12)
2. Use three dots (or F1) to go to Settings
3. Locate Default Developer Tools
4. Click through each checkbox (Network, Style Editor, etc..) and ensure the boxes toggle
5. Click through each row of the corresponding checkbox (Network, Style Editor, etc..) and ensure the rows do not toggle the check boxes

**Dynamically generated elements (Available Toolbox Buttons)** 

1. Right click &rarr; inspect element (or F12)
2. Use three dots (or F1) to go to Settings
3. Locate Available Toolbox Buttons
4. Click through each checkbox (Pick an.., Select an.., etc..) and ensure the boxes toggle
5. Click through each row of the corresponding checkbox (Pick an.., Select an.., etc..) and ensure the rows do not toggle the check boxes

**Statically generated elements (Inspector)**

1. Right click &rarr; inspect element (or F12)
2. Use three dots (or F1) to go to Settings
3. Locate Inspector
4. Click through each checkbox (Show Browser .., Truncate ...) and ensure the boxes toggle
5. Click through each row of the corresponding checkbox (Show Browser .., Truncate ...) and ensure the rows do not toggle the check boxes

**Statically generated elements (Style Editor)**

1. Right click &rarr; inspect element (or F12)
2. Use three dots (or F1) to go to Settings
3. Locate Style Editor
4. Click through each checkbox (Autocomplete CSS) and ensure the boxes toggle
5. Click through each row of the corresponding checkbox (Autocomplete CSS) and ensure the rows do not toggle the check boxes

**Statically generated elements (Screenshot Behaviour)**

1. Right click &rarr; inspect element (or F12)
2. Use three dots (or F1) to go to Settings
3. Locate Screenshot Behaviour
4. Click through each checkbox (Screenshot .., Play ..) and ensure the boxes toggle
5. Click through each row of the corresponding checkbox (Screenshot .., Play ..) and ensure the rows do not toggle the check boxes

**Statically generated elements (Editor Preferences)**

1. Right click &rarr; inspect element (or F12)
2. Use three dots (or F1) to go to Settings
3. Locate Editor Preferences
4. Click through each checkbox (Detect .., Autoclose .., Indent ..) and ensure the boxes toggle
5. Click through each row of the corresponding checkbox (Detect .., Autoclose .., Indent ..) and ensure the rows do not toggle the check boxes

**Statically generated elements (Advanced settings)**

1. Right click &rarr; inspect element (or F12)
2. Use three dots (or F1) to go to Settings
3. Locate Advanced settings
4. Click through each checkbox (Enable .., Show .., etc ..) and ensure the boxes toggle
5. Click through each row of the corresponding checkbox (Enable .., Show .., etc ..) and ensure the rows do not toggle the check boxes

## Running Test Cases (for test driven development)
- [ ] Dynamically generated elements (Default Developer Tools) &nbsp; &nbsp; ... PASSED
- [ ] Dynamically generated elements (Available Toolbox Buttons) &nbsp; &nbsp; ... PASSED
- [ ] Statically generated elements (Inspector) &nbsp; &nbsp; ... PASSED
- [ ] Statically generated elements (Style Editor) &nbsp; &nbsp; ... PASSED
- [ ] Statically generated elements (Screenshot Behaviour) &nbsp; &nbsp; ... PASSED
- [ ] Statically generated elements (Editor Preferences) &nbsp; &nbsp; ... PASSED
- [ ] Statically generated elements (Advanced settings) &nbsp; &nbsp; ... PASSED
