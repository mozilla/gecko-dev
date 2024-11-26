# Android Versioning

Firefox Android versioning uses the same `version.txt` to define the version for Android Components, Focus, and Fenix. The versioning follows the same process as Firefox Desktop. The following formatting corresponds to release branches
on the release track.

## Format

XXX.0 (where XXX is defined as the current desktop release #)

*Please note: the initial release is `major.minor`, but the following releases will be `major.minor.patch`*

* The first will be directly tied to the associated desktop release

### Example
|                                | Desktop                       | Android                        |
| ------------------------------ | ----------------------------- | ------------------------------ |
| Fx XXX initial release         | XXX.0                         | XXX.0                          |
| Desktop + Android dot release  | XXX.0.1                       | XXX.0.1                        |
| Android-only dot release       | <sub>XXX.0.2 is skipped</sub> | XXX.0.2                        |
| Desktop-only dot release       | XXX.0.3                       | <sub>XXX.0.3 is skipped </sub> |
| Android-only dot release       |<sub>XXX.0.4 is skipped</sub>  | XXX.0.4                        |
| Desktop + Android dot release  | XXX.0.5                       | XXX.0.5                        |
