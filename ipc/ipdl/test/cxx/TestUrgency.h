#ifndef mozilla__ipdltest_TestUrgency_h
#define mozilla__ipdltest_TestUrgency_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestUrgencyParent.h"
#include "mozilla/_ipdltest/PTestUrgencyChild.h"

namespace mozilla {
namespace _ipdltest {


class TestUrgencyParent :
    public PTestUrgencyParent
{
public:
    TestUrgencyParent();
    virtual ~TestUrgencyParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

    bool RecvTest1(uint32_t *value);
    bool RecvTest2();
    bool RecvTest3(uint32_t *value);
    bool RecvTest4_Begin();
    bool RecvTest4_NestedSync();
    bool RecvFinalTest_Begin();

    bool ShouldContinueFromReplyTimeout() override
    {
      return false;
    }
    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        passed("ok");
        QuitParent();
    }

private:
    bool inreply_;
};


class TestUrgencyChild :
    public PTestUrgencyChild
{
public:
    TestUrgencyChild();
    virtual ~TestUrgencyChild();

    bool RecvStart();
    bool RecvReply1(uint32_t *reply);
    bool RecvReply2(uint32_t *reply);

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        QuitChild();
    }

private:
    uint32_t test_;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestUrgency_h
