/* eslint-env serviceworker */

onnotificationclick = () => {
  clients.openWindow("empty.html");
};
