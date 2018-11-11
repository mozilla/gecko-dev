/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#ifndef mozilla__ipdltest_TestEndpointOpens_h
#define mozilla__ipdltest_TestEndpointOpens_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestEndpointOpensParent.h"
#include "mozilla/_ipdltest/PTestEndpointOpensChild.h"

#include "mozilla/_ipdltest2/PTestEndpointOpensOpenedParent.h"
#include "mozilla/_ipdltest2/PTestEndpointOpensOpenedChild.h"

namespace mozilla {

// parent process

namespace _ipdltest {

class TestEndpointOpensParent : public PTestEndpointOpensParent
{
public:
  TestEndpointOpensParent() {}
  virtual ~TestEndpointOpensParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

protected:
  virtual bool RecvStartSubprotocol(mozilla::ipc::Endpoint<PTestEndpointOpensOpenedParent>&& endpoint);

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestEndpointOpensOpenedParent : public PTestEndpointOpensOpenedParent
{
public:
  explicit TestEndpointOpensOpenedParent()
  {}
  virtual ~TestEndpointOpensOpenedParent() {}

protected:
  virtual bool RecvHello() override;
  virtual bool RecvHelloSync() override;
  virtual bool AnswerHelloRpc() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

} // namespace _ipdltest2

// child process

namespace _ipdltest {

class TestEndpointOpensChild : public PTestEndpointOpensChild
{
public:
  TestEndpointOpensChild();
  virtual ~TestEndpointOpensChild() {}

protected:
  virtual bool RecvStart() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestEndpointOpensOpenedChild : public PTestEndpointOpensOpenedChild
{
public:
  explicit TestEndpointOpensOpenedChild()
   : mGotHi(false)
  {}
  virtual ~TestEndpointOpensOpenedChild() {}

protected:
  virtual bool RecvHi() override;
  virtual bool AnswerHiRpc() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool mGotHi;
};

} // namespace _ipdltest2

} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestEndpointOpens_h
