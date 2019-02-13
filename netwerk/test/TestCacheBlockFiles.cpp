/*
    TestCacheBlockFiles.cpp
*/


#include <stdio.h>
#include <stdlib.h>
#include <utime.h>

#include <Files.h>
#include <Strings.h>
#include <Errors.h>
#include <Resources.h>
#include <Aliases.h>

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsError.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsMemory.h"
#include "nsIComponentRegistrar.h"
#include "nsANSIFileStreams.h"
#include "nsDiskCacheBlockFile.h"

#include "prclist.h"

/**
 *  StressTest()
 */

typedef struct Allocation {
    int32_t     start;
    int32_t     count;
} Allocation;

nsresult
StressTest(nsIFile *  localFile, int32_t  testNumber, bool readWrite)
{
    nsresult  rv = NS_OK;

#define ITERATIONS      1024
#define MAX_ALLOCATIONS 256
    Allocation  block[MAX_ALLOCATIONS];
    int32_t     currentAllocations = 0;
    int32_t     i;
    uint32_t    a;

    char * writeBuf[4];
    char   readBuf[256 * 4];


    if (readWrite) {
        for (i = 0; i < 4; i++) {
            writeBuf[i] = new char[256 * i];
            if (!writeBuf[i]) {
                printf("Test %d: failed - out of memory\n", testNumber);
                rv = NS_ERROR_OUT_OF_MEMORY;
                goto exit;
            }

            memset(writeBuf[i], i, 256 * i);
        }
    }

    nsDiskCacheBlockFile * blockFile = new nsDiskCacheBlockFile;
    if (!blockFile) {
        printf("Test %d failed (unable to allocate nsDiskCacheBlockFile", testNumber);
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto exit;
    }

    rv = blockFile->Open(localFile, 256);
    if (NS_FAILED(rv)) {
        printf("Test %d: failed (Open returned: 0x%.8x)\n", testNumber, rv);
        goto exit;
    }

    i = ITERATIONS;
    while (i > 0) {
        if ((currentAllocations >= MAX_ALLOCATIONS) ||
            ((currentAllocations > 0) && (rand() % 4 == 0))) {
            // deallocate if we've reached the limit, or 25% of the time we have allocations
            a = rand() % currentAllocations;

            if (readWrite) {
                // read verify deallocation
                rv = blockFile->ReadBlocks(readBuf, block[a].start, block[a].count);
                if (NS_FAILED(rv)) {
                    printf("Test %d: failed (ReadBlocks() returned 0x%.8x)\n", testNumber, rv);
                    goto exit;
                }

                // Verify buffer
                for (i = 0; i < 256 * block[a].count; i++) {
                    if (readBuf[i] != block[a].count) {
                        printf("Test %d: failed (verifying buffer 1)\n", testNumber);
                        rv = NS_ERROR_FAILURE;
                        goto exit;
                    }
                }
            }

            rv = blockFile->DeallocateBlocks(block[a].start, block[a].count);
            if (NS_FAILED(rv)) {
                printf("Test %d: failed (DeallocateBlocks() returned %d)\n", testNumber, rv);
                goto exit;
            }

            --currentAllocations;
            if (currentAllocations > 0)
                block[a] = block[currentAllocations];

        } else {
            // allocate blocks
            --i;
            a = currentAllocations++;
            block[a].count = rand() % 4 + 1; // allocate 1 to 4 blocks
            block[a].start = blockFile->AllocateBlocks(block[a].count);
            if (block[a].start < 0) {
                printf("Test %d: failed (AllocateBlocks() failed.)\n", testNumber);
                goto exit;
            }

            if (readWrite) {
                // write buffer
                rv = blockFile->WriteBlocks(writeBuf[block[a].count], block[a].start, block[a].count);
                if (NS_FAILED(rv)) {
                    printf("Test %d: failed (WriteBlocks() returned 0x%.8x)\n",testNumber, rv);
                    goto exit;
                }
            }
        }
    }

    // now deallocate remaining allocations
    i = currentAllocations;
    while (i--) {

        if (readWrite) {
            // read verify deallocation
            rv = blockFile->ReadBlocks(readBuf, block[a].start, block[a].count);
            if (NS_FAILED(rv)) {
                printf("Test %d: failed (ReadBlocks(1) returned 0x%.8x)\n", testNumber, rv);
                goto exit;
            }

            // Verify buffer
            for (i = 0; i < 256 * block[a].count; i++) {
                if (readBuf[i] != block[a].count) {
                    printf("Test %d: failed (verifying buffer 1)\n", testNumber);
                    rv = NS_ERROR_FAILURE;
                    goto exit;
                }
            }
        }

        rv = blockFile->DeallocateBlocks(block[i].start, block[i].count);
        if (NS_FAILED(rv)) {
            printf("Test %d: failed (DeallocateBlocks() returned %d)\n", testNumber, rv);
            goto exit;
        }
    }



exit:
    nsresult rv2 = blockFile->Close();
    if (NS_FAILED(rv2)) {
        printf("Test %d: failed (Close returned: 0x%.8x)\n", testNumber, rv2);
    }

    return rv ? rv : rv2;
}    

/**
 *  main()
 */

int
main(void)
{
//	OSErr	err;
    printf("hello world\n");

    unsigned long now = time(0);
    srand(now);

    nsCOMPtr<nsIFile>       file;
    nsCOMPtr<nsIFile>  localFile;
    nsresult  rv = NS_OK;
    {
        // Start up XPCOM
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

        // Get default directory
        rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                    getter_AddRefs(file));
        if (NS_FAILED(rv)) {
            printf("NS_GetSpecialDirectory() failed : 0x%.8x\n", rv);
            goto exit;
        }
        char * currentDirPath;
        rv = file->GetPath(&currentDirPath);
        if (NS_FAILED(rv)) {
            printf("currentProcessDir->GetPath() failed : 0x%.8x\n", rv);
            goto exit;
        }

        printf("Current Process Directory: %s\n", currentDirPath);


        // Generate name for cache block file
	rv = file->Append("_CACHE_001_");
        if (NS_FAILED(rv)) goto exit;

        // Delete existing file
        rv = file->Delete(false);
        if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) goto exit;

        // Need nsIFile to open
	localFile = do_QueryInterface(file, &rv);
        if (NS_FAILED(rv)) {
            printf("do_QueryInterface(file) failed : 0x%.8x\n", rv);
            goto exit;
        }

        nsDiskCacheBlockFile * blockFile = new nsDiskCacheBlockFile;
        if (!blockFile) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto exit;
        }

        //----------------------------------------------------------------
        //  local variables used in tests
        //----------------------------------------------------------------
        uint32_t bytesWritten = 0;
        int32_t  startBlock;
        int32_t  i = 0;


        //----------------------------------------------------------------
        //  Test 1: Open nonexistent file
        //----------------------------------------------------------------
        rv = blockFile->Open(localFile, 256);
        if (NS_FAILED(rv)) {
            printf("Test 1: failed (Open returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->Close();
        if (NS_FAILED(rv)) {
            printf("Test 1: failed (Close returned: 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 1: passed\n");


        //----------------------------------------------------------------
        //  Test 2: Open existing file (with no allocation)
        //----------------------------------------------------------------
        rv = blockFile->Open(localFile, 256);
        if (NS_FAILED(rv)) {
            printf("Test 2: failed (Open returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->Close();
        if (NS_FAILED(rv)) {
            printf("Test 2: failed (Close returned: 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 2: passed\n");


        //----------------------------------------------------------------
        //  Test 3: Open existing file (bad format) size < kBitMapBytes
        //----------------------------------------------------------------

        // Delete existing file
        rv = localFile->Delete(false);
        if (NS_FAILED(rv)) {
            printf("Test 3 failed (Delete returned: 0x%.8x)\n", rv);
            goto exit;
        }

        // write < kBitMapBytes to file
        nsANSIFileStream * stream = new nsANSIFileStream;
        if (!stream) {
            printf("Test 3 failed (unable to allocate stream\n", rv);
            goto exit;
        }
        NS_ADDREF(stream);
        rv = stream->Open(localFile);
        if (NS_FAILED(rv)) {
            NS_RELEASE(stream);
            printf("Test 3 failed (stream->Open returned: 0x%.8x)\n", rv);
            goto exit;
        }

        bytesWritten = 0;
        rv = stream->Write("Tell me something good.\n", 24, &bytesWritten);
        if (NS_FAILED(rv)) {
            NS_RELEASE(stream);
            printf("Test 3 failed (stream->Write returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = stream->Close();
        if (NS_FAILED(rv)) {
            NS_RELEASE(stream);
            printf("Test 3 failed (stream->Close returned: 0x%.8x)\n", rv);
            goto exit;
        }
        NS_RELEASE(stream);

        rv = blockFile->Open(localFile, 256);
        if (NS_SUCCEEDED(rv)) {
            printf("Test 3: failed (Open erroneously succeeded)\n", rv);

            (void) blockFile->Close();
            goto exit;
        }

        printf("Test 3: passed\n");


        //----------------------------------------------------------------
        //  Test 4: Open nonexistent file (again)
        //----------------------------------------------------------------

        // Delete existing file
        rv = localFile->Delete(false);
        if (NS_FAILED(rv)) {
            printf("Test 4 failed (Delete returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->Open(localFile, 256);
        if (NS_FAILED(rv)) {
            printf("Test 4: failed (Open returned: 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 4: passed\n");


        //----------------------------------------------------------------
        //  Test 5: AllocateBlocks: invalid block count (0, 5)
        //----------------------------------------------------------------


        startBlock = blockFile->AllocateBlocks(0);
        if (startBlock > -1) {
            printf("Test 5: failed (AllocateBlocks(0) erroneously succeeded)\n");
            goto exit;
        }

        startBlock = blockFile->AllocateBlocks(5);
        if (startBlock > -1) {
            printf("Test 5: failed (AllocateBlocks(5) erroneously succeeded)\n");
            goto exit;
        }
        printf("Test 5: passed\n");


        //----------------------------------------------------------------
        //  Test 6: AllocateBlocks: valid block count (1, 2, 3, 4)
        //----------------------------------------------------------------
        startBlock = blockFile->AllocateBlocks(1);
        if (startBlock != 0) {
            printf("Test 6: failed (AllocateBlocks(1) failed)\n");
            goto exit;
        }

        startBlock = blockFile->AllocateBlocks(2);
        if (startBlock != 1) {
            printf("Test 6: failed (AllocateBlocks(2) failed)\n");
            goto exit;
        }

        startBlock = blockFile->AllocateBlocks(3);
        if (startBlock != 4) {
            printf("Test 6: failed (AllocateBlocks(3) failed)\n");
            goto exit;
        }

        startBlock = blockFile->AllocateBlocks(4);
        if (startBlock != 8) {
            printf("Test 6: failed (AllocateBlocks(4) failed)\n");
            goto exit;
        }

        // blocks allocated should be 1220 3330 4444
        printf("Test 6: passed\n");  // but bits could be mis-allocated



        //----------------------------------------------------------------
        //  Test 7: VerifyAllocation
        //----------------------------------------------------------------
        rv = blockFile->VerifyAllocation(0,1);
        if (NS_FAILED(rv)) {
            printf("Test 7: failed (VerifyAllocation(0,1) returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->VerifyAllocation(1,2);
        if (NS_FAILED(rv)) {
            printf("Test 7: failed (VerifyAllocation(1,2) returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->VerifyAllocation(4,3);
        if (NS_FAILED(rv)) {
            printf("Test 7: failed (VerifyAllocation(4,3) returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->VerifyAllocation(8,4);
        if (NS_FAILED(rv)) {
            printf("Test 7: failed (VerifyAllocation(8,4) returned: 0x%.8x)\n", rv);
            goto exit;
        }
        printf("Test 7: passed\n");


        //----------------------------------------------------------------
        //  Test 8: LastBlock
        //----------------------------------------------------------------
        int32_t  lastBlock = blockFile->LastBlock();
        if (lastBlock != 11) {
            printf("Test 8: failed (LastBlock() returned: %d)\n", lastBlock);
            goto exit;
        }
        printf("Test 8: passed\n");


        //----------------------------------------------------------------
        //  Test 9: DeallocateBlocks: bad startBlock ( < 0)
        //----------------------------------------------------------------
        rv = blockFile->DeallocateBlocks(-1, 4);
        if (NS_SUCCEEDED(rv)) {
            printf("Test 9: failed (DeallocateBlocks(-1, 4) erroneously succeeded)\n");
            goto exit;
        }
        printf("Test 9: passed\n");


        //----------------------------------------------------------------
        //  Test 10: DeallocateBlocks: bad numBlocks (0, 5)
        //----------------------------------------------------------------
        rv = blockFile->DeallocateBlocks(0, 0);
        if (NS_SUCCEEDED(rv)) {
            printf("Test 10: failed (DeallocateBlocks(0, 0) erroneously succeeded)\n");
            goto exit;
        }

        rv = blockFile->DeallocateBlocks(0, 5);
        if (NS_SUCCEEDED(rv)) {
            printf("Test 10: failed (DeallocateBlocks(0, 5) erroneously succeeded)\n");
            goto exit;
        }

        printf("Test 10: passed\n");


        //----------------------------------------------------------------
        //  Test 11: DeallocateBlocks: unallocated blocks
        //----------------------------------------------------------------
        rv = blockFile->DeallocateBlocks(12, 1);
        if (NS_SUCCEEDED(rv)) {
            printf("Test 11: failed (DeallocateBlocks(12, 1) erroneously succeeded)\n");
            goto exit;
        }

        printf("Test 11: passed\n");


        //----------------------------------------------------------------
        //  Test 12: DeallocateBlocks: 1, 2, 3, 4 (allocated in Test 6)
        //----------------------------------------------------------------
        rv = blockFile->DeallocateBlocks(0, 1);
        if (NS_FAILED(rv)) {
            printf("Test 12: failed (DeallocateBlocks(12, 1) returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->DeallocateBlocks(1, 2);
        if (NS_FAILED(rv)) {
            printf("Test 12: failed (DeallocateBlocks(1, 2) returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->DeallocateBlocks(4, 3);
        if (NS_FAILED(rv)) {
            printf("Test 12: failed (DeallocateBlocks(4, 3) returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->DeallocateBlocks(8, 4);
        if (NS_FAILED(rv)) {
            printf("Test 12: failed (DeallocateBlocks(8, 4) returned: 0x%.8x)\n", rv);
            goto exit;
        }

        // zero blocks should be allocated
        rv = blockFile->Close();
        if (NS_FAILED(rv)) {
            printf("Test 12: failed (Close returned: 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 12: passed\n");


        //----------------------------------------------------------------
        //  Test 13: Allocate/Deallocate boundary test
        //----------------------------------------------------------------

        rv = blockFile->Open(localFile, 256);
        if (NS_FAILED(rv)) {
            printf("Test 13: failed (Open returned: 0x%.8x)\n", rv);
            goto exit;
        }

        // fully allocate, 1 block at a time
        for (i=0; i< kBitMapBytes * 8; ++i) {
            startBlock = blockFile->AllocateBlocks(1);
            if (startBlock < 0) {
                printf("Test 13: failed (AllocateBlocks(1) failed on i=%d)\n", i);
                goto exit;
            }
        }

        // attempt allocation with full bit map
        startBlock = blockFile->AllocateBlocks(1);
        if (startBlock >= 0) {
            printf("Test 13: failed (AllocateBlocks(1) erroneously succeeded i=%d)\n", i);
            goto exit;
        }

        // deallocate all the bits
        for (i=0; i< kBitMapBytes * 8; ++i) {
            rv = blockFile->DeallocateBlocks(i,1);
            if (NS_FAILED(rv)) {
                printf("Test 13: failed (DeallocateBlocks(%d,1) returned: 0x%.8x)\n", i,rv);
                goto exit;
            }
        }

        // attempt deallocation beyond end of bit map
        rv = blockFile->DeallocateBlocks(i,1);
        if (NS_SUCCEEDED(rv)) {
            printf("Test 13: failed (DeallocateBlocks(%d,1) erroneously succeeded)\n", i);
            goto exit;
        }

        // bit map should be empty

        // fully allocate, 2 block at a time
        for (i=0; i< kBitMapBytes * 8; i+=2) {
            startBlock = blockFile->AllocateBlocks(2);
            if (startBlock < 0) {
                printf("Test 13: failed (AllocateBlocks(2) failed on i=%d)\n", i);
                goto exit;
            }
        }

        // attempt allocation with full bit map
        startBlock = blockFile->AllocateBlocks(2);
        if (startBlock >= 0) {
            printf("Test 13: failed (AllocateBlocks(2) erroneously succeeded i=%d)\n", i);
            goto exit;
        }

        // deallocate all the bits
        for (i=0; i< kBitMapBytes * 8; i+=2) {
            rv = blockFile->DeallocateBlocks(i,2);
            if (NS_FAILED(rv)) {
                printf("Test 13: failed (DeallocateBlocks(%d,2) returned: 0x%.8x)\n", i,rv);
                goto exit;
            }
        }

        // bit map should be empty

        // fully allocate, 4 block at a time
        for (i=0; i< kBitMapBytes * 8; i+=4) {
            startBlock = blockFile->AllocateBlocks(4);
            if (startBlock < 0) {
                printf("Test 13: failed (AllocateBlocks(4) failed on i=%d)\n", i);
                goto exit;
            }
        }

        // attempt allocation with full bit map
        startBlock = blockFile->AllocateBlocks(4);
        if (startBlock >= 0) {
            printf("Test 13: failed (AllocateBlocks(4) erroneously succeeded i=%d)\n", i);
            goto exit;
        }

        // deallocate all the bits
        for (i=0; i< kBitMapBytes * 8; i+=4) {
            rv = blockFile->DeallocateBlocks(i,4);
            if (NS_FAILED(rv)) {
                printf("Test 13: failed (DeallocateBlocks(%d,4) returned: 0x%.8x)\n", i,rv);
                goto exit;
            }
        }

        // bit map should be empty

        // allocate as many triple-blocks as possible
        for (i=0; i< kBitMapBytes * 8; i+=4) {
            startBlock = blockFile->AllocateBlocks(3);
            if (startBlock < 0) {
                printf("Test 13: failed (AllocateBlocks(3) failed on i=%d)\n", i);
                goto exit;
            }
        }

        // attempt allocation with "full" bit map
        startBlock = blockFile->AllocateBlocks(3);
        if (startBlock >= 0) {
            printf("Test 13: failed (AllocateBlocks(3) erroneously succeeded i=%d)\n", i);
            goto exit;
        }

        // leave some blocks allocated

        rv = blockFile->Close();
        if (NS_FAILED(rv)) {
            printf("Test 13: failed (Close returned: 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 13: passed\n");


        //----------------------------------------------------------------
        //  Test 14: ValidateFile (open existing file w/size < allocated blocks
        //----------------------------------------------------------------
        rv = blockFile->Open(localFile, 256);
        if (NS_SUCCEEDED(rv)) {
            printf("Test 14: failed (Open erroneously succeeded)\n");
            goto exit;
        }

        // Delete existing file
        rv = localFile->Delete(false);
        if (NS_FAILED(rv)) {
            printf("Test 14 failed (Delete returned: 0x%.8x)\n", rv);
            goto exit;
        }
        printf("Test 14: passed\n");


        //----------------------------------------------------------------
        //  Test 15: Allocate/Deallocate stress test
        //----------------------------------------------------------------

        rv = StressTest(localFile, 15, false);
        if (NS_FAILED(rv))
            goto exit;

        printf("Test 15: passed\n");


        //----------------------------------------------------------------
        //  Test 16: WriteBlocks
        //----------------------------------------------------------------

        rv = blockFile->Open(localFile, 256);
        if (NS_FAILED(rv)) {
            printf("Test 16: failed (Open returned: 0x%.8x)\n", rv);
            goto exit;
        }

        char * one   = new char[256 * 1];
        char * two   = new char[256 * 2];
        char * three = new char[256 * 3];
        char * four  = new char[256 * 4];
        if (!one || !two || !three || !four) {
            printf("Test 16: failed - out of memory\n");
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto exit;
        }

        memset(one,   1, 256);
        memset(two,   2, 256 * 2);
        memset(three, 3, 256 * 3);
        memset(four,  4, 256 * 4);

        startBlock = blockFile->AllocateBlocks(1);
        if (startBlock != 0) {
            printf("Test 16: failed (AllocateBlocks(1) failed)\n");
            goto exit;
        }

        rv = blockFile->WriteBlocks(one, startBlock, 1);
        if (NS_FAILED(rv)) {
            printf("Test 16: failed (WriteBlocks(1) returned 0x%.8x)\n", rv);
            goto exit;
        }

        startBlock = blockFile->AllocateBlocks(2);
        if (startBlock != 1) {  // starting with empy map, this allocation should begin at block 1
            printf("Test 16: failed (AllocateBlocks(2) failed)\n");
            goto exit;
        }

        rv = blockFile->WriteBlocks(two, startBlock, 2);
        if (NS_FAILED(rv)) {
            printf("Test 16: failed (WriteBlocks(2) returned 0x%.8x)\n", rv);
            goto exit;
        }

        startBlock = blockFile->AllocateBlocks(3);
        if (startBlock != 4) {  // starting with empy map, this allocation should begin at block 4
            printf("Test 16: failed (AllocateBlocks(3) failed)\n");
            goto exit;
        }

        rv = blockFile->WriteBlocks(three, startBlock, 3);
        if (NS_FAILED(rv)) {
            printf("Test 16: failed (WriteBlocks(3) returned 0x%.8x)\n", rv);
            goto exit;
        }

        startBlock = blockFile->AllocateBlocks(4);
        if (startBlock != 8) {  // starting with empy map, this allocation should begin at block 8
            printf("Test 16: failed (AllocateBlocks(4) failed)\n");
            goto exit;
        }

        rv = blockFile->WriteBlocks(four, startBlock, 4);
        if (NS_FAILED(rv)) {
            printf("Test 16: failed (WriteBlocks(4) returned 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 16: passed\n");


        //----------------------------------------------------------------
        //  Test 17: ReadBlocks
        //----------------------------------------------------------------

        rv = blockFile->ReadBlocks(one, 0, 1);
        if (NS_FAILED(rv)) {
            printf("Test 17: failed (ReadBlocks(1) returned 0x%.8x)\n", rv);
            goto exit;
        }

        // Verify buffer
        for (i = 0; i < 256; i++) {
            if (one[i] != 1) {
                printf("Test 17: failed (verifying buffer 1)\n");
                rv = NS_ERROR_FAILURE;
                goto exit;
            }
        }

        rv = blockFile->ReadBlocks(two, 1, 2);
        if (NS_FAILED(rv)) {
            printf("Test 17: failed (ReadBlocks(2) returned 0x%.8x)\n", rv);
            goto exit;
        }

        // Verify buffer
        for (i = 0; i < 256 * 2; i++) {
            if (two[i] != 2) {
                printf("Test 17: failed (verifying buffer 2)\n");
                rv = NS_ERROR_FAILURE;
                goto exit;
            }
        }

        rv = blockFile->ReadBlocks(three, 4, 3);
        if (NS_FAILED(rv)) {
            printf("Test 17: failed (ReadBlocks(3) returned 0x%.8x)\n", rv);
            goto exit;
        }

        // Verify buffer
        for (i = 0; i < 256 * 3; i++) {
            if (three[i] != 3) {
                printf("Test 17: failed (verifying buffer 3)\n");
                rv = NS_ERROR_FAILURE;
                goto exit;
            }
        }

        rv = blockFile->ReadBlocks(four, 8, 4);
        if (NS_FAILED(rv)) {
            printf("Test 17: failed (ReadBlocks(4) returned 0x%.8x)\n", rv);
            goto exit;
        }

        // Verify buffer
        for (i = 0; i < 256 * 4; i++) {
            if (four[i] != 4) {
                printf("Test 17: failed (verifying buffer 4)\n");
                rv = NS_ERROR_FAILURE;
                goto exit;
            }
        }

        rv = blockFile->Close();
        if (NS_FAILED(rv)) {
            printf("Test 17: failed (Close returned: 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 17: passed\n");


        //----------------------------------------------------------------
        //  Test 18: ValidateFile (open existing file with blocks allocated)
        //----------------------------------------------------------------
        rv = blockFile->Open(localFile, 256);
        if (NS_FAILED(rv)) {
            printf("Test 18: failed (Open returned: 0x%.8x)\n", rv);
            goto exit;
        }

        rv = blockFile->Close();
        if (NS_FAILED(rv)) {
            printf("Test 18: failed (Close returned: 0x%.8x)\n", rv);
            goto exit;
        }

        printf("Test 18: passed\n");

        //----------------------------------------------------------------
        //  Test 19: WriteBlocks/ReadBlocks stress
        //----------------------------------------------------------------

        rv = StressTest(localFile, 19, false);
        if (NS_FAILED(rv))
            goto exit;

        printf("Test 19: passed\n");


exit:

        if (currentDirPath)
            free(currentDirPath);
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    if (NS_FAILED(rv))
        printf("Test failed: 0x%.8x\n", rv);

    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    printf("XPCOM shut down.\n\n");
    return 0;
}

