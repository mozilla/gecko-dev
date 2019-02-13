/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsISimpleEnumerator.h"
#include "nsStringGlue.h"
#include "nsWeakReference.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Attributes.h"

#include <stdio.h>

static nsIObserverService *anObserverService = nullptr;

static void testResult( nsresult rv ) {
    if ( NS_SUCCEEDED( rv ) ) {
        printf("...ok\n");
    } else {
        printf("...failed, rv=0x%x\n", (int)rv);
    }
    return;
}

void printString(nsString &str) {
    printf("%s", NS_ConvertUTF16toUTF8(str).get());
}

class TestObserver final : public nsIObserver,
                           public nsSupportsWeakReference
{
public:
    explicit TestObserver( const nsAString &name )
        : mName( name ) {
    }
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    nsString mName;

private:
    ~TestObserver() {}
};

NS_IMPL_ISUPPORTS( TestObserver, nsIObserver, nsISupportsWeakReference )

NS_IMETHODIMP
TestObserver::Observe( nsISupports     *aSubject,
                       const char *aTopic,
                       const char16_t *someData ) {
    nsCString topic( aTopic );
    nsString data( someData );
    	/*
    		The annoying double-cast below is to work around an annoying bug in
    		the compiler currently used on wensleydale.  This is a test.
    	*/
    printString(mName);
    printf(" has observed something: subject@%p", (void*)aSubject);
    printf(" name=");
    printString(reinterpret_cast<TestObserver*>(reinterpret_cast<void*>(aSubject))->mName);
    printf(" aTopic=%s", topic.get());
    printf(" someData=");
    printString(data);
    printf("\n");
    return NS_OK;
}

int main(int argc, char *argv[])
{
    nsCString topicA; topicA.Assign( "topic-A" );
    nsCString topicB; topicB.Assign( "topic-B" );
    nsresult rv;

    nsresult res = CallCreateInstance("@mozilla.org/observer-service;1", &anObserverService);
	
    if (res == NS_OK) {

        nsIObserver *aObserver = new TestObserver(NS_LITERAL_STRING("Observer-A"));
        aObserver->AddRef();
        nsIObserver *bObserver = new TestObserver(NS_LITERAL_STRING("Observer-B"));
        bObserver->AddRef();
            
        printf("Adding Observer-A as observer of topic-A...\n");
        rv = anObserverService->AddObserver(aObserver, topicA.get(), false);
        testResult(rv);
 
        printf("Adding Observer-B as observer of topic-A...\n");
        rv = anObserverService->AddObserver(bObserver, topicA.get(), false);
        testResult(rv);
 
        printf("Adding Observer-B as observer of topic-B...\n");
        rv = anObserverService->AddObserver(bObserver, topicB.get(), false);
        testResult(rv);

        printf("Testing Notify(observer-A, topic-A)...\n");
        rv = anObserverService->NotifyObservers( aObserver,
                                   topicA.get(),
                                   MOZ_UTF16("Testing Notify(observer-A, topic-A)") );
        testResult(rv);

        printf("Testing Notify(observer-B, topic-B)...\n");
        rv = anObserverService->NotifyObservers( bObserver,
                                   topicB.get(),
                                   MOZ_UTF16("Testing Notify(observer-B, topic-B)") );
        testResult(rv);
 
        printf("Testing EnumerateObserverList (for topic-A)...\n");
        nsCOMPtr<nsISimpleEnumerator> e;
        rv = anObserverService->EnumerateObservers(topicA.get(), getter_AddRefs(e));

        testResult(rv);

        printf("Enumerating observers of topic-A...\n");
        if ( e ) {
          nsCOMPtr<nsIObserver> observer;
          bool loop = true;
          while( NS_SUCCEEDED(e->HasMoreElements(&loop)) && loop) 
          {
              nsCOMPtr<nsISupports> supports;
              e->GetNext(getter_AddRefs(supports));
              observer = do_QueryInterface(supports);
              printf("Calling observe on enumerated observer ");
              printString(reinterpret_cast<TestObserver*>
                                          (reinterpret_cast<void*>(observer.get()))->mName);
              printf("...\n");
              rv = observer->Observe( observer, 
                                      topicA.get(), 
                                      MOZ_UTF16("during enumeration") );
              testResult(rv);
          }
        }
        printf("...done enumerating observers of topic-A\n");

        printf("Removing Observer-A...\n");
        rv = anObserverService->RemoveObserver(aObserver, topicA.get());
        testResult(rv);


        printf("Removing Observer-B (topic-A)...\n");
        rv = anObserverService->RemoveObserver(bObserver, topicB.get());
        testResult(rv);
        printf("Removing Observer-B (topic-B)...\n");
        rv = anObserverService->RemoveObserver(bObserver, topicA.get());
        testResult(rv);
       
    }
    return 0;
}
