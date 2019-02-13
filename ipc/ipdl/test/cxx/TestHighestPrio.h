#ifndef mozilla__ipdltest_TestHighestPrio_h
#define mozilla__ipdltest_TestHighestPrio_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestHighestPrioParent.h"
#include "mozilla/_ipdltest/PTestHighestPrioChild.h"

namespace mozilla {
namespace _ipdltest {


class TestHighestPrioParent :
    public PTestHighestPrioParent
{
public:
    TestHighestPrioParent();
    virtual ~TestHighestPrioParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

    bool RecvMsg1() override;
    bool RecvMsg2() override;
    bool RecvMsg3() override;
    bool RecvMsg4() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        if (msg_num_ != 4)
            fail("missed IPC call");
        passed("ok");
        QuitParent();
    }

private:
    int msg_num_;
};


class TestHighestPrioChild :
    public PTestHighestPrioChild
{
public:
    TestHighestPrioChild();
    virtual ~TestHighestPrioChild();

    bool RecvStart() override;
    bool RecvStartInner() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestHighestPrio_h
