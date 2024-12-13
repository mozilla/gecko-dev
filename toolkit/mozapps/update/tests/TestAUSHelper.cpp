/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "updatedefines.h"

#ifdef XP_WIN
#  include "commonupdatedir.h"
#  include "updatehelper.h"
#  include "certificatecheck.h"

#  include <windows.h>

#  define NS_main wmain
#  define NS_tgetcwd _wgetcwd
#  define NS_ttoi _wtoi
#else
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <time.h>
#  define NS_main main
#  define NS_tgetcwd getcwd
#  define NS_ttoi atoi
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static void WriteMsg(const NS_tchar* path, const char* fmt, ...) {
  FILE* outFP = NS_tfopen(path, NS_T("wb"));
  if (!outFP) {
    return;
  }

  va_list ap;
  va_start(ap, fmt);

  vfprintf(outFP, fmt, ap);
  fprintf(outFP, "\n");

  fclose(outFP);
  outFP = nullptr;

  va_end(ap);
}

static bool CheckMsg(const NS_tchar* path, const char* expected) {
  FILE* inFP = NS_tfopen(path, NS_T("rb"));
  if (!inFP) {
    return false;
  }

  struct stat ms;
  if (fstat(fileno(inFP), &ms)) {
    fclose(inFP);
    inFP = nullptr;
    return false;
  }

  char* mbuf = (char*)malloc(ms.st_size + 1);
  if (!mbuf) {
    fclose(inFP);
    inFP = nullptr;
    return false;
  }

  size_t r = ms.st_size;
  char* rb = mbuf;
  size_t c = fread(rb, sizeof(char), 50, inFP);
  r -= c;
  if (c == 0 && r) {
    free(mbuf);
    fclose(inFP);
    inFP = nullptr;
    return false;
  }
  mbuf[ms.st_size] = '\0';
  rb = mbuf;

  bool isMatch = strcmp(rb, expected) == 0;
  free(mbuf);
  fclose(inFP);
  inFP = nullptr;
  return isMatch;
}

static bool BuildLogFilePath(NS_tchar* aLogFilePath, size_t aBufferSize,
                             NS_tchar** aArgv) {
#ifdef XP_MACOSX
  // Our tests on macOS require absolute paths to log files, as relative paths
  // will be interpreted as relative to the root of the file system, i.e. '/',
  // which is a read-only location where our tests can't write to.
  //
  // This finds the absolute path to `callback_app.app`, which will usually be:
  //    <abs-path>/dir.app/Contents/MacOS/
  //
  NS_tchar* callbackAppBundle = NS_tstrstr(aArgv[0], "callback_app.app");
  if (!callbackAppBundle) {
    return false;
  }

  // This appends the log file name to the same absolute path as
  // `callback_app.app`, which is what our tests expect.
  if (!NS_tvsnprintf(aLogFilePath, aBufferSize / sizeof(aLogFilePath[0]),
                     NS_T("%.*s%s"), callbackAppBundle - aArgv[0], aArgv[0],
                     aArgv[2])) {
    return false;
  }
#else
  if (!NS_tvsnprintf(aLogFilePath, aBufferSize / sizeof(aLogFilePath[0]),
                     NS_T("%s"), aArgv[2])) {
    return false;
  }
#endif
  return true;
}

int NS_main(int argc, NS_tchar** argv) {
  if (argc == 2) {
    if (!NS_tstrcmp(argv[1], NS_T("post-update-async")) ||
        !NS_tstrcmp(argv[1], NS_T("post-update-sync")) ||
        !NS_tstrcmp(argv[1], NS_T("post-update-environment"))) {
      NS_tchar exePath[MAXPATHLEN];
#ifdef XP_WIN
      if (!::GetModuleFileNameW(0, exePath, MAXPATHLEN)) {
        return 1;
      }
#else
      if (!NS_tvsnprintf(exePath, sizeof(exePath) / sizeof(exePath[0]),
                         NS_T("%s"), argv[0])) {
        return 1;
      }
#endif
      NS_tchar runFilePath[MAXPATHLEN];
      if (!NS_tvsnprintf(runFilePath,
                         sizeof(runFilePath) / sizeof(runFilePath[0]),
                         NS_T("%s.running"), exePath)) {
        return 1;
      }
#ifdef XP_WIN
      if (!NS_taccess(runFilePath, F_OK)) {
        // This makes it possible to check if the post update process was
        // launched twice which happens when the service performs an update.
        NS_tchar runFilePathBak[MAXPATHLEN];
        if (!NS_tvsnprintf(runFilePathBak,
                           sizeof(runFilePathBak) / sizeof(runFilePathBak[0]),
                           NS_T("%s.bak"), runFilePath)) {
          return 1;
        }
        MoveFileExW(runFilePath, runFilePathBak, MOVEFILE_REPLACE_EXISTING);
      }
#endif
      WriteMsg(runFilePath, "running");

      if (!NS_tstrcmp(argv[1], NS_T("post-update-sync"))) {
#ifdef XP_WIN
        Sleep(2000);
#else
        sleep(2);
#endif
      }

      NS_tchar logFilePath[MAXPATHLEN];
      if (!NS_tvsnprintf(logFilePath,
                         sizeof(logFilePath) / sizeof(logFilePath[0]),
                         NS_T("%s.log"), exePath)) {
        return 1;
      }

      WriteMsg(logFilePath, "post-update");

      if (!NS_tstrcmp(argv[1], NS_T("post-update-environment"))) {
        // Right now only one argument is supported for post update invocations,
        // so we hardcode the environment variable under test rather than
        // accepting it as an argument.
        //
        // N.b., any content written overwrites earlier content.

#if defined(XP_WIN) || defined(XP_MACOSX)
        const char* envVal = getenv("MOZ_TEST_POST_UPDATE_VAR");
        if (envVal) {
          WriteMsg(logFilePath, "MOZ_TEST_POST_UPDATE_VAR='%s'", envVal);
        } else {
          WriteMsg(logFilePath, "MOZ_TEST_POST_UPDATE_VAR=", envVal);
        }
#else
        // A failure with a message will be much easier to debug than an
        // abnormal exit code.
        WriteMsg(logFilePath,
                 "post-update-environment not supported on this platform");
#endif  // defined(XP_WIN) || defined(XP_MACOSX)
      }

      return 0;
    }
  }

  if (argc < 3) {
    fprintf(
        stderr,
        "\n"
        "Application Update Service Test Helper\n"
        "\n"
        "Usage: WORKINGDIR INFILE OUTFILE -s SECONDS [FILETOLOCK]\n"
        "   or: WORKINGDIR LOGFILE [ARG2 ARG3...]\n"
        "   or: signature-check filepath\n"
        "   or: setup-symlink dir1 dir2 file symlink\n"
        "   or: remove-symlink dir1 dir2 file symlink\n"
        "   or: check-symlink symlink\n"
        "   or: check-umask existing-umask\n"
        "   or: post-update-sync\n"
        "   or: post-update-async\n"
        "   or: post-update-environment\n"
        "   or: create-update-dir\n"
        "   or: wait-for-pid-exit pid timeout\n"
        "\n"
        "  WORKINGDIR  \tThe relative path to the working directory to use.\n"
        "  INFILE      \tThe relative path from the working directory for the "
        "file to\n"
        "              \tread actions to perform such as finish.\n"
        "  OUTFILE     \tThe relative path from the working directory for the "
        "file to\n"
        "              \twrite status information.\n"
        "  SECONDS     \tThe number of seconds to sleep.\n"
        "  FILETOLOCK  \tThe relative path from the working directory to an "
        "existing\n"
        "              \tfile to open exlusively.\n"
        "              \tOnly available on Windows platforms and silently "
        "ignored on\n"
        "              \tother platforms.\n"
        "  LOGFILE     \tThe relative path from the working directory to log "
        "the\n"
        "              \tcommand line arguments.\n"
        "  ARG2 ARG3...\tArguments to write to the LOGFILE after the preceding "
        "command\n"
        "              \tline arguments.\n"
        "\n"
        "Note: All paths must be relative.\n"
        "\n");
    return 1;
  }

  if (!NS_tstrcmp(argv[1], NS_T("check-signature"))) {
#if defined(XP_WIN) && defined(MOZ_MAINTENANCE_SERVICE)
    if (ERROR_SUCCESS == VerifyCertificateTrustForFile(argv[2])) {
      return 0;
    } else {
      return 1;
    }
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("setup-symlink"))) {
#ifdef XP_UNIX
    NS_tchar path[MAXPATHLEN];
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (mkdir(path, 0755)) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s/%s"),
                       NS_T("/tmp"), argv[2], argv[3])) {
      return 1;
    }
    if (mkdir(path, 0755)) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]),
                       NS_T("%s/%s/%s/%s"), NS_T("/tmp"), argv[2], argv[3],
                       argv[4])) {
      return 1;
    }
    FILE* file = NS_tfopen(path, NS_T("w"));
    if (file) {
      fputs(NS_T("test"), file);
      fclose(file);
    }
    if (symlink(path, argv[5]) != 0) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (argc > 6 && !NS_tstrcmp(argv[6], NS_T("change-perm"))) {
      if (chmod(path, 0644)) {
        return 1;
      }
    }
    return 0;
#else
    // Not implemented on non-Unix platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("remove-symlink"))) {
#ifdef XP_UNIX
    // The following can be called at the start of a test in case these symlinks
    // need to be removed if they already exist and at the end of a test to
    // remove the symlinks created by the test so ignore file doesn't exist
    // errors.
    NS_tchar path[MAXPATHLEN];
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (chmod(path, 0755) && errno != ENOENT) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]),
                       NS_T("%s/%s/%s/%s"), NS_T("/tmp"), argv[2], argv[3],
                       argv[4])) {
      return 1;
    }
    if (unlink(path) && errno != ENOENT) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s/%s"),
                       NS_T("/tmp"), argv[2], argv[3])) {
      return 1;
    }
    if (rmdir(path) && errno != ENOENT) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (rmdir(path) && errno != ENOENT) {
      return 1;
    }
    return 0;
#else
    // Not implemented on non-Unix platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("check-symlink"))) {
#ifdef XP_UNIX
    struct stat ss;
    if (lstat(argv[2], &ss)) {
      return 1;
    }
    return S_ISLNK(ss.st_mode) ? 0 : 1;
#else
    // Not implemented on non-Unix platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("check-umask"))) {
#ifdef XP_UNIX
    // Discover the current value of the umask.  There is no way to read the
    // umask without changing it.  The system call is specified as unable to
    // fail.
    uint32_t umask = ::umask(0777);
    ::umask(umask);

    NS_tchar logFilePath[MAXPATHLEN];
    if (!BuildLogFilePath(logFilePath, sizeof(logFilePath), argv)) {
      return 1;
    }

    FILE* logFP = NS_tfopen(logFilePath, NS_T("wb"));
    if (!logFP) {
      return 1;
    }
    fprintf(logFP, "check-umask\numask-%d\n", umask);

    fclose(logFP);
    logFP = nullptr;

    return 0;
#else
    // Not implemented on non-Unix platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("wait-for-service-stop"))) {
#if defined(XP_WIN) && defined(MOZ_MAINTENANCE_SERVICE)
    const int maxWaitSeconds = NS_ttoi(argv[3]);
    LPCWSTR serviceName = argv[2];
    DWORD serviceState = WaitForServiceStop(serviceName, maxWaitSeconds);
    if (SERVICE_STOPPED == serviceState) {
      return 0;
    } else {
      return serviceState;
    }
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("wait-for-application-exit"))) {
#ifdef XP_WIN
    const int maxWaitSeconds = NS_ttoi(argv[3]);
    LPCWSTR application = argv[2];
    DWORD ret = WaitForProcessExit(application, maxWaitSeconds);
    if (ERROR_SUCCESS == ret) {
      return 0;
    } else if (WAIT_TIMEOUT == ret) {
      return 1;
    } else {
      return 2;
    }
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("wait-for-pid-exit"))) {
    const int pid = NS_ttoi(argv[2]);
    const int maxWaitSeconds = NS_ttoi(argv[3]);

#ifdef XP_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (process == nullptr) {
      // Windows reacts to "that process already died" here by calling the pid
      // an invalid parameter.
      return GetLastError() == ERROR_INVALID_PARAMETER ? 0 : 2;
    }
    DWORD result = WaitForSingleObject(process, maxWaitSeconds * 1000);
    CloseHandle(process);
    if (result == WAIT_OBJECT_0) {
      return 0;
    } else if (result != WAIT_TIMEOUT) {
      return 2;
    }
    return 1;
#else
    time_t endTime = time(nullptr) + maxWaitSeconds;
    while (waitpid(pid, nullptr, WNOHANG) == 0) {
      if (time(nullptr) > endTime) {
        return 1;
      }
      sleep(1);
    }
    return 0;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("launch-service"))) {
#if defined(XP_WIN) && defined(MOZ_MAINTENANCE_SERVICE)
    DWORD ret =
        LaunchServiceSoftwareUpdateCommand(argc - 2, (LPCWSTR*)argv + 2);
    if (ret != ERROR_SUCCESS) {
      // 192 is used to avoid reusing a possible return value from the call to
      // WaitForServiceStop
      return 0x000000C0;
    }
    // Wait a maximum of 120 seconds.
    DWORD lastState = WaitForServiceStop(SVC_NAME, 120);
    if (SERVICE_STOPPED == lastState) {
      return 0;
    }
    return lastState;
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("create-update-dir"))) {
#ifdef XP_WIN
    mozilla::UniquePtr<wchar_t[]> updateDir;
    HRESULT result = GetCommonUpdateDirectory(argv[2], updateDir);
    return SUCCEEDED(result) ? 0 : 1;
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (NS_tchdir(argv[1]) != 0) {
    return 1;
  }

  // File in use test helper section
  if (!NS_tstrcmp(argv[4], NS_T("-s"))) {
    // Note: glibc's getcwd() allocates the buffer dynamically using malloc(3)
    // if buf (the 1st param) is NULL so free cwd when it is no longer needed.
    NS_tchar* cwd = NS_tgetcwd(nullptr, 0);
    NS_tchar inFilePath[MAXPATHLEN];
    if (!NS_tvsnprintf(inFilePath, sizeof(inFilePath) / sizeof(inFilePath[0]),
                       NS_T("%s/%s"), cwd, argv[2])) {
      return 1;
    }
    NS_tchar outFilePath[MAXPATHLEN];
    if (!NS_tvsnprintf(outFilePath,
                       sizeof(outFilePath) / sizeof(outFilePath[0]),
                       NS_T("%s/%s"), cwd, argv[3])) {
      return 1;
    }
    free(cwd);

    int seconds = NS_ttoi(argv[5]);
#ifdef XP_WIN
    HANDLE hFile = INVALID_HANDLE_VALUE;
    if (argc == 7) {
      hFile = CreateFileW(argv[6], DELETE | GENERIC_WRITE, 0, nullptr,
                          OPEN_EXISTING, 0, nullptr);
      if (hFile == INVALID_HANDLE_VALUE) {
        WriteMsg(outFilePath, "error_locking");
        return 1;
      }
    }

    WriteMsg(outFilePath, "sleeping");
    int i = 0;
    while (!CheckMsg(inFilePath, "finish\n") && i++ <= seconds) {
      Sleep(1000);
    }

    if (argc == 7) {
      CloseHandle(hFile);
    }
#else
    WriteMsg(outFilePath, "sleeping");
    int i = 0;
    while (!CheckMsg(inFilePath, "finish\n") && i++ <= seconds) {
      sleep(1);
    }
#endif
    WriteMsg(outFilePath, "finished");
    return 0;
  }

  {
    // Command line argument test helper section
    NS_tchar logFilePath[MAXPATHLEN];
    if (!BuildLogFilePath(logFilePath, sizeof(logFilePath), argv)) {
      return 1;
    }

    FILE* logFP = NS_tfopen(logFilePath, NS_T("wb"));
    if (!logFP) {
      return 1;
    }
    for (int i = 1; i < argc; ++i) {
      fprintf(logFP, LOG_S "\n", argv[i]);
    }

    fclose(logFP);
    logFP = nullptr;
  }

  return 0;
}
