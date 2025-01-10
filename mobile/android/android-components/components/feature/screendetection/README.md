# [Android Components](../../../README.md) > Feature > ScreenDetection

Feature implementation for detecting screen recordings and screenshots.

## Usage

To use the ScreenDetectionFeature, you need to initialize it in your Activity.
Here is an example of how to do it in an Activity:

    ```kotlin
    class MyActivity : AppCompatActivity() {
        private lateinit var screenDetectionFeature: ScreenDetectionFeature

        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)
            setContentView(R.layout.activity_main)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                // init feature with default parameters.
                screenDetectionFeature = ScreenDetectionFeature(this)
                lifecycle.addObserver(screenDetectionFeature)
            }
    }
    ```
### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-screendetection:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
