#include "TestBridgeMain.h"

#include "base/task.h"
#include "IPDLUnitTests.h"      // fail etc.
#include "IPDLUnitTestSubprocess.h"

#include "nsAutoPtr.h"

using namespace std;

namespace mozilla {
namespace _ipdltest {


//-----------------------------------------------------------------------------
// main process
void
TestBridgeMainParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

PTestBridgeMainSubParent*
TestBridgeMainParent::AllocPTestBridgeMainSubParent(Transport* transport,
                                                    ProcessId otherPid)
{
    nsAutoPtr<TestBridgeMainSubParent> a(new TestBridgeMainSubParent(transport));
    if (!a->Open(transport, otherPid, XRE_GetIOMessageLoop(), ipc::ParentSide)) {
        return nullptr;
    }
    return a.forget();
}

void
TestBridgeMainParent::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  
    passed("ok");
    QuitParent();
}

bool
TestBridgeMainSubParent::RecvHello()
{
    return SendHi();
}

bool
TestBridgeMainSubParent::RecvHelloSync()
{
    return true;
}

bool
TestBridgeMainSubParent::AnswerHelloRpc()
{
    return CallHiRpc();
}

void
TestBridgeMainSubParent::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        do_AddRef(new DeleteTask<TestBridgeMainSubParent>(this)));
}

//-----------------------------------------------------------------------------
// sub process --- child of main
TestBridgeMainChild* gBridgeMainChild;

TestBridgeMainChild::TestBridgeMainChild()
    : mSubprocess(nullptr)
{
    gBridgeMainChild = this;
}

bool
TestBridgeMainChild::RecvStart()
{
    vector<string> subsubArgs;
    subsubArgs.push_back("TestBridgeSub");

    mSubprocess = new IPDLUnitTestSubprocess();
    if (!mSubprocess->SyncLaunch(subsubArgs))
        fail("problem launching subprocess");

    IPC::Channel* transport = mSubprocess->GetChannel();
    if (!transport)
        fail("no transport");

    TestBridgeSubParent* bsp = new TestBridgeSubParent();
    bsp->Open(transport, base::GetProcId(mSubprocess->GetChildProcessHandle()));

    bsp->Main();
    return true;
}

void
TestBridgeMainChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  
    // NB: this is kosher because QuitChild() joins with the IO thread
    XRE_GetIOMessageLoop()->PostTask(
        do_AddRef(new DeleteTask<IPDLUnitTestSubprocess>(mSubprocess)));
    QuitChild();
}

void
TestBridgeSubParent::Main()
{
    if (!SendPing())
        fail("sending Ping");
}

bool
TestBridgeSubParent::RecvBridgeEm()
{
    if (NS_FAILED(PTestBridgeMainSub::Bridge(gBridgeMainChild, this)))
        fail("bridging Main and Sub");
    return true;
}

void
TestBridgeSubParent::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");
    gBridgeMainChild->Close();

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        do_AddRef(new DeleteTask<TestBridgeSubParent>(this)));
}

//-----------------------------------------------------------------------------
// subsub process --- child of sub

static TestBridgeSubChild* gBridgeSubChild;

TestBridgeSubChild::TestBridgeSubChild()
{
    gBridgeSubChild = this;   
}

bool
TestBridgeSubChild::RecvPing()
{
    if (!SendBridgeEm())
        fail("sending BridgeEm");
    return true;
}

PTestBridgeMainSubChild*
TestBridgeSubChild::AllocPTestBridgeMainSubChild(Transport* transport,
                                                 ProcessId otherPid)
{
    nsAutoPtr<TestBridgeMainSubChild> a(new TestBridgeMainSubChild(transport));
    if (!a->Open(transport, otherPid, XRE_GetIOMessageLoop(), ipc::ChildSide)) {
        return nullptr;
    }

    if (!a->SendHello())
        fail("sending Hello");

    return a.forget();
}

void
TestBridgeSubChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");
    QuitChild();
}

bool
TestBridgeMainSubChild::RecvHi()
{
    if (!SendHelloSync())
        fail("sending HelloSync");
    if (!CallHelloRpc())
        fail("calling HelloRpc");
    if (!mGotHi)
        fail("didn't answer HiRpc");

    // Need to close the channel without message-processing frames on
    // the C++ stack
    MessageLoop::current()->PostTask(
        NewNonOwningRunnableMethod(this, &TestBridgeMainSubChild::Close));
    return true;
}

bool
TestBridgeMainSubChild::AnswerHiRpc()
{
    mGotHi = true;              // d00d
    return true;
}

void
TestBridgeMainSubChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  

    gBridgeSubChild->Close();

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        do_AddRef(new DeleteTask<TestBridgeMainSubChild>(this)));
}

} // namespace mozilla
} // namespace _ipdltest
