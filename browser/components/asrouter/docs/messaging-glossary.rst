=========================
Messaging System Glossary
=========================
.. glossary::
    :sorted:

    `about:welcome <https://firefox-source-docs.mozilla.org/browser/components/asrouter/docs/about-welcome.html>`_
        about:welcome is the url for the initial onboarding experience in Firefox, shown to users when they first install the browser. The page guides new users through setup, highlights key features, and can be customized for specific user segments. It is often used in experiments to test new onboarding flows and improve user engagement.


    `Advanced Targeting <https://experimenter.info/targeting/advanced-targeting>`_
        Advanced Targeting uses custom criteria to determine user eligibility for messages or experiments in Firefox, typically in the context of :term:`Nimbus`. Unlike regular targeting, which uses simpler, predefined criteria, advanced targeting leverages custom JEXL (JavaScript Expression Language) expressions for precise segmentation based on user attributes and conditions. The resulting user segments are often referred to as :term:`custom audiences`.

    `ASRouter (ASR) <index.html>`_
        ASRouter (Activity Stream Router) is the component within Firefox responsible for delivering various types of messages to users based on predefined actions and triggers. It determines when and how messages should be displayed, ensuring relevant content is shown at appropriate times.

    Control Branch
        A control branch in an experiment serves as a baseline to compare against the :term:`treatment branches<treatment branch>`. Users in the control branch typically see no message or are exposed to the default version of a message or feature.

    `Custom Audiences <https://experimenter.info/workflow/implementing/custom-audiences>`_
        Custom Audiences are specific user groups defined by advanced targeting. Custom audiences help to precisely segment users for targeted experiments and messages.

    `Desktop Messaging Surfaces <https://experimenter.info/messaging/desktop-messaging-surfaces>`_
        Desktop Messaging Surfaces refer to surfaces (i.e. Spotlight, feature callouts, CFR, infobar) available in the messaging system that can be used to show messages at various locations (i.e. newtab, browser chrome) within the Firefox browser.

    Daily Active Users (DAU)
        Daily Active Users (DAU) is a metric that measures the number of unique users who engage with a product or service within a 24-hour period. Monitoring DAU helps teams understand user behavior and the impact of changes or experiments.

    `Dismissal <https://experimenter.info/messaging/telemetry>`_
        Dismissal occurs when a user closes or dismisses a message. Tracking dismissals helps in understanding user preferences and the effectiveness of messages.

    Engagement
        Engagement refers to user interactions with a message, such as clicks, dismissals, or other forms of interaction. High engagement typically indicates that the message is resonating with users.

    `Experiment <https://experimenter.info/messaging/experiments-and-user-messaging>`_
        Experiments are controlled tests run within Firefox to evaluate the impact of different messages or features. The Messaging System can be used to deliver and measure the results of these experiments, typically using :term:`Nimbus`.

    Experiment Brief
        An experiment brief is a document typically created by a Product Manager outlining the hypothesis, design, content, and audience sizing for an experiment. It serves as a guide for engineers to implement and configure experiments accurately.

    `Experimenter <https://experimenter.info/>`_
        Experimenter is a :term:`Nimbus` user interface used for running A/B experiments and feature rollouts. It offers tools for configuration, analysis, and client libraries to manage both :term:`experiments<experiment>` and :term:`rollouts<rollout>`.

    `Experiment Localization System <https://experimenter.info/localization-process/>`_
        This system allows for the localization of experiment content outside of the standard Firefox release cycle. Translated strings are provided in JSON format and can be added directly to the Localization field in :term:`Experimenter`.

    `Exposure <https://experimenter.info/messaging/telemetry>`_
        Exposure refers to a telemetry event indicating that a message was actually displayed to the user or would have been displayed in the case of no-message control groups. Exposure differs from impression telemetry in that :term:`impressions<impression>` only capture messages that are actually displayed to the user.

    `Feature Definition <https://experimenter.info/feature-definition>`_
        Feature Definition involves detailing the specific aspects of a feature (experimentation surface), including its purpose, expected outcomes, and how it integrates with the existing system.

    `First Run <first-run.html>`_
        First Run refers to the initial launch of Firefox after installation or upgrade. During this phase, new users are guided through an onboarding flow in :term:`about:welcome`.

    `First Run Experiment <https://experimenter.info/workflow/implementing/desktop-onboarding/onboarding-feature-desktop>`_
        First Run Experiments are experiments that make changes to the onboarding flow presented to users on their first installation of Firefox. These experiments help in gathering data from new users and improving the onboarding experience. This is currently only available for use on Windows.

    `First Startup <https://firefox-source-docs.mozilla.org/toolkit/modules/toolkit_modules/FirstStartup.html>`_
        First startup is a Windows-only technical process that occurs before the first application window of Firefox appears. It is invoked by the Windows installer to perform essential initializations and ensures that all necessary background tasks are completed before the user interacts with the browser.

    Forced Enrollment
        Forced Enrollment is a method used in experiments to ensure that specific users are enrolled into an experiment or receive a particular message. Forced enrollment ignores :term:`advanced targeting` used for :term:`natural enrollment`. It is often used during testing phases to verify the correct functioning and impact of messages or features before broader deployment. Forced Enrollment can be initiated through specific URLs or browser configurations.

    `Frequency Capping <frequency-caps.html>`_
        Frequency capping limits the number of times a message is shown to a user to avoid overexposure and annoyance. This ensures a better user experience.

    `Glean <https://docs.telemetry.mozilla.org/concepts/glean/glean.html>`_
        Glean is Mozilla’s telemetry and product analytics solution, designed to collect, validate, and store data across products. Glean provides a more modern, flexible framework than traditional `Firefox telemetry <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/index.html>`_. This includes automatic documentation, stricter data review processes, and enhanced privacy features.

    `Groups and Campaigns <https://experimenter.info/messaging/groups-and-campaigns>`_
        Groups and campaigns refer to organized sets of messages targeted at specific user segments to achieve particular goals, such as promoting a new feature or encouraging user engagement.

    `Holdback <https://experimenter.info/for-product/#when-shipping-product-changes-a-guide-on-when-to-use-what-option>`_
        Holdback refers to a control strategy in experiments and feature rollouts where a portion of the user population does not receive a new change. This allows for comparison between those who experience the change and the holdback group who do not, helping to measure the impact of the change on key metrics.

    `Impression <https://experimenter.info/messaging/display-logic#impressions>`_
        An impression is recorded each time a message is shown to a user. The specifics of what counts as a single impression can vary depending on the messaging surface. Impressions differ from :term:`exposure` events in that the latter also captures when a message would have been seen by users in no-message control groups.

    Long-term Holdbacks
        Long-term Holdbacks are long-running "meta-experiments" that measure the cumulative impact of a series of deliveries (:term:`experiments<experiment>` and :term:`rollouts<rollout>`). This is achieved by maintaining a holdback group of users that do not receive these deliveries for some period of time, allowing for the comparison of this group’s performance against those who do not.

    `Message <index.html>`_
        A message in the context of ASRouter is a configuration that defines the triggers, targeting criteria, and content for a specific message. Messages are used to control what messages are shown and when. Message content can vary in format, including text, images, and/or interactive elements.

    `Message Provider <debugging-docs.html#how-to-see-all-messages-from-a-provider>`_
        Message Providers are sources of messages for the ASRouter. They can be local JSON files, remote endpoints, or other internal services that supply messages to be displayed.

    Natural Enrollment
        Natural Enrollment occurs when users are automatically enrolled in an experiment based on predefined advanced targeting criteria without any forced actions. It reflects typical user behavior and provides more accurate data on the experiment's impact as enrolled users meet advanced targeting conditions.

    `Nimbus <https://firefox-source-docs.mozilla.org/toolkit/components/nimbus/docs/index.html>`_
        Nimbus is a platform used by Mozilla for running experiments and feature rollouts. Nimbus manages :term:`recipes<recipe>` and pushes them to :term:`Remote Settings` for wider distribution. The Messaging System often interfaces with Nimbus to deliver experimental messages and collect data.

    `Onboarding <first-run.html>`_
        Onboarding refers to the set of messages and flows designed to guide new and returning users through initial setup and familiarize them with key features of Firefox. One example for new users is the :term:`about:welcome` flow.

    `Reach <https://experimenter.info/messaging/telemetry/>`_
        Reach refers to the potential audience size for a message in an experiment branch, calculated based on the conditions met for message display even if the user is not enrolled in the experiment branch showing the message.

    Recipe
        Recipes define Firefox experiments run with :term:`Nimbus`. Recipes that might apply to a particular user are delivered to Firefox from :term:`Remote Settings`.

    `Remote Localization <https://experimenter.info/messaging/remote-localization>`_
        Remote localization involves updating message content for different languages and regions dynamically, ensuring that messages are relevant and understandable for users worldwide. This is achieved through the use of `Fluent <https://firefox-source-docs.mozilla.org/l10n/fluent/index.html>`_ ids for strings landed in the Firefox source code.

    `Remote Settings <https://firefox-source-docs.mozilla.org/services/settings/>`_
        Remote Settings is a service that allows Mozilla to remotely update and configure various settings within Firefox, including ASRouter messages. Remote Settings can be thought of as a CDN (Content Distribution Network) for efficiently distributing recipes. It ensures messages can be dynamically updated without requiring a browser update.

    `Rollout <https://experimenter.info/for-product/#when-shipping-product-changes-a-guide-on-when-to-use-what-option>`_
        A Rollout refers to the off-train deployment of a product change to a defined user population. "Off-`train <https://firefox-source-docs.mozilla.org/contributing/pocket-guide-shipping-firefox.html#train-model>`_" means updates are deployed outside of the regular Firefox `release cycle <https://firefox-source-docs.mozilla.org/contributing/pocket-guide-shipping-firefox.html#release-cycle>`_. Unlike experiments that compare control and treatment groups to measure causal impacts, rollouts primarily focus on reducing technical risks and ensuring scalability. Rollouts can be scaled up or down as needed and allow for the immediate availability of changes to a wide audience while monitoring the impact on key metrics.

    `Schemas <https://firefox-source-docs.mozilla.org/toolkit/components/messaging-system/docs/index.html>`_
        JSON Schema is used to define the structure and validation rules for JSON data used in ASRouter messages. Schemas ensures that messages adhere to the expected format and content requirements.

    `Sticky Enrollment <https://experimenter.info/2022-07#sticky-enrollment>`_
        Sticky Enrollment ensures that once a user is enrolled in an experiment, they remain in that condition for the entire duration, even if the targeting criteria no longer apply. This prevents users from being unenrolled or switched between different branches, providing consistent exposure to the experiment's conditions. Some targeting configurations require sticky enrollment, such as those used for :term:`first run experiments<first run experiment>`.

    `Targeting <targeting-attributes.html>`_
        Targeting refers to the use of specific criteria to determine which messages are shown to which users. This involves using JEXL (JavaScript Expression Language) expressions to evaluate attributes such as user preferences, browser configurations, and behaviors. Targeting ensures that messages are relevant to the user.

    `Messaging Telemetry <https://experimenter.info/messaging/telemetry>`_
        Messaging Telemetry refers to the collection of data on user interactions with messages, such as impressions, button clicks, and dismissals. This data is used for analyzing the effectiveness of messages and guiding future improvements. Messaging Telemetry should not be confused with the more generic `Firefox Telemetry <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/index.html>`_.

    Treatment Branch
        A Treatment Branch in an experiment refers to a specific variant or condition being tested. Each treatment branch represents a different version of the message or feature being evaluated. This allows for comparison across branches and/or against a :term:`control branch` to determine the impact of the variant or condition.

    `Trigger <https://experimenter.info/messaging/display-logic/#triggers>`_
        Triggers are events or conditions that activate the delivery of a message by the Messaging System. They dictate when a message will try to appear for a user. These can include user actions, time-based conditions, or specific states within the browser.
