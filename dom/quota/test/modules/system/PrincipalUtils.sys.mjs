/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export const PrincipalUtils = {
  createPrincipal(url) {
    const uri = Services.io.newURI(url);
    return Services.scriptSecurityManager.createContentPrincipal(uri, {});
  },
};
