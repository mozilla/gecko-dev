/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export const ProcessUtils = {
  isInParentProcess() {
    return (
      Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
    );
  },
};
