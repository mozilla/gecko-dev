# Android Test Automation Framework (POM-Based)

## 🚀 Introduction
This framework introduces a modern, scalable approach to Android UI testing using the Page Object Model (POM). It combines Espresso, UIAutomator2, and Jetpack Compose testing under a single unified abstraction, making test code clean, reusable, and easy to maintain.

Tests written with this system read like user journeys, not code. Teams can define pages once and reuse them throughout tests—encouraging ownership, modularity, and clarity across features.

## 🛠️ Getting Started

### Prerequisites
- Android Studio Arctic Fox or newer
- Kotlin 1.9+
- Compose UI Test library
- Enabled Test Orchestrator (optional for retries)

### Writing Your First Test
```kotlin
@Test
fun verifyHomeLoads() {
    on.homePage.navigateToPage()
        .mozVerifyElementsByGroup("requiredForPage")
}
```

### Structure Overview
- `BasePage.kt`: Common actions (clicks, verification, navigation)
- `Selector.kt`: Describes UI elements in a flexible, tool-agnostic way
- `PageContext.kt`: Entry point for test pages via `on.<Page>`
- `NavigationRegistry.kt`: Stores how to move between pages

## 🧪 Test Development Workflow

1. **Define Selectors**
```kotlin
val TITLE = Selector(
    strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
    value = "Welcome",
    description = "Welcome title on Home Page",
    groups = listOf("requiredForPage")
)
```

2. **Create a Page Object**
```kotlin
object HomePage : BasePage() {
    override val pageName = "HomePage"

    override fun mozGetSelectorsByGroup(group: String) = HomePageSelectors.all.filter {
        it.groups.contains(group)
    }
}
```

3. **Add to Context**
```kotlin
val homePage = HomePage
```

4. **Write a Test**
```kotlin
on.homePage.navigateToPage()
    .mozVerifyElementsByGroup("requiredForPage")
```

## 📚 Additional Resources
- 📖 [Test Automation Strategy](./docs/TestAutomationStrategy.md): Roadmap for phases and long-term goals
- 💡 Example tests: See `ui/efficiency/tests/`
- 📎 Diagrams: (Coming soon)

## 👥 Contributing
When adding new pages or selectors:
- Follow the fluent interface pattern
- Group selectors meaningfully (e.g., `"requiredForPage"`, `"toolbar"`)
- Register navigation paths explicitly in each `Page`'s `init` block

## ✅ Best Practices
- Use clear, readable selector descriptions
- Avoid direct interaction with Espresso/UIAutomator in tests
- Build tests in Given/When/Then structure

---
This framework enables powerful, flexible testing—but starts simple. With it, we empower teams to own their features *and* their tests.
