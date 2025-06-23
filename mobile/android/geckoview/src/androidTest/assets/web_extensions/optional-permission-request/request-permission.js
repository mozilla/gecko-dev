window.onload = () => {
  document.body.addEventListener("click", async () => {
    const perms = {
      permissions: ["activeTab"],
      origins: ["*://example.com/*"],
      data_collection: ["healthInfo"],
    };
    const response = await browser.permissions.request(perms);
    browser.runtime.sendNativeMessage("browser", `${response}`);
  });
};
