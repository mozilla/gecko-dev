// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// --- certDN ---

if (DebugSymbol.findFunctionsNamed("CERT_AsciiToName").length) {
  console.log("Attaching `CERT_AsciiToName` interceptor...");
  Interceptor.attach(DebugSymbol.fromName("CERT_AsciiToName").address, {
    onEnter: function (args) {
      send({
        func: "CERT_AsciiToName",
        data: args[0].readUtf8String(),
      });
    },
  });
}

// --- pkcs7 ---

if (DebugSymbol.findFunctionsNamed("CERT_DecodeCertPackage").length) {
  console.log("Attaching `CERT_DecodeCertPackage` interceptor...");
  Interceptor.attach(DebugSymbol.fromName("CERT_DecodeCertPackage").address, {
    onEnter: function (args) {
      const len = args[1].toInt32();
      const buf = args[0].readByteArray(len);

      send({
        func: "CERT_DecodeCertPackage",
        data: new Uint8Array(buf),
      });
    },
  });
}

// --- pkcs8 ---

if (
  DebugSymbol.findFunctionsNamed("PK11_ImportDERPrivateKeyInfoAndReturnKey")
    .length
) {
  console.log(
    "Attaching `PK11_ImportDERPrivateKeyInfoAndReturnKey` interceptor...",
  );
  Interceptor.attach(
    DebugSymbol.fromName("PK11_ImportDERPrivateKeyInfoAndReturnKey").address,
    {
      onEnter: function (args) {
        const secItem = args[3]; // { type(8), data(8), len(4) }

        const len = secItem.add(8).add(8).readUInt();
        const buf = secItem.add(8).readByteArray(len);

        send({
          func: "PK11_ImportDERPrivateKeyInfoAndReturnKey",
          data: new Uint8Array(buf),
        });
      },
    },
  );
}

// --- pkcs12 ---

if (DebugSymbol.findFunctionsNamed("SEC_PKCS12DecoderUpdate").length) {
  console.log("Attaching `SEC_PKCS12DecoderUpdate` interceptor...");
  Interceptor.attach(DebugSymbol.fromName("SEC_PKCS12DecoderUpdate").address, {
    onEnter: function (args) {
      const len = args[2].toInt32();
      const buf = args[1].readByteArray(len);

      send({ func: "SEC_PKCS12DecoderUpdate", data: new Uint8Array(buf) });
    },
  });
}

// --- quickder ---

if (DebugSymbol.findFunctionsNamed("SEC_QuickDERDecodeItem_Util").length) {
  console.log("Attaching `SEC_QuickDERDecodeItem_Util` interceptor...");
  Interceptor.attach(
    DebugSymbol.fromName("SEC_QuickDERDecodeItem_Util").address,
    {
      onEnter: function (args) {
        const secItem = args[3]; // { type(8), data(8), len(4) }

        const len = secItem.add(8).add(8).readUInt();
        const buf = secItem.add(8).readByteArray(len);

        send({
          func: "SEC_QuickDERDecodeItem_Util",
          data: new Uint8Array(buf),
        });
      },
    },
  );
}

// --- TLS ---

if (DebugSymbol.findFunctionsNamed("ssl_DefClose").length) {
  console.log("Attaching `ssl_DefClose` interceptor...");
  Interceptor.attach(DebugSymbol.fromName("ssl_DefClose").address, {
    onEnter: function (args) {
      send({ func: "ssl_DefClose", ss: args[0] });
    },
  });
}

if (DebugSymbol.findFunctionsNamed("ssl_DefRecv").length) {
  console.log("Attaching `ssl_DefRecv` interceptor...");
  Interceptor.attach(DebugSymbol.fromName("ssl_DefRecv").address, {
    onEnter: function (args) {
      this.ss = args[0];
      this.buf = args[1];
      this.len = args[2].toInt32();
    },
    onLeave: function (_retVal) {
      const buf = this.buf.readByteArray(this.len);

      send({
        func: "ssl_DefRecv",
        ss: this.ss,
        data: new Uint8Array(buf),
      });
    },
  });
}

if (DebugSymbol.findFunctionsNamed("ssl_DefRead").length) {
  console.log("Attaching `ssl_DefRead` interceptor...");
  Interceptor.attach(DebugSymbol.fromName("ssl_DefRead").address, {
    onEnter: function (args) {
      this.ss = args[0];
      this.buf = args[1];
      this.len = args[2].toInt32();
    },
    onLeave: function (_retVal) {
      const buf = this.buf.readByteArray(this.len);

      send({
        func: "ssl_DefRead",
        ss: this.ss,
        data: new Uint8Array(buf),
      });
    },
  });
}
