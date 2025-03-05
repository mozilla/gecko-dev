# HCM Settings

Several Firefox settings work together to determine how web content and browser chrome are rendered. They can be hard to keep track of! Use the flowcharts below for quick reference.

## Settings that control color usage in browser chrome
- OS HCM:
	- Windows: High Contrast Mode in OS accessibility settings
	- macOS: Increase Contrast in OS accessibility settings
	- Linux: High Contrast Theme in OS accessibility settings
- FF Theme (AKA FF Colorway)
Note: OS HCM settings will only trigger HCM color usage in chrome if a user's FF theme is set to "system auto". If they have a pre-selected colorway or other FF theme (including explicit "Dark" or "Light") they will not see color changes upon enabling OS HCM.

```{mermaid}
flowchart TD
A[Is OS HCM enabeld?]
A -->|Yes| B[Is FF's theme set to System Auto?]
B --> |Yes| C[Use OS HCM colors to render browser chrome]
B -->|No| D[Use FF theme colors to render browser chrome]
A -->|No| D
```

## Settings that control color usage in content
- Colors Dialog (about:preferences > Contrast Control)
- Extensions like Dark Reader, or changes to user.css, may override author specified colors independent of HCM.

```{mermaid}
flowchart TD
A[Which option is selected in 'Contrast Control'?]

A -->|Use platform's contrast settings| B[Is a OS HCM enabled?]
A -->|Off| H
A --->|On| I[Use colors dialog colors to render all content &lpar;HCM&rpar;]

B -->|Yes| C[Use OS colors to render all content &lpar;HCM&rpar;]
B -->|No| H[Use system colors for all unstyled content]
```
