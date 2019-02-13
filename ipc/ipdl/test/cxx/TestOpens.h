#ifndef mozilla__ipdltest_TestOpens_h
#define mozilla__ipdltest_TestOpens_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestOpensParent.h"
#include "mozilla/_ipdltest/PTestOpensChild.h"

#include "mozilla/_ipdltest2/PTestOpensOpenedParent.h"
#include "mozilla/_ipdltest2/PTestOpensOpenedChild.h"

namespace mozilla {

// parent process

namespace _ipdltest {

class TestOpensParent : public PTestOpensParent
{
public:
    TestOpensParent() {}
    virtual ~TestOpensParent() {}

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

protected:
    virtual PTestOpensOpenedParent*
    AllocPTestOpensOpenedParent(Transport* transport, ProcessId otherProcess) override;

    virtual void ActorDestroy(ActorDestroyReason why) override;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestOpensOpenedParent : public PTestOpensOpenedParent
{
public:
    TestOpensOpenedParent(Transport* aTransport)
        : mTransport(aTransport)
    {}
    virtual ~TestOpensOpenedParent() {}

protected:
    virtual bool RecvHello() override;
    virtual bool RecvHelloSync() override;
    virtual bool AnswerHelloRpc() override;

    virtual void ActorDestroy(ActorDestroyReason why) override;

    Transport* mTransport;
};

} // namespace _ipdltest2

// child process

namespace _ipdltest {

class TestOpensChild : public PTestOpensChild
{
public:
    TestOpensChild();
    virtual ~TestOpensChild() {}

protected:
    virtual bool RecvStart() override;

    virtual PTestOpensOpenedChild*
    AllocPTestOpensOpenedChild(Transport* transport, ProcessId otherProcess) override;

    virtual void ActorDestroy(ActorDestroyReason why) override;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestOpensOpenedChild : public PTestOpensOpenedChild
{
public:
    TestOpensOpenedChild(Transport* aTransport)
        : mGotHi(false)
        , mTransport(aTransport)
    {}
    virtual ~TestOpensOpenedChild() {}

protected:
    virtual bool RecvHi() override;
    virtual bool AnswerHiRpc() override;

    virtual void ActorDestroy(ActorDestroyReason why) override;

    bool mGotHi;
    Transport* mTransport;
};

} // namespace _ipdltest2

} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestOpens_h
