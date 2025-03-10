"use strict";
/* Helper functions to munge SDP and split the sending track into
 * separate tracks on the receiving end. This can be done in a number
 * of ways, the one used here uses the fact that the MID and RID header
 * extensions which are used for packet routing share the same wire
 * format. The receiver interprets the rids from the sender as mids
 * which allows receiving the different spatial resolutions on separate
 * m-lines and tracks.
 */

// Borrowed from wpt, with some dependencies removed.

const ridExtensions = [
  "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id",
  "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id",
];

function ridToMid(description, rids) {
  const sections = SDPUtils.splitSections(description.sdp);
  const dtls = SDPUtils.getDtlsParameters(sections[1], sections[0]);
  const ice = SDPUtils.getIceParameters(sections[1], sections[0]);
  const rtpParameters = SDPUtils.parseRtpParameters(sections[1]);
  const setupValue = description.sdp.match(/a=setup:(.*)/)[1];
  const directionValue =
    description.sdp.match(/a=sendrecv|a=sendonly|a=recvonly|a=inactive/) ||
    "a=sendrecv";
  const mline = SDPUtils.parseMLine(sections[1]);

  // Skip mid extension; we are replacing it with the rid extmap
  rtpParameters.headerExtensions = rtpParameters.headerExtensions.filter(
    ext => ext.uri != "urn:ietf:params:rtp-hdrext:sdes:mid"
  );

  for (const ext of rtpParameters.headerExtensions) {
    if (ext.uri == "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id") {
      ext.uri = "urn:ietf:params:rtp-hdrext:sdes:mid";
    }
  }

  // Filter rtx as we have no way to (re)interpret rrid.
  // Not doing this makes probing use RTX, it's not understood and ramp-up is slower.
  rtpParameters.codecs = rtpParameters.codecs.filter(
    c => c.name.toUpperCase() !== "RTX"
  );

  if (!rids) {
    rids = Array.from(description.sdp.matchAll(/a=rid:(.*) send/g)).map(
      r => r[1]
    );
  }

  let sdp =
    SDPUtils.writeSessionBoilerplate() +
    SDPUtils.writeDtlsParameters(dtls, setupValue) +
    SDPUtils.writeIceParameters(ice) +
    "a=group:BUNDLE " +
    rids.join(" ") +
    "\r\n";
  const baseRtpDescription = SDPUtils.writeRtpDescription(
    mline.kind,
    rtpParameters
  );
  for (const rid of rids) {
    sdp +=
      baseRtpDescription +
      "a=mid:" +
      rid +
      "\r\n" +
      "a=msid:rid-" +
      rid +
      " rid-" +
      rid +
      "\r\n";
    sdp += directionValue + "\r\n";
  }
  return sdp;
}

function midToRid(description, localDescription, rids) {
  const sections = SDPUtils.splitSections(description.sdp);
  const dtls = SDPUtils.getDtlsParameters(sections[1], sections[0]);
  const ice = SDPUtils.getIceParameters(sections[1], sections[0]);
  const rtpParameters = SDPUtils.parseRtpParameters(sections[1]);
  const setupValue = description.sdp.match(/a=setup:(.*)/)[1];
  const directionValue =
    description.sdp.match(/a=sendrecv|a=sendonly|a=recvonly|a=inactive/) ||
    "a=sendrecv";
  const mline = SDPUtils.parseMLine(sections[1]);

  // Skip rid extensions; we are replacing them with the mid extmap
  rtpParameters.headerExtensions = rtpParameters.headerExtensions.filter(
    ext => !ridExtensions.includes(ext.uri)
  );

  for (const ext of rtpParameters.headerExtensions) {
    if (ext.uri == "urn:ietf:params:rtp-hdrext:sdes:mid") {
      ext.uri = "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
    }
  }

  const localMid = localDescription
    ? SDPUtils.getMid(SDPUtils.splitSections(localDescription.sdp)[1])
    : "0";

  if (!rids) {
    rids = [];
    for (let i = 1; i < sections.length; i++) {
      rids.push(SDPUtils.getMid(sections[i]));
    }
  }

  let sdp =
    SDPUtils.writeSessionBoilerplate() +
    SDPUtils.writeDtlsParameters(dtls, setupValue) +
    SDPUtils.writeIceParameters(ice) +
    "a=group:BUNDLE " +
    localMid +
    "\r\n";
  sdp += SDPUtils.writeRtpDescription(mline.kind, rtpParameters);
  // Although we are converting mids to rids, we still need a mid.
  // The first one will be consistent with trickle ICE candidates.
  sdp += "a=mid:" + localMid + "\r\n";
  sdp += directionValue + "\r\n";

  for (const rid of rids) {
    const stringrid = String(rid); // allow integers
    const choices = stringrid.split(",");
    choices.forEach(choice => {
      sdp += "a=rid:" + choice + " recv\r\n";
    });
  }
  if (rids.length) {
    sdp += "a=simulcast:recv " + rids.join(";") + "\r\n";
  }

  return sdp;
}

async function doOfferToSendSimulcast(offerer, answerer) {
  await offerer.setLocalDescription();

  // Is this a renegotiation? If so, we cannot remove (or reorder!) any mids,
  // even if some rids have been removed or reordered.
  let mids = [];
  if (answerer.localDescription) {
    // Renegotiation. Mids must be the same as before, because renegotiation
    // can never remove or reorder mids, nor can it expand the simulcast
    // envelope.
    mids = [...answerer.localDescription.sdp.matchAll(/a=mid:(.*)/g)].map(
      e => e[1]
    );
  } else {
    // First negotiation; the mids will be exactly the same as the rids
    const simulcastAttr = offerer.localDescription.sdp.match(
      /a=simulcast:send (.*)/
    );
    if (simulcastAttr) {
      mids = simulcastAttr[1].split(";");
    }
  }

  const nonSimulcastOffer = ridToMid(offerer.localDescription, mids);
  await answerer.setRemoteDescription({
    type: "offer",
    sdp: nonSimulcastOffer,
  });
}

async function doAnswerToRecvSimulcast(offerer, answerer, rids) {
  await answerer.setLocalDescription();
  const simulcastAnswer = midToRid(
    answerer.localDescription,
    offerer.localDescription,
    rids
  );
  await offerer.setRemoteDescription({ type: "answer", sdp: simulcastAnswer });
}

async function doOfferToRecvSimulcast(offerer, answerer, rids) {
  await offerer.setLocalDescription();
  const simulcastOffer = midToRid(
    offerer.localDescription,
    answerer.localDescription,
    rids
  );
  await answerer.setRemoteDescription({ type: "offer", sdp: simulcastOffer });
}

async function doAnswerToSendSimulcast(offerer, answerer) {
  await answerer.setLocalDescription();

  // See which mids the offerer had; it will barf if we remove or reorder them
  const mids = [...offerer.localDescription.sdp.matchAll(/a=mid:(.*)/g)].map(
    e => e[1]
  );

  const nonSimulcastAnswer = ridToMid(answerer.localDescription, mids);
  await offerer.setRemoteDescription({
    type: "answer",
    sdp: nonSimulcastAnswer,
  });
}

async function doOfferToSendSimulcastAndAnswer(offerer, answerer, rids) {
  await doOfferToSendSimulcast(offerer, answerer);
  await doAnswerToRecvSimulcast(offerer, answerer, rids);
}

async function doOfferToRecvSimulcastAndAnswer(offerer, answerer, rids) {
  await doOfferToRecvSimulcast(offerer, answerer, rids);
  await doAnswerToSendSimulcast(offerer, answerer);
}

// This would be useful for cases other than simulcast, but we do not use it
// anywhere else right now, nor do we have a place for wpt-friendly helpers at
// the moment.
function createPlaybackElement(track) {
  const elem = document.createElement(track.kind);
  elem.autoplay = true;
  elem.srcObject = new MediaStream([track]);
  elem.id = track.id;
  elem.width = 240;
  elem.height = 180;
  document.body.appendChild(elem);
  return elem;
}

async function getPlaybackWithLoadedMetadata(track) {
  const elem = createPlaybackElement(track);
  return new Promise(resolve => {
    elem.addEventListener("loadedmetadata", () => {
      resolve(elem);
    });
  });
}
