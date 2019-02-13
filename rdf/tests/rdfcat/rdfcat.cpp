/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A simple test program that reads in RDF/XML into an in-memory data
  source, then uses the RDF/XML serialization API to write an
  equivalent (but not identical) RDF/XML file back to stdout.

  The program takes a single parameter: the URL from which to read.

 */

#include <stdio.h>
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "nsThreadUtils.h"
#include "plstr.h"
#include "prio.h"
#include "prthread.h"

////////////////////////////////////////////////////////////////////////
// CIDs

// rdf
static NS_DEFINE_CID(kRDFXMLDataSourceCID,  NS_RDFXMLDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// Blatantly stolen from netwerk/test/
#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(">>> %s failed: rv=%x\n", step, static_cast<uint32_t>(rv)); \
        return 1;\
    } \
    PR_END_MACRO

////////////////////////////////////////////////////////////////////////

class ConsoleOutputStreamImpl : public nsIOutputStream
{
protected:
    virtual ~ConsoleOutputStreamImpl(void) {}

public:
    ConsoleOutputStreamImpl(void) {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIOutputStream interface
    NS_IMETHOD Close(void) override {
        return NS_OK;
    }

    NS_IMETHOD Write(const char* aBuf, uint32_t aCount, uint32_t *aWriteCount) override {
        PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf, aCount);
        *aWriteCount = aCount;
        return NS_OK;
    }

    NS_IMETHOD
    WriteFrom(nsIInputStream *inStr, uint32_t count, uint32_t *_retval) override {
        NS_NOTREACHED("WriteFrom");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    WriteSegments(nsReadSegmentFun reader, void * closure, uint32_t count, uint32_t *_retval) override {
        NS_NOTREACHED("WriteSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    IsNonBlocking(bool *aNonBlocking) override {
        NS_NOTREACHED("IsNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush(void) override {
        PR_Sync(PR_GetSpecialFD(PR_StandardOutput));
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(ConsoleOutputStreamImpl, nsIOutputStream)

////////////////////////////////////////////////////////////////////////

int
main(int argc, char** argv)
{
    nsresult rv;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <url>\n", argv[0]);
        return 1;
    }

    NS_InitXPCOM2(nullptr, nullptr, nullptr);

    // Create a stream data source and initialize it on argv[1], which
    // is hopefully a "file:" URL.
    nsCOMPtr<nsIRDFDataSource> ds = do_CreateInstance(kRDFXMLDataSourceCID,
                                                      &rv);
    RETURN_IF_FAILED(rv, "RDF/XML datasource creation");

    nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(ds, &rv);
    RETURN_IF_FAILED(rv, "QI to nsIRDFRemoteDataSource");

    rv = remote->Init(argv[1]);
    RETURN_IF_FAILED(rv, "datasource initialization");

    // Okay, this should load the XML file...
    rv = remote->Refresh(false);
    RETURN_IF_FAILED(rv, "datasource refresh");

    // Pump events until the load is finished
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
    bool done = false;
    while (!done) {
        NS_ENSURE_TRUE(NS_ProcessNextEvent(thread), 1);
        remote->GetLoaded(&done);
    }

    // And this should write it back out. The do_QI() on the pointer
    // is a hack to make sure that the new object gets AddRef()-ed.
    nsCOMPtr<nsIOutputStream> out =
        do_QueryInterface(new ConsoleOutputStreamImpl, &rv);
    RETURN_IF_FAILED(rv, "creation of console output stream");

    nsCOMPtr<nsIRDFXMLSource> source = do_QueryInterface(ds);
    RETURN_IF_FAILED(rv, "QI to nsIRDFXMLSource");

    rv = source->Serialize(out);
    RETURN_IF_FAILED(rv, "datasoure serialization");

    return 0;
}
