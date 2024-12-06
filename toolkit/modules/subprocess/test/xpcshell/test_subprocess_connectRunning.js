/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_subprocess_connectRunning() {
  if (AppConstants.platform === "win") {
    Assert.throws(
      () => Subprocess.connectRunning([42, 58, 63]),
      /Not implemented/
    );
    return;
  }

  let tempFile = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempFile.append("test-subprocess-connectRunning.txt");
  if (tempFile.exists()) {
    tempFile.remove(true);
  }
  registerCleanupFunction(async function () {
    tempFile.remove(true);
  });

  let running = await Subprocess.call({
    command: await Subprocess.pathSearch("tee"),
    arguments: [tempFile.path],
    environment: {},
    stderr: "pipe",
  });
  let { getSubprocessImplForTest } = ChromeUtils.importESModule(
    "resource://gre/modules/Subprocess.sys.mjs"
  );
  let worker = getSubprocessImplForTest().Process.getWorker();
  let fds = await worker.call("getFds", [running.id]);

  let wrongConnect = Subprocess.connectRunning(fds);
  await Assert.rejects(
    wrongConnect,
    function (error) {
      return /Attempt to connect FDs already handled by Subprocess/.test(
        error.message
      );
    },
    "Cannot reuse existing FDs in connectRunning"
  );

  // The test needs to dup() the FDs because of how "tee" is launched above:
  // when Subprocess.call() launched "tee" there will be Pipe created that
  // obviously refers to OS level FDs. When connectRunning() will be executed
  // then there will also be Pipe object created referencing the FDs. This
  // leads to a situation where 'running.{stdin,stdout}' and
  // 'proc.{stdin,stdout}' are sharing the same OS level FDs. Thus dup() is
  // required to make sure operations on those are done correctly. Especially
  // a Pipe on the JS side wraps the FD in a unix.Fd() which ensures a close()
  // is done so close() twice on the same FD is wrong.
  let { libc } = ChromeUtils.importESModule(
    "resource://gre/modules/subprocess/subprocess_unix.sys.mjs"
  );
  // unix.Fd() here should ensure there's a close() done.
  const duped_fds = fds.map(e => libc.dup(e));

  let proc = await Subprocess.connectRunning(duped_fds);
  equal(proc.pid, null, "Already running process pid is null");

  let contents = "lorem ipsum";
  let writeOp = proc.stdin.write(contents);
  equal(
    (await writeOp).bytesWritten,
    contents.length,
    "Contents correctly written to stdin"
  );
  let readOp = running.stdout.readString(contents.length);
  equal(await readOp, contents, "Pipes communication is functional");
  await running.kill();
  ok(tempFile.exists(), "temp file was written to");
  equal(
    await IOUtils.readUTF8(tempFile.path),
    contents,
    "Contents correctly written to temp file"
  );
  await proc.kill();
});

add_task(async function test_cleaned_up() {
  let { getSubprocessImplForTest } = ChromeUtils.importESModule(
    "resource://gre/modules/Subprocess.sys.mjs"
  );

  let worker = getSubprocessImplForTest().Process.getWorker();

  let openFiles = await worker.call("getOpenFiles", []);
  let processes = await worker.call("getProcesses", []);

  equal(openFiles.size, 0, "No remaining open files");
  equal(processes.size, 0, "No remaining processes");
});
