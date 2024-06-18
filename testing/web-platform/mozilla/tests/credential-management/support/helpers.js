async function clearCredential(origin, id) {
  let deletePromise = new Promise((resolve) => {
    window.addEventListener(
      "message",
      (event) => {
        if (event.origin == origin) {
          if (event.data == "created") {
            resolve();
          }
        }
      }
    );
  });
  let win = window.open(`${origin}/_mozilla/credential-management/support/identity.provider-delete.sub.html?id=${id}`, "_blank");
  await deletePromise;
  win.close();
}

const ccs = SpecialPowers.Cc[
  "@mozilla.org/browser/credentialchooserservice;1"
].getService(SpecialPowers.Ci.nsICredentialChooserService);
