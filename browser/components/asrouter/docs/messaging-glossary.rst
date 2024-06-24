=========================
Messaging System Glossary
=========================

.. glossary::
    :sorted:

    Advanced Targeting
        Advanced Targeting uses custom criteria to determine user eligibility for messages or experiments in Firefox. Unlike regular targeting, which uses simpler, predefined criteria, advanced targeting leverages custom JEXL (JavaScript Expression Language) expressions for precise segmentation based on user attributes and conditions. The resulting user segments are often referred to as `custom audiences <#term-Custom-Audiences>`_.

        `Learn more <https://experimenter.info/targeting/advanced-targeting>`_

    ASRouter (ASR)
        ASRouter (Activity Stream Router) is the component within Firefox responsible for delivering various types of messages to users based on predefined actions and triggers. It determines when and how messages should be displayed, ensuring relevant content is shown at appropriate times.

        `Learn more <index.html>`_

    Custom Audiences
        Custom Audiences are specific user groups defined by advanced targeting. Custom audiences help to precisely segment users for targeted experiments and messages.

        `Learn more <https://experimenter.info/workflow/implementing/custom-audiences>`_

    Desktop Messaging Surfaces
        Desktop Messaging Surfaces refer to surfaces (i.e. Spotlight, feature callouts, CFR, infobar) available in the messaging system that can be used to show messages at various locations (i.e. newtab, browser chrome) within the Firefox browser.

        `Learn more <https://experimenter.info/messaging/desktop-messaging-surfaces>`_

    Daily Active Users (DAU)
        Daily Active Users (DAU) is a metric that measures the number of unique users who engage with a product or service within a 24-hour period. Monitoring DAU helps teams understand user behavior and the impact of changes or experiments.

    Dismissal
        Dismissal occurs when a user closes or dismisses a message. Tracking dismissals helps in understanding user preferences and the effectiveness of messages.

        `Learn more <https://experimenter.info/messaging/telemetry>`_

    Engagement
        Engagement refers to user interactions with a message, such as clicks, dismissals, or other forms of interaction. High engagement typically indicates that the message is resonating with users.

    Experiment
        Experiments are controlled tests run within Firefox to evaluate the impact of different messages or features. The Messaging System can be used to deliver and measure the results of these experiments.

        `Learn more <https://experimenter.info/messaging/experiments-and-user-messaging>`_

    Experiment Brief
        An experiment brief is a document typically created by a Product Manager outlining the hypothesis, design, content, and audience sizing for an experiment. It serves as a guide for engineers to implement and configure experiments accurately.

    Experimenter
        Experimenter is a platform used for running A/B experiments and feature rollouts. It offers tools for configuration, analysis, and client libraries to manage both `experiments <#term-Experiment>`_ and `rollouts <#term-Rollout>`_.

        `Learn more <https://experimenter.info/>`_

    Exposure
        Exposure refers to a telemetry event indicating that a message was actually displayed to the user.

        `Learn more <https://experimenter.info/messaging/telemetry>`_

    Feature Definition
        Feature Definition involves detailing the specific aspects of a feature (experimentation surface), including its purpose, expected outcomes, and how it integrates with the existing system.

        `Learn more <https://experimenter.info/feature-definition>`_

    First Run
        First Run refers to the initial launch of Firefox after installation or upgrade. During this phase, new users are guided through an onboarding flow.

        `Learn more <first-run.html>`_

    First Run Experiment
        First Run Experiments are experiments that make changes to the onboarding flow presented to users on their first installation of Firefox. These experiments help in gathering data from new users and improving the onboarding experience. This is currently only available for use on Windows.

        `Learn more <https://experimenter.info/workflow/implementing/desktop-onboarding/onboarding-feature-desktop>`_

    First Startup
        First startup is a Windows-only technical process that occurs before the first application window of Firefox appears. It is invoked by the Windows installer to perform essential initializations and ensures that all necessary background tasks are completed before the user interacts with the browser.

        `Learn more <https://firefox-source-docs.mozilla.org/toolkit/modules/toolkit_modules/FirstStartup.html>`_

    Forced Enrollment
        Forced Enrollment is a method used in experiments to ensure that specific users are enrolled into an experiment or receive a particular message. Forced enrollment ignores `advanced targeting <#term-Advanced-Targeting>`_ used for `natural enrollment <#term-Natural-Enrollment>`_. It is often used during testing phases to verify the correct functioning and impact of messages or features before broader deployment. Forced Enrollment can be initiated through specific URLs or browser configurations.

    Frequency Capping
        Frequency capping limits the number of times a message is shown to a user to avoid overexposure and annoyance. This ensures a better user experience.

        `Learn more <https://experimenter.info/messaging/frequency-cap>`_

    Glean
        Glean is Mozilla’s telemetry and product analytics solution, designed to collect, validate, and store data across products. Glean provides a more modern, flexible framework than traditional `Firefox telemetry <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/index.html>`_. This includes automatic documentation, stricter data review processes, and enhanced privacy features.

        `Learn more <https://docs.telemetry.mozilla.org/concepts/glean/glean.html>`_

    Groups and Campaigns
        Groups and campaigns refer to organized sets of messages targeted at specific user segments to achieve particular goals, such as promoting a new feature or encouraging user engagement.

        `Learn more <https://experimenter.info/messaging/groups-and-campaigns>`_

    Holdback
        Holdback refers to a control strategy in experiments and feature rollouts where a portion of the user population does not receive a new change. This allows for comparison between those who experience the change and the holdback group who do not, helping to measure the impact of the change on key metrics.

        `Learn more <https://experimenter.info/for-product/#when-shipping-product-changes-a-guide-on-when-to-use-what-option>`_

    Impression
        An impression is recorded each time a message is shown to a user. The specifics of what counts as a single impression can vary depending on the messaging surface.

        `Learn more <https://experimenter.info/messaging/display-logic#impressions>`_

    Message
        A message in the ASRouter system is a piece of content delivered to the user. Messages can vary in format, including text, images, and interactive elements, and are defined by their respective recipes.

        `Learn more <index.html>`_

    Message Provider
        Message Providers are sources of messages for the ASRouter. They can be local JSON files, remote endpoints, or other internal services that supply messages to be displayed.

        `Learn more <debugging-docs.html#how-to-see-all-messages-from-a-provider>`_

    Natural Enrollment
        Natural Enrollment occurs when users are automatically enrolled in an experiment based on predefined advanced targeting criteria without any forced actions. It reflects typical user behavior and provides more accurate data on the experiment's impact as enrolled users meet advanced targeting conditions.

    Nimbus
        Nimbus is a platform used by Mozilla for running experiments and feature rollouts. The Messaging System often interfaces with Nimbus to deliver experimental messages and collect data.

        `Learn more <https://firefox-source-docs.mozilla.org/toolkit/components/nimbus/docs/index.html>`_

    Onboarding
        Onboarding refers to the set of messages and flows designed to guide new and returning users through initial setup and familiarize them with key features of Firefox.

        `Learn more <first-run.html>`_

    Reach
        Reach refers to the potential audience size for a message in an experiment branch, calculated based on the conditions met for message display even if the user is not enrolled in the experiment branch showing the message.

        `Learn more <https://experimenter.info/messaging/telemetry/>`_

    Recipe
        A recipe in the context of ASRouter is a configuration that defines the triggers, targeting criteria, and content for a specific message. Recipes are used to control what messages are shown and when.

        `Learn more <https://experimenter.info/workflow/implementing/desktop-onboarding/onboarding-feature-desktop/#how-do-first-run-experiments-work-on-windows>`_

    Remote Localization
        Remote localization involves updating message content for different languages and regions dynamically, ensuring that messages are relevant and understandable for users worldwide.

        `Learn more <https://experimenter.info/messaging/remote-localization>`_

    Remote Settings
        Remote Settings is a service that allows Mozilla to remotely update and configure various settings within Firefox, including ASRouter messages. It ensures messages can be dynamically updated without requiring a browser update.

        `Learn more <https://firefox-source-docs.mozilla.org/services/settings/>`_

    Rollout
        A Rollout refers to the off-train deployment of a product change to a defined user population. "Off-`train <https://firefox-source-docs.mozilla.org/contributing/pocket-guide-shipping-firefox.html#train-model>`_" means updates are deployed outside of the regular Firefox `release cycle <https://firefox-source-docs.mozilla.org/contributing/pocket-guide-shipping-firefox.html#release-cycle>`_. Unlike experiments that compare control and treatment groups to measure causal impacts, rollouts primarily focus on reducing technical risks and ensuring scalability. Rollouts can be scaled up or down as needed and allow for the immediate availability of changes to a wide audience while monitoring the impact on key metrics.

        `Learn more <https://experimenter.info/for-product/#when-shipping-product-changes-a-guide-on-when-to-use-what-option>`_

    Schemas
        JSON Schema is used to define the structure and validation rules for JSON data used in ASRouter messages. Schemas ensures that messages adhere to the expected format and content requirements.

        `Learn more <https://firefox-source-docs.mozilla.org/toolkit/components/messaging-system/docs/index.html>`_

    Sticky Enrollment
        Sticky Enrollment ensures that once a user is enrolled in an experiment, they remain in that condition for the entire duration, even if the targeting criteria no longer apply. This prevents users from being unenrolled or switched between different branches, providing consistent exposure to the experiment's conditions.

        `Learn more <https://experimenter.info/2022-07#sticky-enrollment>`_

    Targeting
        Targeting refers to the use of specific criteria to determine which messages are shown to which users. This involves using JEXL (JavaScript Expression Language) expressions to evaluate attributes such as user preferences, browser configurations, and behaviors. Targeting ensures that messages are relevant to the user.

        `Learn more <targeting-attributes.html>`_

    Messaging Telemetry
        Messaging Telemetry refers to the collection of data on user interactions with messages, such as impressions, button clicks, and dismissals. This data is used for analyzing the effectiveness of messages and guiding future improvements. Messaging Telemetry should not be confused with the more generic `Firefox Telemetry <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/index.html>`_.

        `Learn more <https://experimenter.info/messaging/telemetry>`_

    Treatment Branch
        A Treatment Branch in an experiment refers to a specific variant or condition being tested. Each treatment branch represents a different version of the message or feature being evaluated. This allows for comparison to determine which variant performs best.

    Trigger
        Triggers are events or conditions that activate the delivery of a message by the Messaging System. They dictate when a message will try to appear for a user. These can include user actions, time-based conditions, or specific states within the browser.

        `Learn more <https://experimenter.info/messaging/display-logic/#triggers>`_
