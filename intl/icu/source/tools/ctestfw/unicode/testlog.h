// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2004-2010, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

/* Created by grhoten 03/17/2004 */

/* Base class for data driven tests */

#ifndef U_TESTFW_TESTLOG
#define U_TESTFW_TESTLOG

#include <string>
#include <string_view>
#include "unicode/utypes.h"
#include "unicode/testtype.h"

/** Facilitates internal logging of data driven test service 
 *  It would be interesting to develop this into a full      
 *  fledged control system as in Java.                       
 */
class T_CTEST_EXPORT_API TestLog {
public:
    virtual ~TestLog();
    virtual void errln(std::u16string_view message) = 0;
    virtual void logln(std::u16string_view message) = 0;
    virtual void dataerrln(std::u16string_view message) = 0;
    virtual const char* getTestDataPath(UErrorCode& err) = 0;
};

// Note: The IcuTestErrorCode used to be a subclass of ErrorCode, but that made it not usable for
// unit tests that work without U_SHOW_CPLUSPLUS_API.
// So instead we *copy* the ErrorCode API.

class T_CTEST_EXPORT_API IcuTestErrorCode {
public:
    IcuTestErrorCode(const IcuTestErrorCode&) = delete;
    IcuTestErrorCode& operator=(const IcuTestErrorCode&) = delete;

    IcuTestErrorCode(TestLog &callingTestClass, const char *callingTestName);
    virtual ~IcuTestErrorCode();

    // ErrorCode API
    operator UErrorCode & () { return errorCode; }
    operator UErrorCode * () { return &errorCode; }
    UBool isSuccess() const { return U_SUCCESS(errorCode); }
    UBool isFailure() const { return U_FAILURE(errorCode); }
    UErrorCode get() const { return errorCode; }
    void set(UErrorCode value) { errorCode=value; }
    UErrorCode reset();
    void assertSuccess() const;
    const char* errorName() const;

    // Returns true if isFailure().
    UBool errIfFailureAndReset();
    UBool errIfFailureAndReset(const char *fmt, ...);
    UBool errDataIfFailureAndReset();
    UBool errDataIfFailureAndReset(const char *fmt, ...);
    UBool expectErrorAndReset(UErrorCode expectedError);
    UBool expectErrorAndReset(UErrorCode expectedError, const char *fmt, ...);

    /** Sets an additional message string to be appended to failure output. */
    void setScope(const char* message);
    void setScope(std::u16string_view message);

protected:
    virtual void handleFailure() const;

private:
    UErrorCode errorCode;
    TestLog &testClass;
    const char *const testName;
    std::u16string scopeMessage;

    void errlog(UBool dataErr, std::u16string_view mainMessage, const char* extraMessage) const;
};

#endif
