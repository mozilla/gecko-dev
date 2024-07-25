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
