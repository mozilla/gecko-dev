/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "nsXPCOMGlue.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef XP_WIN
#include <windows.h>
#define snprintf _snprintf
#define strcasecmp _stricmp
#endif

#include "nsAppRunner.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsCRTGlue.h"
#include "nsStringAPI.h"
#include "nsServiceManagerUtils.h"
#include "plstr.h"
#include "prprf.h"
#include "prenv.h"
#include "nsINIParser.h"

#ifdef XP_WIN
#include "nsWindowsWMain.cpp"
#endif

#include "BinaryPath.h"

#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL

using namespace mozilla;

/**
 * Output a string to the user.  This method is really only meant to be used to
 * output last-ditch error messages designed for developers NOT END USERS.
 *
 * @param isError
 *        Pass true to indicate severe errors.
 * @param fmt
 *        printf-style format string followed by arguments.
 */
static void Output(bool isError, const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if (defined(XP_WIN) && !MOZ_WINCONSOLE)
  wchar_t msg[2048];
  _vsnwprintf(msg, sizeof(msg)/sizeof(msg[0]), NS_ConvertUTF8toUTF16(fmt).get(), ap);

  UINT flags = MB_OK;
  if (isError)
    flags |= MB_ICONERROR;
  else
    flags |= MB_ICONINFORMATION;
    
  MessageBoxW(nullptr, msg, L"XULRunner", flags);
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static bool IsArg(const char* arg, const char* s)
{
  if (*arg == '-')
  {
    if (*++arg == '-')
      ++arg;
    return !strcasecmp(arg, s);
  }

#if defined(XP_WIN)
  if (*arg == '/')
    return !strcasecmp(++arg, s);
#endif

  return false;
}

static nsresult
GetGREVersion(const char *argv0,
              nsACString *aMilestone,
              nsACString *aVersion)
{
  if (aMilestone)
    aMilestone->AssignLiteral("<Error>");
  if (aVersion)
    aVersion->AssignLiteral("<Error>");

  nsCOMPtr<nsIFile> iniFile;
  nsresult rv = BinaryPath::GetFile(argv0, getter_AddRefs(iniFile));
  if (NS_FAILED(rv))
    return rv;

  iniFile->SetNativeLeafName(NS_LITERAL_CSTRING("platform.ini"));

  nsINIParser parser;
  rv = parser.Init(iniFile);
  if (NS_FAILED(rv))
    return rv;

  if (aMilestone) {
    rv = parser.GetString("Build", "Milestone", *aMilestone);
    if (NS_FAILED(rv))
      return rv;
  }
  if (aVersion) {
    rv = parser.GetString("Build", "BuildID", *aVersion);
    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}

/**
 * A helper class which calls NS_LogInit/NS_LogTerm in its scope.
 */
class ScopedLogging
{
public:
  ScopedLogging() { NS_LogInit(); }
  ~ScopedLogging() { NS_LogTerm(); }
};

static void Usage(const char *argv0)
{
    nsAutoCString milestone;
    GetGREVersion(argv0, &milestone, nullptr);

    // display additional information (XXX make localizable?)
    Output(false,
           "Mozilla XULRunner %s\n\n"
           "Usage: " XULRUNNER_PROGNAME " [OPTIONS]\n"
           "       " XULRUNNER_PROGNAME " APP-FILE [APP-OPTIONS...]\n"
           "\n"
           "OPTIONS\n"
           "      --app                  specify APP-FILE (optional)\n"
           "  -h, --help                 show this message\n"
           "  -v, --version              show version\n"
           "  --gre-version              print the GRE version string on stdout\n"
           "\n"
           "APP-FILE\n"
           "  Application initialization file.\n"
           "\n"
           "APP-OPTIONS\n"
           "  Application specific options.\n",
           milestone.get());
}

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

static const nsDynamicFunctionLoad kXULFuncs[] = {
    { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
    { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
    { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
    { "XRE_main", (NSFuncPtr*) &XRE_main },
    { nullptr, nullptr }
};

class AutoAppData
{
public:
  AutoAppData(nsIFile* aINIFile) : mAppData(nullptr) {
    nsresult rv = XRE_CreateAppData(aINIFile, &mAppData);
    if (NS_FAILED(rv))
      mAppData = nullptr;
  }
  ~AutoAppData() {
    if (mAppData)
      XRE_FreeAppData(mAppData);
  }

  operator nsXREAppData*() const { return mAppData; }
  nsXREAppData* operator -> () const { return mAppData; }

private:
  nsXREAppData* mAppData;
};

int main(int argc, char* argv[])
{
  char exePath[MAXPATHLEN];
  nsresult rv = mozilla::BinaryPath::Get(argv[0], exePath);
  if (NS_FAILED(rv)) {
    Output(true, "Couldn't calculate the application directory.\n");
    return 255;
  }

  char *lastSlash = strrchr(exePath, XPCOM_FILE_PATH_SEPARATOR[0]);
  if (!lastSlash || (size_t(lastSlash - exePath) > MAXPATHLEN - sizeof(XPCOM_DLL) - 1))
    return 255;

  strcpy(++lastSlash, XPCOM_DLL);

  rv = XPCOMGlueStartup(exePath);
  if (NS_FAILED(rv)) {
    Output(true, "Couldn't load XPCOM.\n");
    return 255;
  }

  ScopedLogging log;

  if (argc > 1 && (IsArg(argv[1], "h") ||
                   IsArg(argv[1], "help") ||
                   IsArg(argv[1], "?")))
  {
    Usage(argv[0]);
    return 0;
  }

  if (argc == 2 && (IsArg(argv[1], "v") || IsArg(argv[1], "version")))
  {
    nsAutoCString milestone;
    nsAutoCString version;
    GetGREVersion(argv[0], &milestone, &version);
    Output(false, "Mozilla XULRunner %s - %s\n",
           milestone.get(), version.get());
    return 0;
  }

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    Output(true, "Couldn't load XRE functions.\n");
    return 255;
  }

  if (argc > 1) {
    nsAutoCString milestone;
    rv = GetGREVersion(argv[0], &milestone, nullptr);
    if (NS_FAILED(rv))
      return 2;

    if (IsArg(argv[1], "gre-version")) {
      if (argc != 2) {
        Usage(argv[0]);
        return 1;
      }

      printf("%s\n", milestone.get());
      return 0;
    }

    if (IsArg(argv[1], "install-app")) {
      Output(true, "--install-app support has been removed.  Use 'python install-app.py' instead.\n");
      return 1;
    }
  }

  const char *appDataFile = getenv("XUL_APP_FILE");

  if (!(appDataFile && *appDataFile)) {
    if (argc < 2) {
      Usage(argv[0]);
      return 1;
    }

    if (IsArg(argv[1], "app")) {
      if (argc == 2) {
        Usage(argv[0]);
        return 1;
      }
      argv[1] = argv[0];
      ++argv;
      --argc;
    }

    appDataFile = argv[1];
    argv[1] = argv[0];
    ++argv;
    --argc;

    static char kAppEnv[MAXPATHLEN];
    snprintf(kAppEnv, MAXPATHLEN, "XUL_APP_FILE=%s", appDataFile);
    putenv(kAppEnv);
  }

  nsCOMPtr<nsIFile> appDataLF;
  rv = XRE_GetFileFromPath(appDataFile, getter_AddRefs(appDataLF));
  if (NS_FAILED(rv)) {
    Output(true, "Error: unrecognized application.ini path.\n");
    return 2;
  }

  AutoAppData appData(appDataLF);
  if (!appData) {
    Output(true, "Error: couldn't parse application.ini.\n");
    return 2;
  }

  return XRE_main(argc, argv, appData, 0);
}
