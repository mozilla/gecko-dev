/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.contacts = (function(_, mozL10n) {
  "use strict";

  var sharedMixins = loop.shared.mixins;

  const Button = loop.shared.views.Button;
  const ButtonGroup = loop.shared.views.ButtonGroup;
  const CALL_TYPES = loop.shared.utils.CALL_TYPES;

  // Number of contacts to add to the list at the same time.
  const CONTACTS_CHUNK_SIZE = 100;

  // At least this number of contacts should be present for the filter to appear.
  const MIN_CONTACTS_FOR_FILTERING = 7;

  let getContactNames = function(contact) {
    // The model currently does not enforce a name to be present, but we're
    // going to assume it is awaiting more advanced validation of required fields
    // by the model. (See bug 1069918)
    // NOTE: this method of finding a firstname and lastname is not i18n-proof.
    let names = contact.name[0].split(" ");
    return {
      firstName: names.shift(),
      lastName: names.join(" ")
    };
  };

  /** Used to retrieve the preferred email or phone number
   *  for the contact. Both fields are optional.
   * @param   {object} contact
   *          The contact object to get the field from.
   * @param   {string} field
   *          The field that should be read out of the contact object.
   * @returns {object} An object with a 'value' property that hold a string value.
   */
  let getPreferred = function(contact, field) {
    if (!contact[field] || !contact[field].length) {
      return { value: "" };
    }
    return contact[field].find(e => e.pref) || contact[field][0];
  };

  /** Used to set the preferred email or phone number
   *  for the contact. Both fields are optional.
   * @param   {object} contact
   *          The contact object to get the field from.
   * @param   {string} field
   *          The field within the contact to set.
   * @param   {string} value
   *          The value that the field should be set to.
   */
  let setPreferred = function(contact, field, value) {
    // Don't clear the field if it doesn't exist.
    if (!value && (!contact[field] || !contact[field].length)) {
      return;
    }

    if (!contact[field]) {
      contact[field] = [];
    }

    if (!contact[field].length) {
      contact[field][0] = {"value": value};
      return;
    }
    // Set the value in the preferred tuple and return.
    for (let i in contact[field]) {
      if (contact[field][i].pref) {
        contact[field][i].value = value;
        return;
      }
    }
    contact[field][0].value = value;
  };

  const GravatarPromo = React.createClass({
    mixins: [sharedMixins.WindowCloseMixin],

    propTypes: {
      handleUse: React.PropTypes.func.isRequired
    },

    getInitialState: function() {
      return {
        showMe: navigator.mozLoop.getLoopPref("contacts.gravatars.promo") &&
          !navigator.mozLoop.getLoopPref("contacts.gravatars.show")
      };
    },

    handleCloseButtonClick: function() {
      navigator.mozLoop.setLoopPref("contacts.gravatars.promo", false);
      this.setState({ showMe: false });
    },

    handleLinkClick: function(event) {
      if (!event.target || !event.target.href) {
        return;
      }

      event.preventDefault();
      navigator.mozLoop.openURL(event.target.href);
      this.closeWindow();
    },

    handleUseButtonClick: function() {
      navigator.mozLoop.setLoopPref("contacts.gravatars.promo", false);
      navigator.mozLoop.setLoopPref("contacts.gravatars.show", true);
      this.setState({ showMe: false });
      this.props.handleUse();
    },

    render: function() {
      if (!this.state.showMe) {
        return null;
      }

      let privacyUrl = navigator.mozLoop.getLoopPref("legal.privacy_url");
      let message = mozL10n.get("gravatars_promo_message", {
        "learn_more": React.renderToStaticMarkup(
          <a href={privacyUrl} target="_blank">
            {mozL10n.get("gravatars_promo_message_learnmore")}
          </a>
        )
      });
      return (
        <div className="contacts-gravatar-promo">
          <Button additionalClass="button-close"
                  caption=""
                  onClick={this.handleCloseButtonClick} />
          <p dangerouslySetInnerHTML={{__html: message}}
             onClick={this.handleLinkClick}></p>
          <ButtonGroup>
            <Button caption={mozL10n.get("gravatars_promo_button_nothanks")}
                    onClick={this.handleCloseButtonClick}/>
            <Button additionalClass="button-accept"
                    caption={mozL10n.get("gravatars_promo_button_use")}
                    onClick={this.handleUseButtonClick}/>
          </ButtonGroup>
        </div>
      );
    }
  });

  const ContactDropdown = React.createClass({
    propTypes: {
      // If the contact is blocked or not.
      blocked: React.PropTypes.bool.isRequired,
      canEdit: React.PropTypes.bool,
      handleAction: React.PropTypes.func.isRequired
    },

    getInitialState: function () {
      return {
        openDirUp: false
      };
    },

    componentDidMount: function () {
      // This method is called once when the dropdown menu is added to the DOM
      // inside the contact item.  If the menu extends outside of the visible
      // area of the scrollable list, it is re-rendered in different direction.

      let menuNode = this.getDOMNode();
      let menuNodeRect = menuNode.getBoundingClientRect();

      let listNode = document.getElementsByClassName("contact-list")[0];
      let listNodeRect = listNode.getBoundingClientRect();

      if (menuNodeRect.top + menuNodeRect.height >=
          listNodeRect.top + listNodeRect.height) {
        this.setState({
          openDirUp: true
        });
      }
    },

    onItemClick: function(event) {
      this.props.handleAction(event.currentTarget.dataset.action);
    },

    render: function() {
      var cx = React.addons.classSet;

      let blockAction = this.props.blocked ? "unblock" : "block";
      let blockLabel = this.props.blocked ? "unblock_contact_menu_button"
                                          : "block_contact_menu_button";

      return (
        <ul className={cx({ "dropdown-menu": true,
                            "dropdown-menu-up": this.state.openDirUp })}>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": this.props.blocked })}
              data-action="video-call"
              onClick={this.onItemClick}>
            <i className="icon icon-video-call" />
            {mozL10n.get("video_call_menu_button")}
          </li>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": this.props.blocked })}
              data-action="audio-call"
              onClick={this.onItemClick}>
            <i className="icon icon-audio-call" />
            {mozL10n.get("audio_call_menu_button")}
          </li>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": !this.props.canEdit })}
              data-action="edit"
              onClick={this.onItemClick}>
            <i className="icon icon-edit" />
            {mozL10n.get("edit_contact_menu_button")}
          </li>
          <li className="dropdown-menu-item"
              data-action={blockAction}
              onClick={this.onItemClick}>
            <i className={"icon icon-" + blockAction} />
            {mozL10n.get(blockLabel)}
          </li>
          <li className={cx({ "dropdown-menu-item": true,
                              "disabled": !this.props.canEdit })}
               data-action="remove"
               onClick={this.onItemClick}>
            <i className="icon icon-remove" />
            {mozL10n.get("remove_contact_menu_button2")}
          </li>
        </ul>
      );
    }
  });

  const ContactDetail = React.createClass({
    getInitialState: function() {
      return {
        showMenu: false
      };
    },

    propTypes: {
      contact: React.PropTypes.object.isRequired,
      handleContactAction: React.PropTypes.func
    },

    _onBodyClick: function() {
      // Hide the menu after other click handlers have been invoked.
      setTimeout(this.hideDropdownMenu, 10);
    },

    showDropdownMenu: function() {
      document.body.addEventListener("click", this._onBodyClick);
      this.setState({showMenu: true});
    },

    hideDropdownMenu: function() {
      document.body.removeEventListener("click", this._onBodyClick);
      // Since this call may be deferred, we need to guard it, for example in
      // case the contact was removed in the meantime.
      if (this.isMounted()) {
        this.setState({showMenu: false});
      }
    },

    componentWillUnmount: function() {
      document.body.removeEventListener("click", this._onBodyClick);
    },

    shouldComponentUpdate: function(nextProps, nextState) {
      let currContact = this.props.contact;
      let nextContact = nextProps.contact;
      let currContactEmail = getPreferred(currContact, "email").value;
      let nextContactEmail = getPreferred(nextContact, "email").value;
      return (
        currContact.name[0] !== nextContact.name[0] ||
        currContact.blocked !== nextContact.blocked ||
        currContactEmail !== nextContactEmail ||
        nextState.showMenu !== this.state.showMenu
      );
    },

    handleAction: function(actionName) {
      if (this.props.handleContactAction) {
        this.props.handleContactAction(this.props.contact, actionName);
      }
    },

    canEdit: function() {
      // We cannot modify imported contacts.  For the moment, the check for
      // determining whether the contact is imported is based on its category.
      return this.props.contact.category[0] != "google";
    },

    render: function() {
      let names = getContactNames(this.props.contact);
      let email = getPreferred(this.props.contact, "email");
      let cx = React.addons.classSet;
      let contactCSSClass = cx({
        contact: true,
        blocked: this.props.contact.blocked
      });

      return (
        <li className={contactCSSClass} onMouseLeave={this.hideDropdownMenu}>
          <div className="avatar">
            <img src={navigator.mozLoop.getUserAvatar(email.value)} />
          </div>
          <div className="details">
            <div className="username"><strong>{names.firstName}</strong> {names.lastName}
              <i className={cx({"icon icon-google": this.props.contact.category[0] == "google"})} />
              <i className={cx({"icon icon-blocked": this.props.contact.blocked})} />
            </div>
            <div className="email">{email.value}</div>
          </div>
          <div className="icons">
            <i className="icon icon-video"
               onClick={this.handleAction.bind(null, "video-call")} />
            <i className="icon icon-caret-down"
               onClick={this.showDropdownMenu} />
          </div>
          {this.state.showMenu
            ? <ContactDropdown blocked={this.props.contact.blocked}
                               canEdit={this.canEdit()}
                               handleAction={this.handleAction} />
            : null
          }
        </li>
      );
    }
  });

  const ContactsList = React.createClass({
    mixins: [
      React.addons.LinkedStateMixin,
      loop.shared.mixins.WindowCloseMixin
    ],

    propTypes: {
      notifications: React.PropTypes.instanceOf(
        loop.shared.models.NotificationCollection).isRequired,
        // Callback to handle entry to the add/edit contact form.
        startForm: React.PropTypes.func.isRequired
    },

    /**
     * Contacts collection object
     */
    contacts: null,

    /**
     * User profile
     */
    _userProfile: null,

    getInitialState: function() {
      return {
        importBusy: false,
        filter: ""
      };
    },

    refresh: function(callback = function() {}) {
      let contactsAPI = navigator.mozLoop.contacts;

      this.handleContactRemoveAll();

      contactsAPI.getAll((err, contacts) => {
        if (err) {
          callback(err);
          return;
        }

        // Add contacts already present in the DB. We do this in timed chunks to
        // circumvent blocking the main event loop.
        let addContactsInChunks = () => {
          contacts.splice(0, CONTACTS_CHUNK_SIZE).forEach(contact => {
            this.handleContactAddOrUpdate(contact, false);
          });
          if (contacts.length) {
            setTimeout(addContactsInChunks, 0);
          } else {
            callback();
          }
          this.forceUpdate();
        };

        addContactsInChunks(contacts);
      });
    },

    componentWillMount: function() {
      // Take the time to initialize class variables that are used outside
      // `this.state`.
      this.contacts = {};
      this._userProfile = navigator.mozLoop.userProfile;
    },

    componentDidMount: function() {
      window.addEventListener("LoopStatusChanged", this._onStatusChanged);

      this.refresh(err => {
        if (err) {
          throw err;
        }

        let contactsAPI = navigator.mozLoop.contacts;

        // Listen for contact changes/ updates.
        contactsAPI.on("add", (eventName, contact) => {
          this.handleContactAddOrUpdate(contact);
        });
        contactsAPI.on("remove", (eventName, contact) => {
          this.handleContactRemove(contact);
        });
        contactsAPI.on("removeAll", () => {
          this.handleContactRemoveAll();
        });
        contactsAPI.on("update", (eventName, contact) => {
          this.handleContactAddOrUpdate(contact);
        });
      });
    },

    componentWillUnmount: function() {
      window.removeEventListener("LoopStatusChanged", this._onStatusChanged);
    },

    _onStatusChanged: function() {
      let profile = navigator.mozLoop.userProfile;
      let currUid = this._userProfile ? this._userProfile.uid : null;
      let newUid = profile ? profile.uid : null;
      if (currUid != newUid) {
        // On profile change (login, logout), reload all contacts.
        this._userProfile = profile;
        // The following will do a forceUpdate() for us.
        this.refresh();
      }
    },

    handleContactAddOrUpdate: function(contact, render = true) {
      let contacts = this.contacts;
      let guid = String(contact._guid);
      contacts[guid] = contact;
      if (render) {
        this.forceUpdate();
      }
    },

    handleContactRemove: function(contact) {
      let contacts = this.contacts;
      let guid = String(contact._guid);
      if (!contacts[guid]) {
        return;
      }
      delete contacts[guid];
      this.forceUpdate();
    },

    handleContactRemoveAll: function() {
      // Do not allow any race conditions when removing all contacts.
      this.contacts = {};
      this.forceUpdate();
    },

    handleImportButtonClick: function() {
      this.setState({ importBusy: true });
      navigator.mozLoop.startImport({
        service: "google"
      }, (err, stats) => {
        this.setState({ importBusy: false });
        if (err) {
          console.error("Contact import error", err);
          this.props.notifications.errorL10n("import_contacts_failure_message");
          return;
        }
        this.props.notifications.successL10n("import_contacts_success_message", {
          num: stats.success,
          total: stats.success
        });
      });
    },

    handleAddContactButtonClick: function() {
      this.props.startForm("contacts_add");
    },

    handleContactAction: function(contact, actionName) {
      switch (actionName) {
        case "edit":
          this.props.startForm("contacts_edit", contact);
          break;
        case "remove":
          navigator.mozLoop.confirm({
            message: mozL10n.get("confirm_delete_contact_alert"),
            okButton: mozL10n.get("confirm_delete_contact_remove_button"),
            cancelButton: mozL10n.get("confirm_delete_contact_cancel_button")
          }, (error, result) => {
            if (error) {
              throw error;
            }

            if (!result) {
              return;
            }

            navigator.mozLoop.contacts.remove(contact._guid, err => {
              if (err) {
                throw err;
              }
            });
          });
          break;
        case "block":
        case "unblock":
          // Invoke the API named like the action.
          navigator.mozLoop.contacts[actionName](contact._guid, err => {
            if (err) {
              throw err;
            }
          });
          break;
        case "video-call":
          if (!contact.blocked) {
            navigator.mozLoop.calls.startDirectCall(contact, CALL_TYPES.AUDIO_VIDEO);
            this.closeWindow();
          }
          break;
        case "audio-call":
          if (!contact.blocked) {
            navigator.mozLoop.calls.startDirectCall(contact, CALL_TYPES.AUDIO_ONLY);
            this.closeWindow();
          }
          break;
        default:
          console.error("Unrecognized action: " + actionName);
          break;
      }
    },

    handleUseGravatar: function() {
      // We got permission to use Gravatar icons now, so we need to redraw the
      // list entirely to show them.
      this.refresh();
    },

    sortContacts: function(contact1, contact2) {
      let comp = contact1.name[0].localeCompare(contact2.name[0]);
      if (comp !== 0) {
        return comp;
      }
      // If names are equal, compare against unique ids to make sure we have
      // consistent ordering.
      return contact1._guid - contact2._guid;
    },

    render: function() {
      let cx = React.addons.classSet;

      let viewForItem = item => {
        return (
          <ContactDetail contact={item}
                         handleContactAction={this.handleContactAction}
                         key={item._guid} />
        );
      };

      let shownContacts = _.groupBy(this.contacts, function(contact) {
        return contact.blocked ? "blocked" : "available";
      });

      let showFilter = Object.getOwnPropertyNames(this.contacts).length >=
                       MIN_CONTACTS_FOR_FILTERING;
      if (showFilter) {
        let filter = this.state.filter.trim().toLocaleLowerCase();
        if (filter) {
          let filterFn = contact => {
            return contact.name[0].toLocaleLowerCase().includes(filter) ||
                   getPreferred(contact, "email").value.toLocaleLowerCase().includes(filter);
          };
          if (shownContacts.available) {
            shownContacts.available = shownContacts.available.filter(filterFn);
          }
          if (shownContacts.blocked) {
            shownContacts.blocked = shownContacts.blocked.filter(filterFn);
          }
        }
      }

      return (
        <div>
          <div className="content-area">
            <ButtonGroup>
              <Button caption={this.state.importBusy
                               ? mozL10n.get("importing_contacts_progress_button")
                               : mozL10n.get("import_contacts_button2")}
                      disabled={this.state.importBusy}
                      onClick={this.handleImportButtonClick}>
                <div className={cx({"contact-import-spinner": true,
                                    spinner: true,
                                    busy: this.state.importBusy})} />
              </Button>
              <Button caption={mozL10n.get("new_contact_button")}
                      onClick={this.handleAddContactButtonClick} />
            </ButtonGroup>
            {showFilter ?
            <input className="contact-filter"
                   placeholder={mozL10n.get("contacts_search_placesholder")}
                   valueLink={this.linkState("filter")} />
            : null }
            <GravatarPromo handleUse={this.handleUseGravatar}/>
          </div>
          <ul className="contact-list">
            {shownContacts.available ?
              shownContacts.available.sort(this.sortContacts).map(viewForItem) :
              null}
            {shownContacts.blocked && shownContacts.blocked.length > 0 ?
              <div className="contact-separator">{mozL10n.get("contacts_blocked_contacts")}</div> :
              null}
            {shownContacts.blocked ?
              shownContacts.blocked.sort(this.sortContacts).map(viewForItem) :
              null}
          </ul>
        </div>
      );
    }
  });

  const ContactDetailsForm = React.createClass({
    mixins: [React.addons.LinkedStateMixin],

    propTypes: {
      mode: React.PropTypes.string,
      // Callback used to change the selected tab - it is passed the tab name.
      selectTab: React.PropTypes.func.isRequired
    },

    getInitialState: function() {
      return {
        contact: null,
        pristine: true,
        name: "",
        email: "",
        tel: ""
      };
    },

    initForm: function(contact) {
      let state = this.getInitialState();
      if (contact) {
        state.contact = contact;
        state.name = contact.name[0];
        state.email = getPreferred(contact, "email").value;
        state.tel = getPreferred(contact, "tel").value;
      }
      this.setState(state);
    },

    handleAcceptButtonClick: function() {
      // Allow validity error indicators to be displayed.
      this.setState({
        pristine: false
      });

      let emailInput = this.refs.email.getDOMNode();
      let telInput = this.refs.tel.getDOMNode();
      if (!this.refs.name.getDOMNode().checkValidity() ||
          ((emailInput.required || emailInput.value) && !emailInput.checkValidity()) ||
          ((telInput.required || telInput.value) && !telInput.checkValidity())) {
        return;
      }

      this.props.selectTab("contacts");

      let contactsAPI = navigator.mozLoop.contacts;

      switch (this.props.mode) {
        case "edit":
          this.state.contact.name[0] = this.state.name.trim();
          setPreferred(this.state.contact, "email", this.state.email.trim());
          setPreferred(this.state.contact, "tel", this.state.tel.trim());
          contactsAPI.update(this.state.contact, err => {
            if (err) {
              throw err;
            }
          });
          this.setState({
            contact: null
          });
          break;
        case "add":
          var contact = {
            id: navigator.mozLoop.generateUUID(),
            name: [this.state.name.trim()],
            email: [{
              pref: true,
              type: ["home"],
              value: this.state.email.trim()
            }],
            category: ["local"]
          };
          var tel = this.state.tel.trim();
          if (!!tel) {
            contact.tel = [{
              pref: true,
              type: ["fxos"],
              value: tel
            }];
          }
          contactsAPI.add(contact, err => {
            if (err) {
              throw err;
            }
          });
          break;
      }
    },

    handleCancelButtonClick: function() {
      this.props.selectTab("contacts");
    },

    render: function() {
      let cx = React.addons.classSet;
      let phoneOrEmailRequired = !this.state.email && !this.state.tel;

      return (
        <div className="content-area contact-form">
          <header>{this.props.mode == "add"
                   ? mozL10n.get("add_contact_button")
                   : mozL10n.get("edit_contact_title")}</header>
          <label>{mozL10n.get("edit_contact_name_label")}</label>
          <input className={cx({pristine: this.state.pristine})}
                 pattern="\s*\S.*"
                 ref="name"
                 required
                 type="text"
                 valueLink={this.linkState("name")} />
          <label>{mozL10n.get("edit_contact_email_label")}</label>
          <input className={cx({pristine: this.state.pristine})}
                 ref="email"
                 required={phoneOrEmailRequired}
                 type="email"
                 valueLink={this.linkState("email")} />
          <label>{mozL10n.get("new_contact_fxos_phone_placeholder")}</label>
          <input className={cx({pristine: this.state.pristine})}
                 ref="tel"
                 required={phoneOrEmailRequired}
                 type="tel"
                 valueLink={this.linkState("tel")} />
          <ButtonGroup>
            <Button additionalClass="button-cancel"
                    caption={mozL10n.get("cancel_button")}
                    onClick={this.handleCancelButtonClick} />
            <Button additionalClass="button-accept"
                    caption={this.props.mode == "add"
                             ? mozL10n.get("add_contact_button")
                             : mozL10n.get("edit_contact_done_button")}
                    onClick={this.handleAcceptButtonClick} />
          </ButtonGroup>
        </div>
      );
    }
  });

  return {
    ContactsList: ContactsList,
    ContactDetailsForm: ContactDetailsForm,
    _getPreferred: getPreferred,
    _setPreferred: setPreferred
  };
})(_, document.mozL10n);
