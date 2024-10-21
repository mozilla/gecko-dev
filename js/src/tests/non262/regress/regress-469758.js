// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

assertThrowsInstanceOfWithMessageCheck(
    () => {
      {let i=1}
      {let j=1; [][j][2]}
    },
    TypeError,
    message => message.endsWith("[][j] is undefined"));

reportCompare(0, 0, 'ok');
