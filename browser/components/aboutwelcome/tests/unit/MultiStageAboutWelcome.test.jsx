import { GlobalOverrider } from "newtab/test/unit/utils";
import {
  MultiStageAboutWelcome,
  SecondaryCTA,
  StepsIndicator,
  ProgressBar,
  WelcomeScreen,
} from "content-src/components/MultiStageAboutWelcome";
import { SingleSelect } from "content-src/components/SingleSelect";
import React from "react";
import { shallow, mount } from "enzyme";
import { AboutWelcomeDefaults } from "modules/AboutWelcomeDefaults.sys.mjs";
import { AboutWelcomeUtils } from "content-src/lib/aboutwelcome-utils.mjs";

const spinEventLoop = async () => {
  // Spin the event loop to allow the useEffect hooks to execute,
  // any promises to resolve, and re-rendering to happen after the
  // promises have updated the state/props
  await new Promise(resolve => setTimeout(resolve, 0));
};

describe("MultiStageAboutWelcome module", () => {
  let globals;
  let sandbox;

  const DEFAULT_PROPS = {
    defaultScreens: AboutWelcomeDefaults.getDefaults().screens,
    metricsFlowUri: "http://localhost/",
    message_id: "DEFAULT_ABOUTWELCOME",
    utm_term: "default",
    startScreen: 0,
  };

  beforeEach(async () => {
    globals = new GlobalOverrider();
    globals.set({
      AWEvaluateScreenTargeting: () => {},
      AWGetSelectedTheme: () => Promise.resolve("automatic"),
      AWGetInstalledAddons: () => Promise.resolve(["test-addon-id"]),
      AWGetUnhandledCampaignAction: () => Promise.resolve(false),
      AWSendEventTelemetry: () => {},
      AWSendToParent: () => {},
      AWWaitForMigrationClose: () => Promise.resolve(),
      AWSelectTheme: () => Promise.resolve(),
      AWFinish: () => Promise.resolve(),
    });
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  describe("MultiStageAboutWelcome functional component", () => {
    it("should render MultiStageAboutWelcome", () => {
      const wrapper = shallow(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);

      assert.ok(wrapper.exists());
    });

    it("should pass activeTheme and initialTheme props to WelcomeScreen", async () => {
      let wrapper = mount(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);
      await spinEventLoop();
      // sync up enzyme's representation with the real DOM
      wrapper.update();

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      assert.strictEqual(welcomeScreenWrapper.prop("activeTheme"), "automatic");
      assert.strictEqual(
        welcomeScreenWrapper.prop("initialTheme"),
        "automatic"
      );
    });

    it("should fetch a list of installed Addons", async () => {
      let wrapper = mount(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);
      await spinEventLoop();
      // sync up enzyme's representation with the real DOM
      wrapper.update();

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      assert.strictEqual(
        welcomeScreenWrapper.prop("installedAddons")[0],
        "test-addon-id"
      );
    });

    it("should handle primary Action", () => {
      const screens = [
        {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              label: "Test button",
              action: {
                navigate: true,
              },
            },
          },
        },
      ];

      const PRIMARY_ACTION_PROPS = {
        defaultScreens: screens,
        metricsFlowUri: "http://localhost/",
        message_id: "DEFAULT_ABOUTWELCOME",
        utm_term: "default",
        startScreen: 0,
      };

      const stub = sinon.stub(AboutWelcomeUtils, "sendActionTelemetry");
      let wrapper = mount(<MultiStageAboutWelcome {...PRIMARY_ACTION_PROPS} />);
      wrapper.update();

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      const btnPrimary = welcomeScreenWrapper.find(".primary");
      btnPrimary.simulate("click");
      assert.calledOnce(stub);
      assert.equal(
        stub.firstCall.args[0],
        welcomeScreenWrapper.props().messageId
      );
      assert.equal(stub.firstCall.args[1], "primary_button");
      stub.restore();
    });

    it("should autoAdvance on last screen and send appropriate telemetry", () => {
      let clock = sinon.useFakeTimers();
      const screens = [
        {
          auto_advance: "primary_button",
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              label: "Test Button",
              action: {
                navigate: true,
              },
            },
          },
        },
      ];
      const AUTO_ADVANCE_PROPS = {
        defaultScreens: screens,
        metricsFlowUri: "http://localhost/",
        message_id: "DEFAULT_ABOUTWELCOME",
        utm_term: "default",
        startScreen: 0,
      };
      const wrapper = mount(<MultiStageAboutWelcome {...AUTO_ADVANCE_PROPS} />);
      wrapper.update();
      const finishStub = sandbox.stub(global, "AWFinish");
      const telemetryStub = sinon.stub(
        AboutWelcomeUtils,
        "sendActionTelemetry"
      );

      assert.notCalled(finishStub);
      clock.tick(20001);
      assert.calledOnce(finishStub);
      assert.calledOnce(telemetryStub);
      assert.equal(telemetryStub.lastCall.args[2], "AUTO_ADVANCE");
      clock.restore();
      finishStub.restore();
      telemetryStub.restore();
    });

    it("should send telemetry ping on collectSelect", () => {
      const screens = [
        {
          id: "EASY_SETUP_TEST",
          content: {
            tiles: {
              type: "multiselect",
              data: [
                {
                  id: "checkbox-1",
                  defaultValue: true,
                },
              ],
            },
            primary_button: {
              label: "Test Button",
              action: {
                collectSelect: true,
              },
            },
          },
        },
      ];
      const EASY_SETUP_PROPS = {
        defaultScreens: screens,
        message_id: "DEFAULT_ABOUTWELCOME",
        startScreen: 0,
      };
      const stub = sinon.stub(AboutWelcomeUtils, "sendActionTelemetry");
      let wrapper = mount(<MultiStageAboutWelcome {...EASY_SETUP_PROPS} />);
      wrapper.update();

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      const btnPrimary = welcomeScreenWrapper.find(".primary");
      btnPrimary.simulate("click");
      assert.calledTwice(stub);
      assert.equal(
        stub.firstCall.args[0],
        welcomeScreenWrapper.props().messageId
      );
      assert.equal(stub.firstCall.args[1], "primary_button");
      assert.equal(
        stub.lastCall.args[0],
        welcomeScreenWrapper.props().messageId
      );
      assert.ok(stub.lastCall.args[1].includes("checkbox-1"));
      assert.equal(stub.lastCall.args[2], "SELECT_CHECKBOX");
      stub.restore();
    });
  });

  describe("WelcomeScreen component", () => {
    describe("easy setup screen", () => {
      const easySetupScreen = AboutWelcomeDefaults.getDefaults().screens.find(
        s => s.id === "AW_EASY_SETUP_NEEDS_DEFAULT_AND_PIN"
      );
      let EASY_SETUP_SCREEN_PROPS;

      beforeEach(() => {
        EASY_SETUP_SCREEN_PROPS = {
          id: easySetupScreen.id,
          content: easySetupScreen.content,
          messageId: `${DEFAULT_PROPS.message_id}_${easySetupScreen.id}`,
          UTMTerm: DEFAULT_PROPS.utm_term,
          flowParams: null,
          totalNumberOfScreens: 1,
          setScreenMultiSelects: sandbox.stub(),
          setActiveMultiSelect: sandbox.stub(),
        };
      });

      it("should render Easy Setup screen", () => {
        const wrapper = shallow(<WelcomeScreen {...EASY_SETUP_SCREEN_PROPS} />);
        assert.ok(wrapper.exists());
      });

      it("should render secondary.top button", () => {
        let SCREEN_PROPS = {
          content: {
            title: "Step",
            secondary_button_top: {
              text: "test",
              label: "test label",
            },
          },
          position: "top",
        };
        const wrapper = mount(<SecondaryCTA {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.secondary-cta.top").exists());
      });

      it("should render the arrow icon in the secondary button", () => {
        let SCREEN_PROPS = {
          content: {
            title: "Step",
            secondary_button: {
              has_arrow_icon: true,
              label: "test label",
            },
          },
        };
        const wrapper = mount(<SecondaryCTA {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("button.arrow-icon").exists());
      });

      it("should render steps indicator", () => {
        let PROPS = { totalNumberOfScreens: 1 };
        const wrapper = mount(<StepsIndicator {...PROPS} />);
        assert.ok(wrapper.find("div.indicator").exists());
      });

      it("should assign the total number of screens and current screen to the aria-valuemax and aria-valuenow labels", () => {
        const EXTRA_PROPS = { totalNumberOfScreens: 3, order: 1 };
        const wrapper = mount(
          <WelcomeScreen {...EASY_SETUP_SCREEN_PROPS} {...EXTRA_PROPS} />
        );

        const steps = wrapper.find(`div.steps`);
        assert.ok(steps.exists());
        const { attributes } = steps.getDOMNode();
        assert.equal(
          parseInt(attributes.getNamedItem("aria-valuemax").value, 10),
          EXTRA_PROPS.totalNumberOfScreens
        );
        assert.equal(
          parseInt(attributes.getNamedItem("aria-valuenow").value, 10),
          EXTRA_PROPS.order + 1
        );
      });

      it("should render progress bar", () => {
        let SCREEN_PROPS = {
          step: 1,
          previousStep: 0,
          totalNumberOfScreens: 2,
        };
        const wrapper = mount(<ProgressBar {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.indicator").exists());
        assert.propertyVal(
          wrapper.find("div.indicator").prop("style"),
          "--progress-bar-progress",
          "50%"
        );
      });

      it("should have a primary, secondary and secondary.top button in the rendered input", () => {
        const wrapper = mount(<WelcomeScreen {...EASY_SETUP_SCREEN_PROPS} />);
        assert.ok(wrapper.find(".primary").exists());
        assert.ok(
          wrapper
            .find(".secondary-cta button.secondary[value='secondary_button']")
            .exists()
        );
        assert.ok(
          wrapper
            .find(
              ".secondary-cta.top button.secondary[value='secondary_button_top']"
            )
            .exists()
        );
      });
    });

    describe("theme screen", () => {
      const THEME_SCREEN_PROPS = {
        id: "test-theme-screen",
        totalNumberOfScreens: 1,
        content: {
          title: "test title",
          subtitle: "test subtitle",
          tiles: {
            type: "theme",
            action: {
              theme: "<event>",
            },
            data: [
              {
                theme: "automatic",
                label: "test-label",
                tooltip: "test-tooltip",
                description: "test-description",
              },
            ],
          },
          primary_button: {
            action: {},
            label: "test button",
          },
        },
        navigate: null,
        messageId: `${DEFAULT_PROPS.message_id}_"test-theme-screen"`,
        UTMTerm: DEFAULT_PROPS.utm_term,
        flowParams: null,
        activeTheme: "automatic",
      };

      it("should render WelcomeScreen", () => {
        const wrapper = shallow(<WelcomeScreen {...THEME_SCREEN_PROPS} />);

        assert.ok(wrapper.exists());
      });

      it("should check this.props.activeTheme in the rendered input", () => {
        const wrapper = shallow(<SingleSelect {...THEME_SCREEN_PROPS} />);

        const selectedThemeInput = wrapper.find(".theme input[checked=true]");
        assert.strictEqual(
          selectedThemeInput.prop("value"),
          THEME_SCREEN_PROPS.activeTheme
        );
      });
    });
    describe("import screen", () => {
      const IMPORT_SCREEN_PROPS = {
        content: {
          title: "test title",
          subtitle: "test subtitle",
        },
      };
      it("should render ImportScreen", () => {
        const wrapper = mount(<WelcomeScreen {...IMPORT_SCREEN_PROPS} />);
        assert.ok(wrapper.exists());
      });
      it("should not have a primary or secondary button", () => {
        const wrapper = mount(<WelcomeScreen {...IMPORT_SCREEN_PROPS} />);
        assert.isFalse(wrapper.find(".primary").exists());
        assert.isFalse(
          wrapper.find(".secondary button[value='secondary_button']").exists()
        );
        assert.isFalse(
          wrapper
            .find(".secondary button[value='secondary_button_top']")
            .exists()
        );
      });
    });

    describe("Wallpaper screen", () => {
      let WALLPAPER_SCREEN_PROPS;
      beforeEach(() => {
        WALLPAPER_SCREEN_PROPS = {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            tiles: {
              type: "theme",
              category: {
                type: "wallpaper",
                action: {
                  type: "MULTI_ACTION",
                  data: {
                    actions: [
                      {
                        type: "SET_PREF",
                        data: {
                          pref: {
                            name: "test-dark",
                          },
                        },
                      },
                      {
                        type: "SET_PREF",
                        data: {
                          pref: {
                            name: "test-light",
                          },
                        },
                      },
                    ],
                  },
                },
              },
              action: {
                theme: "<event>",
              },
              data: [
                {
                  theme: "mountain",
                  type: "light",
                },
              ],
            },
            primary_button: {
              action: {},
              label: "test button",
            },
          },
          navigate: sandbox.stub(),
          setActiveTheme: sandbox.stub(),
        };
        sandbox.stub(AboutWelcomeUtils, "handleUserAction").resolves();
      });
      it("should handle wallpaper click", () => {
        const wrapper = mount(<WelcomeScreen {...WALLPAPER_SCREEN_PROPS} />);
        const wallpaperOptions = wrapper.find(
          ".tiles-single-select-section .select-item input[value='mountain']"
        );
        wallpaperOptions.simulate("click");
        assert.calledTwice(AboutWelcomeUtils.handleUserAction);
      });
    });

    describe("Single select picker screen", () => {
      let SINGLE_SELECT_SCREEN_PROPS;
      beforeEach(() => {
        SINGLE_SELECT_SCREEN_PROPS = {
          content: {
            title: {
              raw: "Test title",
            },
            subtitle: {
              raw: "Test subtitle",
            },
            tiles: {
              type: "single-select",
              selected: "test1",
              action: {
                picker: "<event>",
              },
              data: [
                {
                  id: "test1",
                  label: {
                    raw: "test1 label",
                  },
                  action: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "sidebar.revamp",
                        value: true,
                      },
                    },
                  },
                },
                {
                  defaultValue: true,
                  id: "test2",
                  label: {
                    raw: "test2 label",
                  },
                  flair: {
                    text: {
                      raw: "New!",
                    },
                  },
                  action: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "sidebar.revamp",
                        value: false,
                      },
                    },
                  },
                },
              ],
            },
            secondary_button: {
              label: {
                raw: "Skip this step",
              },
              action: {
                navigate: true,
              },
              has_arrow_icon: true,
            },
          },
          navigate: sandbox.stub(),
          setActiveSingleSelect: sandbox.stub(),
        };
        sandbox.stub(AboutWelcomeUtils, "handleUserAction").resolves();
      });
      it("should select the configured default value if present", async () => {
        const wrapper = mount(
          <WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />
        );
        assert.ok(
          wrapper
            .find(".tiles-single-select-section .select-item .test1")
            .exists()
        );
      });
      it("should preselect the active value if present", async () => {
        SINGLE_SELECT_SCREEN_PROPS.activeSingleSelect = "test2";
        const wrapper = mount(
          <WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />
        );
        assert.ok(
          wrapper
            .find(".tiles-single-select-section .select-item .test2")
            .exists()
        );
      });
      it("should handle item click", () => {
        const wrapper = mount(
          <WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />
        );
        const selectOption = wrapper.find(
          ".tiles-single-select-section .select-item input[value='test1']"
        );
        selectOption.simulate("click");
        assert.calledOnce(AboutWelcomeUtils.handleUserAction);
      });
      it("should handle item key down selection", () => {
        const wrapper = mount(
          <WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />
        );
        const selectOption = wrapper.find(
          ".tiles-single-select-section .select-item input[value='test1']"
        );
        selectOption.simulate("keydown", { key: "Enter" });
        assert.calledOnce(AboutWelcomeUtils.handleUserAction);
      });
      it("should render flair", () => {
        const wrapper = mount(
          <WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />
        );
        const flair = wrapper.find(".flair");
        assert.ok(flair.exists());
        assert.ok(flair.text() === "New!");
      });
      it("should automatically trigger the selected tile's action for an approved action", () => {
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.autoTrigger = true;
        mount(<WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />);
        assert.calledOnce(AboutWelcomeUtils.handleUserAction);
      });
      it("should not trigger the selected tile's action for an unapproved action", () => {
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.autoTrigger = true;
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.data[0].action = "OPEN_URL";

        mount(<WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />);

        assert.notCalled(AboutWelcomeUtils.handleUserAction);
      });
      it("should not trigger the selected tile's action for an unapproved pref with SET_PREF action", () => {
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.autoTrigger = true;
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.data[0].action.data.pref =
          "unapproved.pref";

        mount(<WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />);

        assert.notCalled(AboutWelcomeUtils.handleUserAction);
      });
      it("should trigger all of the selected tile's actions if MULTI_ACTION is used with SET_PREF and allowed prefs", () => {
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.autoTrigger = true;
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.data[0].action = {
          type: "MULTI_ACTION",
          data: {
            actions: [
              {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: "sidebar.revamp",
                  },
                },
              },
              {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: "sidebar.verticalTabs",
                  },
                },
              },
            ],
          },
        };
        mount(<WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />);
        assert.calledOnce(AboutWelcomeUtils.handleUserAction);
      });
      it("should not trigger any of the selected tile's action if MULTI_ACTION is used with one unallowed pref", () => {
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.autoTrigger = true;
        SINGLE_SELECT_SCREEN_PROPS.content.tiles.data[0].action = {
          type: "MULTI_ACTION",
          data: {
            actions: [
              {
                type: "OPEN_URL",
              },
              {
                type: "SET_PREF",
                data: {
                  pref: "sidebar.verticalTabs",
                },
              },
            ],
          },
        };
        mount(<WelcomeScreen {...SINGLE_SELECT_SCREEN_PROPS} />);
        assert.notCalled(AboutWelcomeUtils.handleUserAction);
      });
    });

    describe("#handleAction", () => {
      let SCREEN_PROPS;
      let TEST_ACTION;
      beforeEach(() => {
        SCREEN_PROPS = {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              action: {},
              label: "test button",
            },
          },
          navigate: sandbox.stub(),
          setActiveTheme: sandbox.stub(),
          UTMTerm: "you_tee_emm",
        };
        TEST_ACTION = SCREEN_PROPS.content.primary_button.action;
        sandbox.stub(AboutWelcomeUtils, "handleUserAction").resolves();
      });
      it("should handle navigate", () => {
        TEST_ACTION.navigate = true;
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledOnce(SCREEN_PROPS.navigate);
      });
      it("should handle theme", () => {
        TEST_ACTION.theme = "test";
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledWith(SCREEN_PROPS.setActiveTheme, "test");
      });
      it("should handle dismiss", () => {
        SCREEN_PROPS.content.dismiss_button = {
          action: { dismiss: true },
        };
        const finishStub = sandbox.stub(global, "AWFinish");
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".dismiss-button").simulate("click");

        assert.calledOnce(finishStub);
      });
      it("should handle SHOW_FIREFOX_ACCOUNTS", () => {
        TEST_ACTION.type = "SHOW_FIREFOX_ACCOUNTS";
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          data: {
            extraParams: {
              utm_campaign: "firstrun",
              utm_medium: "referral",
              utm_source: "activity-stream",
              utm_term: "you_tee_emm-screen",
            },
          },
          type: "SHOW_FIREFOX_ACCOUNTS",
        });
      });
      it("should handle OPEN_URL", () => {
        TEST_ACTION.type = "OPEN_URL";
        TEST_ACTION.data = {
          args: "https://example.com?utm_campaign=test-campaign",
        };
        TEST_ACTION.addFlowParams = true;
        let flowBeginTime = Date.now();
        const wrapper = mount(
          <WelcomeScreen
            {...SCREEN_PROPS}
            flowParams={{
              deviceId: "test-device-id",
              flowId: "test-flow-id",
              flowBeginTime,
            }}
          />
        );

        wrapper.find(".primary").simulate("click");

        let [handledAction] = AboutWelcomeUtils.handleUserAction.firstCall.args;
        assert.equal(handledAction.type, "OPEN_URL");
        let { searchParams } = new URL(handledAction.data.args);
        assert.equal(searchParams.get("utm_campaign"), "test-campaign");
        assert.equal(searchParams.get("utm_medium"), "referral");
        assert.equal(searchParams.get("utm_source"), "activity-stream");
        assert.equal(searchParams.get("utm_term"), "you_tee_emm-screen");
        assert.equal(searchParams.get("device_id"), "test-device-id");
        assert.equal(searchParams.get("flow_id"), "test-flow-id");
        assert.equal(
          searchParams.get("flow_begin_time"),
          flowBeginTime.toString()
        );
      });
      it("should handle SHOW_MIGRATION_WIZARD", () => {
        TEST_ACTION.type = "SHOW_MIGRATION_WIZARD";
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          type: "SHOW_MIGRATION_WIZARD",
        });
      });
      it("should handle SHOW_MIGRATION_WIZARD INSIDE MULTI_ACTION", async () => {
        const migrationCloseStub = sandbox.stub(
          global,
          "AWWaitForMigrationClose"
        );
        const MULTI_ACTION_SCREEN_PROPS = {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              action: {
                type: "MULTI_ACTION",
                navigate: true,
                data: {
                  actions: [
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                    },
                    {
                      type: "SET_DEFAULT_BROWSER",
                    },
                    {
                      type: "SHOW_MIGRATION_WIZARD",
                      data: {},
                    },
                  ],
                },
              },
              label: "test button",
            },
          },
          navigate: sandbox.stub(),
        };
        const wrapper = mount(<WelcomeScreen {...MULTI_ACTION_SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");
        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          type: "MULTI_ACTION",
          navigate: true,
          data: {
            actions: [
              {
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
              {
                type: "SET_DEFAULT_BROWSER",
              },
              {
                type: "SHOW_MIGRATION_WIZARD",
                data: {},
              },
            ],
          },
        });
        // handleUserAction returns a Promise, so let's let the microtask queue
        // flush so that anything waiting for the handleUserAction Promise to
        // resolve can run.
        await new Promise(resolve => queueMicrotask(resolve));
        assert.calledOnce(migrationCloseStub);
      });

      it("should handle SHOW_MIGRATION_WIZARD INSIDE NESTED MULTI_ACTION", async () => {
        const migrationCloseStub = sandbox.stub(
          global,
          "AWWaitForMigrationClose"
        );
        const MULTI_ACTION_SCREEN_PROPS = {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              action: {
                type: "MULTI_ACTION",
                navigate: true,
                data: {
                  actions: [
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                    },
                    {
                      type: "SET_DEFAULT_BROWSER",
                    },
                    {
                      type: "MULTI_ACTION",
                      data: {
                        actions: [
                          {
                            type: "SET_PREF",
                          },
                          {
                            type: "SHOW_MIGRATION_WIZARD",
                            data: {},
                          },
                        ],
                      },
                    },
                  ],
                },
              },
              label: "test button",
            },
          },
          navigate: sandbox.stub(),
        };
        const wrapper = mount(<WelcomeScreen {...MULTI_ACTION_SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");
        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          type: "MULTI_ACTION",
          navigate: true,
          data: {
            actions: [
              {
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
              {
                type: "SET_DEFAULT_BROWSER",
              },
              {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                    },
                    {
                      type: "SHOW_MIGRATION_WIZARD",
                      data: {},
                    },
                  ],
                },
              },
            ],
          },
        });
        // handleUserAction returns a Promise, so let's let the microtask queue
        // flush so that anything waiting for the handleUserAction Promise to
        // resolve can run.
        await new Promise(resolve => queueMicrotask(resolve));
        assert.calledOnce(migrationCloseStub);
      });
      it("should unset prefs from unchecked checkboxes", () => {
        const PREF_SCREEN_PROPS = {
          content: {
            title: "Checkboxes",
            tiles: {
              type: "multiselect",
              data: [
                {
                  id: "checkbox-1",
                  label: "checkbox 1",
                  checkedAction: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "pref-a",
                        value: true,
                      },
                    },
                  },
                  uncheckedAction: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "pref-a",
                      },
                    },
                  },
                },
                {
                  id: "checkbox-2",
                  label: "checkbox 2",
                  checkedAction: {
                    type: "MULTI_ACTION",
                    data: {
                      actions: [
                        {
                          type: "SET_PREF",
                          data: {
                            pref: {
                              name: "pref-b",
                              value: "pref-b",
                            },
                          },
                        },
                        {
                          type: "SET_PREF",
                          data: {
                            pref: {
                              name: "pref-c",
                              value: 3,
                            },
                          },
                        },
                      ],
                    },
                  },
                  uncheckedAction: {
                    type: "SET_PREF",
                    data: {
                      pref: { name: "pref-b" },
                    },
                  },
                },
              ],
            },
            primary_button: {
              label: "Set Prefs",
              action: {
                type: "MULTI_ACTION",
                collectSelect: true,
                isDynamic: true,
                navigate: true,
                data: {
                  actions: [],
                },
              },
            },
          },
          navigate: sandbox.stub(),
          setScreenMultiSelects: sandbox.stub(),
          setActiveMultiSelect: sandbox.stub(),
        };

        // No checkboxes checked. All prefs will be unset and pref-c will not be
        // reset.
        {
          const wrapper = mount(
            <WelcomeScreen {...PREF_SCREEN_PROPS} activeMultiSelect={[]} />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                { type: "SET_PREF", data: { pref: { name: "pref-a" } } },
                { type: "SET_PREF", data: { pref: { name: "pref-b" } } },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }

        // The first checkbox is checked. Only pref-a will be set and pref-c
        // will not be reset.
        {
          const wrapper = mount(
            <WelcomeScreen
              {...PREF_SCREEN_PROPS}
              activeMultiSelect={["checkbox-1"]}
            />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: "pref-a",
                      value: true,
                    },
                  },
                },
                { type: "SET_PREF", data: { pref: { name: "pref-b" } } },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }

        // The second checkbox is checked. Prefs pref-b and pref-c will be set.
        {
          const wrapper = mount(
            <WelcomeScreen
              {...PREF_SCREEN_PROPS}
              activeMultiSelect={["checkbox-2"]}
            />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                { type: "SET_PREF", data: { pref: { name: "pref-a" } } },
                {
                  type: "MULTI_ACTION",
                  data: {
                    actions: [
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-b", value: "pref-b" } },
                      },
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-c", value: 3 } },
                      },
                    ],
                  },
                },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }

        // // Both checkboxes are checked. All prefs will be set.
        {
          const wrapper = mount(
            <WelcomeScreen
              {...PREF_SCREEN_PROPS}
              activeMultiSelect={["checkbox-1", "checkbox-2"]}
            />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                {
                  type: "SET_PREF",
                  data: { pref: { name: "pref-a", value: true } },
                },
                {
                  type: "MULTI_ACTION",
                  data: {
                    actions: [
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-b", value: "pref-b" } },
                      },
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-c", value: 3 } },
                      },
                    ],
                  },
                },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }
      });
      it("Should handle a campaign action when applicable", async () => {
        let actionSpy = sandbox.spy(AboutWelcomeUtils, "handleCampaignAction");
        let telemetrySpy = sandbox.spy(
          AboutWelcomeUtils,
          "sendActionTelemetry"
        );

        globals.set("AWGetUnhandledCampaignAction", () =>
          Promise.resolve("SET_DEFAULT_BROWSER")
        );
        // Return true when "HANDLE_CAMPAIGN_ACTION" is sent to parent
        globals.set("AWSendToParent", () => Promise.resolve(true));
        const screens = [
          {
            content: {
              title: "test title",
            },
          },
        ];
        const TEST_PROPS = {
          defaultScreens: screens,
          message_id: "DEFAULT_ABOUTWELCOME",
          startScreen: 0,
        };
        let wrapper = mount(<MultiStageAboutWelcome {...TEST_PROPS} />);
        await spinEventLoop();
        wrapper.update();
        assert.calledOnce(actionSpy);
        // If campaign is handled, we should send telemetry
        assert.calledOnce(telemetrySpy);
        assert.equal(telemetrySpy.firstCall.args[1], "CAMPAIGN_ACTION");
        globals.restore();
      });
      it("Should not handle a campaign action when the action has already been handled", async () => {
        let actionSpy = sandbox.spy(AboutWelcomeUtils, "handleCampaignAction");
        let telemetrySpy = sandbox.spy(
          AboutWelcomeUtils,
          "sendActionTelemetry"
        );

        globals.set("AWGetUnhandledCampaignAction", () =>
          Promise.resolve(false)
        );
        const screens = [
          {
            content: {
              title: "test title",
            },
          },
        ];
        const TEST_PROPS = {
          defaultScreens: screens,
          message_id: "DEFAULT_ABOUTWELCOME",
          startScreen: 0,
        };
        let wrapper = mount(<MultiStageAboutWelcome {...TEST_PROPS} />);
        await spinEventLoop();
        wrapper.update();
        assert.notCalled(actionSpy);
        assert.notCalled(telemetrySpy);
        globals.restore();
      });
      it("Should not send telemetrty when campaign action handling fails", async () => {
        let actionSpy = sandbox.spy(AboutWelcomeUtils, "handleCampaignAction");
        let telemetrySpy = sandbox.spy(
          AboutWelcomeUtils,
          "sendActionTelemetry"
        );

        globals.set("AWGetUnhandledCampaignAction", () =>
          Promise.resolve("SET_DEFAULT_BROWSER")
        );

        // Return undefined when "HANDLE_CAMPAIGN_ACTION" is sent to parent as
        // though "AWPage:HANDLE_CAMPAIGN_ACTION" case did not return true due
        // to failure executing action or the campaign handled pref being true
        globals.set("AWSendToParent", () => Promise.resolve(undefined));
        const screens = [
          {
            content: {
              title: "test title",
            },
          },
        ];
        const TEST_PROPS = {
          defaultScreens: screens,
          message_id: "DEFAULT_ABOUTWELCOME",
          startScreen: 0,
        };
        let wrapper = mount(<MultiStageAboutWelcome {...TEST_PROPS} />);
        await spinEventLoop();
        wrapper.update();
        assert.calledOnce(actionSpy);
        // If campaign handling fails, we should not send telemetry
        assert.notCalled(telemetrySpy);
        globals.restore();
      });
    });

    describe("#handleUserAction", () => {
      let SCREEN_PROPS;
      let TEST_ACTION;
      let awSendToParentStub;
      let finishStub;
      let handleUserActionSpy;
      beforeEach(() => {
        SCREEN_PROPS = {
          content: {
            primary_button: {
              action: { type: "TEST_ACTION" },
              label: "test button",
            },
          },
        };
        awSendToParentStub = sandbox.stub();
        globals.set("AWSendToParent", awSendToParentStub);
        finishStub = sandbox.stub(global, "AWFinish");
        handleUserActionSpy = sandbox.spy(
          AboutWelcomeUtils,
          "handleUserAction"
        );

        TEST_ACTION = SCREEN_PROPS.content.primary_button.action;
      });

      afterEach(() => {
        handleUserActionSpy.restore();
        finishStub.restore();
        globals.restore();
      });

      it("Should dismiss when resolve boolean is true and needAwait true", async () => {
        TEST_ACTION.dismiss = "actionResult";
        TEST_ACTION.needsAwait = true;
        // `needsAwait` is true, so the handleUserAction function should return a `Promise<boolean>`
        awSendToParentStub.callsFake(
          () => Promise.resolve(true) // Resolves to a boolean for awaited calls
        );

        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);
        wrapper.find(".primary").simulate("click");

        // Assert click of primary button calls handleUserAction
        assert.calledOnce(handleUserActionSpy);
        assert.equal(handleUserActionSpy.firstCall.args[0].type, "TEST_ACTION");
        // handleUserAction returns a Promise, so let's let the microtask queue
        // flush so that anything waiting for the handleUserAction Promise to
        // resolve can run.
        await new Promise(resolve => queueMicrotask(resolve));

        // Check handleUserAction return value is a `Promise<boolean>`
        assert.ok(handleUserActionSpy.firstCall.returnValue instanceof Promise);
        const awaitedResult = await handleUserActionSpy.firstCall.returnValue;
        // Check that the result is a boolean
        assert.equal(
          typeof awaitedResult,
          "boolean",
          "The awaited call should return a boolean."
        );
        assert.equal(
          awaitedResult,
          true,
          "The awaited call should resolve to true, as per the mock return value."
        );
        // Check AWFinish gets called when awaited actionResult value is true
        assert.calledOnce(finishStub);
      });

      it("Should not dismiss when resolve boolean is false and needAwait true", async () => {
        TEST_ACTION.dismiss = "actionResult";
        TEST_ACTION.needsAwait = true;
        // `needsAwait` is true, so the handleUserAction function should return a `Promise<boolean>`
        awSendToParentStub.callsFake(
          () => Promise.resolve(false) // Resolves to a boolean for awaited calls
        );

        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);
        wrapper.find(".primary").simulate("click");

        // Assert click of primary button calls handleUserAction
        assert.calledOnce(handleUserActionSpy);
        assert.equal(handleUserActionSpy.firstCall.args[0].type, "TEST_ACTION");
        await new Promise(resolve => queueMicrotask(resolve));

        // Check handleUserAction return value is a `Promise<boolean>`
        assert.ok(handleUserActionSpy.firstCall.returnValue instanceof Promise);
        const awaitedResult = await handleUserActionSpy.firstCall.returnValue;
        // Check that the result is a boolean
        assert.equal(
          typeof awaitedResult,
          "boolean",
          "The awaited call should return a boolean."
        );
        assert.equal(
          awaitedResult,
          false,
          "The awaited call should resolve to false, as per the mock return value."
        );
        // Check AWFinish not get called when awaited actionResult value is false
        assert.notCalled(finishStub);
      });

      it("Should dismiss when true and handleUserAction not awaited", async () => {
        TEST_ACTION.dismiss = true;
        // `needsAwait` is not set, so the handleUserAction function should return a `Promise<undefined>`
        awSendToParentStub.callsFake(
          () => Promise.resolve(undefined) // Resolves to a undefined for non awaited calls
        );

        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);
        wrapper.find(".primary").simulate("click");

        // Assert click of primary button calls handleUserAction
        assert.calledOnce(handleUserActionSpy);
        assert.equal(handleUserActionSpy.firstCall.args[0].type, "TEST_ACTION");
        await new Promise(resolve => queueMicrotask(resolve));

        // Check handleUserAction return value is a `Promise<undefined>`
        assert.ok(handleUserActionSpy.firstCall.returnValue instanceof Promise);
        const awaitedResult = await handleUserActionSpy.firstCall.returnValue;
        assert.equal(
          awaitedResult,
          undefined,
          "The awaited call should resolve to undefined, as per the mock return value."
        );
        // Check AWFinish gets called when dismiss is true for non awaited calls
        assert.calledOnce(finishStub);
      });
    });
  });
});
