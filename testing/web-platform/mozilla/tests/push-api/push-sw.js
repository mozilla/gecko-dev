async function postAll(data) {
  const clients = await self.clients.matchAll({ includeUncontrolled: true });
  for (const client of clients) {
    client.postMessage(data);
  }
}

onpushsubscriptionchange = ev => {
  postAll({
    type: ev.type,
    constructor: ev.constructor.name,
    oldSubscription: ev.oldSubscription?.toJSON(),
    newSubscription: ev.newSubscription?.toJSON(),
  });
}
