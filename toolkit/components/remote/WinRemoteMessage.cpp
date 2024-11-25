/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCommandLine.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/Try.h"
#include "WinRemoteMessage.h"

using namespace mozilla;

#define MOZ_MAGIC_COPYDATA_PREFIX "🔥🦊"

WinRemoteMessageSender::WinRemoteMessageSender(int32_t aArgc,
                                               const char** aArgv,
                                               const nsAString& aWorkingDir)
    : mData({static_cast<DWORD>(
          WinRemoteMessageVersion::NullSeparatedArguments)}) {
  mCmdLineBuffer.AppendLiteral(MOZ_MAGIC_COPYDATA_PREFIX "\0");
  AppendUTF16toUTF8(aWorkingDir, mCmdLineBuffer);
  mCmdLineBuffer.Append('\0');

  for (int32_t i = 0; i < aArgc; i++) {
    mCmdLineBuffer.Append(aArgv[i]);
    mCmdLineBuffer.Append('\0');
  }

  char* mutableBuffer;
  mData.cbData = mCmdLineBuffer.GetMutableData(&mutableBuffer);
  mData.lpData = mutableBuffer;
}

COPYDATASTRUCT* WinRemoteMessageSender::CopyData() { return &mData; }

nsresult WinRemoteMessageReceiver::ParseV2(const nsAString& aBuffer) {
  CommandLineParserWin<char16_t> parser;
  size_t cch = parser.HandleCommandLine(aBuffer);
  ++cch;  // skip a null char

  nsCOMPtr<nsIFile> workingDir;
  if (cch < aBuffer.Length()) {
    MOZ_TRY(
        NS_NewLocalFile(Substring(aBuffer, cch), getter_AddRefs(workingDir)));
  }

  int argc = parser.Argc();
  Vector<nsAutoCString> utf8args;
  if (!utf8args.reserve(argc)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  UniquePtr<const char*[]> argv(new const char*[argc]);
  for (int i = 0; i < argc; ++i) {
    utf8args.infallibleAppend(NS_ConvertUTF16toUTF8(parser.Argv()[i]));
    argv[i] = utf8args[i].get();
  }

  mCommandLine = new nsCommandLine();
  return mCommandLine->Init(argc, argv.get(), workingDir,
                            nsICommandLine::STATE_REMOTE_AUTO);
}

nsresult WinRemoteMessageReceiver::ParseV3(const nsACString& aBuffer) {
  nsCOMPtr<nsIFile> workingDir;

  // String should start with the magic sequence followed by null
  int32_t nextNul = aBuffer.FindChar('\0');
  if (nextNul < 0) {
    return NS_ERROR_FAILURE;
  }

  if (!Substring(aBuffer, 0, nextNul).Equals(MOZ_MAGIC_COPYDATA_PREFIX)) {
    return NS_ERROR_FAILURE;
  }

  int32_t pos = nextNul + 1;
  nextNul = aBuffer.FindChar('\0', pos);
  if (nextNul < 0) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_NewUTF8LocalFile(Substring(aBuffer, pos, nextNul - pos),
                                    getter_AddRefs(workingDir));
  NS_ENSURE_SUCCESS(rv, rv);

  pos = nextNul + 1;
  nsTArray<const char*> argv;

  while (true) {
    nextNul = aBuffer.FindChar('\0', pos);
    if (nextNul < 0) {
      break;
    }

    // Because each argument is null terminated we can just add the pointer to
    // the array directly.
    argv.AppendElement(aBuffer.BeginReading() + pos);
    pos = nextNul + 1;
  }

  // There should always be at least one argument, the path to the binary.
  if (argv.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  mCommandLine = new nsCommandLine();
  return mCommandLine->Init(argv.Length(), argv.Elements(), workingDir,
                            nsICommandLine::STATE_REMOTE_AUTO);
}

nsresult WinRemoteMessageReceiver::Parse(const COPYDATASTRUCT* aMessageData) {
  switch (static_cast<WinRemoteMessageVersion>(aMessageData->dwData)) {
    case WinRemoteMessageVersion::CommandLineAndWorkingDirInUtf16:
      return ParseV2(
          nsDependentSubstring(reinterpret_cast<wchar_t*>(aMessageData->lpData),
                               aMessageData->cbData / sizeof(char16_t)));
    case WinRemoteMessageVersion::NullSeparatedArguments:
      return ParseV3(nsDependentCSubstring(
          reinterpret_cast<char*>(aMessageData->lpData), aMessageData->cbData));
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported message version");
      return NS_ERROR_FAILURE;
  }
}

nsICommandLineRunner* WinRemoteMessageReceiver::CommandLineRunner() {
  return mCommandLine;
}

#undef MOZ_MAGIC_COPYDATA_PREFIX
