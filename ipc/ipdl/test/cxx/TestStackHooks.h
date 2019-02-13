#ifndef mozilla__ipdltest_TestStackHooks_h
#define mozilla__ipdltest_TestStackHooks_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestStackHooksParent.h"
#include "mozilla/_ipdltest/PTestStackHooksChild.h"

namespace mozilla {
namespace _ipdltest {


class TestStackHooksParent :
    public PTestStackHooksParent
{
public:
    TestStackHooksParent();
    virtual ~TestStackHooksParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    virtual bool RecvAsync() override {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    virtual bool RecvSync() override {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    virtual bool AnswerRpc() override {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    virtual bool AnswerStackFrame() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    virtual void EnteredCxxStack() override {
        mOnStack = true;
    }
    virtual void ExitedCxxStack() override {
        mOnStack = false;
    }

    virtual void EnteredCall() override {
        ++mIncallDepth;
    }
    virtual void ExitedCall() override {
        --mIncallDepth;
    }

private:
    bool mOnStack;
    int mIncallDepth;
};


class TestStackHooksChild :
    public PTestStackHooksChild
{
public:
    TestStackHooksChild();
    virtual ~TestStackHooksChild();

    void RunTests();

protected:
    virtual bool RecvStart() override;

    virtual bool AnswerStackFrame() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");

        if (mEntered != mExited)
            fail("unbalanced enter/exit notifications");

        if (mOnStack)
            fail("computing mOnStack went awry; should have failed above assertion");

        QuitChild();
    }

    virtual void EnteredCxxStack() override {
        ++mEntered;
        mOnStack = true;
    }
    virtual void ExitedCxxStack() override {
        ++mExited;
        mOnStack = false;
    }

    virtual void EnteredCall() override {
        ++mIncallDepth;
    }
    virtual void ExitedCall() override {
        --mIncallDepth;
    }

private:
    bool mOnStack;
    int mEntered;
    int mExited;
    int mIncallDepth;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestStackHooks_h
