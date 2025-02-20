const PAGE = `
<!doctype html>
<select style="width: 600px">
  <option>ABC</option>
  <option selected>DEFG</option>
</select>
`;

add_task(async function () {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], async function () {
        ok(
          content.document
            .querySelector("option[selected]")
            .matches(":checked"),
          "Option should be selected"
        );
      });

      let popup = await openSelectPopup("click");

      await SpecialPowers.spawn(browser, [], async function () {
        ok(
          content.document
            .querySelector("option[selected]")
            .matches(":checked"),
          "Option should still be selected"
        );
      });

      popup.hidePopup();
    }
  );
});
