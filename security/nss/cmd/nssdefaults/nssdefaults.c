/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "nspr.h"
#include "nss.h"
#include "pk11func.h"
#include "secutil.h"
#include "secmod.h"
#include "utilpars.h"

static char *progName;

#define ERR_USAGE 2
#define ERR_UNKNOWN_DB_TYPE 3
#define ERR_UNKNOWN_POLICY 4
#define ERR_GET_POLICY_FAIL 5
#define ERR_UNKNOWN_OPTION 6
#define ERR_GET_OPTION_FAIL 7
#define ERR_INIT_FAILED -1
#define ERR_NO_COMMANDS_FOUND -2

static void
Usage()
{
    PR_fprintf(PR_STDERR,
               "Usage:	 %s [-d certdir] [-P dbprefix] [--dbtype] [-p policy] [-o option] [--system-fips] [-x][-a]\n",
               progName);
    exit(ERR_USAGE);
}

enum {
    opt_CertDir = 0,
    opt_DBPrefix,
    opt_DBType,
    opt_Policy,
    opt_Option,
    opt_SystemFips,
    opt_Fips,
    opt_Hex,
    opt_All,
};

static secuCommandFlag nssdefault_options[] = {
    { /* opt_CertDir        */ 'd', PR_TRUE, 0, PR_FALSE },
    { /* opt_DBPrefix       */ 'P', PR_TRUE, 0, PR_FALSE },
    { /* opt_DBType         */ 'b', PR_FALSE, 0, PR_FALSE, "dbtype" },
    { /* opt_Policy         */ 'p', PR_TRUE, 0, PR_FALSE },
    { /* opt_Option         */ 'o', PR_TRUE, 0, PR_FALSE },
    { /* opt_SystemFips     */ 's', PR_FALSE, 0, PR_FALSE, "system-fips" },
    { /* opt_Fips           */ 'f', PR_FALSE, 0, PR_FALSE, "fips" },
    { /* opt_Hex            */ 'x', PR_FALSE, 0, PR_FALSE },
    { /* opt_All            */ 'a', PR_FALSE, 0, PR_FALSE },
};

void
dump_Raw(char *label, CK_ATTRIBUTE *attr)
{
    int i;
    unsigned char *value = (unsigned char *)attr->pValue;
    printf("0x");
    for (i = 0; i < attr->ulValueLen; i++) {
        printf("%02x", value[i]);
    }
    printf("<%s>\n", label);
}

char *DBTypeName[] = { "None", "sql", "extern", "dbm", "multiaccess" };

int
print_DBType(NSSDBType dbType, PRBool phex)
{
    printf("Default DBType: ");
    if (phex) {
        printf("0x%x\n", dbType);
        return 0;
    }
    if (dbType >= PR_ARRAY_SIZE(DBTypeName)) {
        printf("unknown(%d)\n", dbType);
        return ERR_UNKNOWN_DB_TYPE;
    }
    printf("%s\n", DBTypeName[dbType]);
    return 0;
}

int
print_Bool(const char *label, PRBool val, PRBool phex)
{
    if (phex) {
        printf("%s0x%x\n", label, val);
        return 0;
    }
    printf("%s%s\n", label, val ? "true" : "false");
    return 0;
}

int
print_Policy(const char *policy, PRBool phex, PRBool all)
{
    SECOidTag oid = SECMOD_PolicyStringToOid(policy, "any");
    PRUint32 flags;
    const char *comma = "";
    int i;
    SECStatus rv;

    printf("Policy %s: ", policy);
    if (oid == SEC_OID_UNKNOWN) {
        printf("unknown policy\n");
        return ERR_UNKNOWN_POLICY;
    }
    rv = NSS_GetAlgorithmPolicy(oid, &flags);
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError("policy failed");
        return ERR_GET_POLICY_FAIL;
    }
    if (phex) {
        printf("0x%04x\n", flags);
        return 0;
    }
    if (flags == 0) {
        printf("none\n");
        return 0;
    }
    for (i = 0; i < sizeof(flags) * PR_BITS_PER_BYTE; i++) {
        PRUint32 flag = (1 << i);
        const char *value;
        if ((flags & flag) == 0) {
            continue;
        }
        value = SECMOD_FlagsToPolicyString(flag, PR_TRUE);
        if (value != NULL) {
            printf("%s%s", comma, value);
            comma = ",";
            continue;
        }
        if (all) {
            printf("%sUnused(%04x)", comma, flag);
            comma = ",";
            continue;
        }
    }
    printf("\n");
    return 0;
}

int
print_Option(const char *optionString, PRBool phex)
{
    PRInt32 option = SECMOD_PolicyStringToOpt(optionString);
    PRInt32 value;
    SECStatus rv;

    printf("Option %s: ", optionString);
    if (option == 0) {
        printf("unknown option\n");
        return ERR_UNKNOWN_OPTION;
    }

    rv = NSS_OptionGet(option, &value);
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError("get option failed");
        return ERR_GET_OPTION_FAIL;
    }
    if (phex) {
        printf("0x%04x\n", value);
    } else {
        printf("%d\n", value);
    }
    return 0;
}

int
main(int argc, char **argv)
{
    char *dbprefix = "";
    char *nssdir = NULL;
    SECStatus rv;
    secuCommand nssdefault;
    int local_errno = ERR_NO_COMMANDS_FOUND;
    PRBool phex, all;

    nssdefault.numCommands = 0;
    nssdefault.commands = 0;
    nssdefault.numOptions = PR_ARRAY_SIZE(nssdefault_options);
    nssdefault.options = nssdefault_options;

#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &nssdefault);

    if (rv != SECSuccess) {
        Usage();
    }

    phex = nssdefault.options[opt_Hex].activated;
    all = nssdefault.options[opt_All].activated;

    if (nssdefault.options[opt_CertDir].activated) {
        nssdir = nssdefault.options[opt_CertDir].arg;
    }
    if (nssdefault.options[opt_DBPrefix].activated) {
        dbprefix = nssdefault.options[opt_DBPrefix].arg;
    }

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    if (nssdir == NULL) {
        rv = NSS_NoDB_Init("");
    } else {
        rv = NSS_Initialize(nssdir, dbprefix, dbprefix, "secmod.db", 0);
    }
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError(progName);
        local_errno = ERR_INIT_FAILED;
        goto done;
    }
    /* prints the default db type */
    if (nssdefault.options[opt_DBType].activated) {
        char *appName = NULL;
        NSSDBType dbType = NSS_DB_TYPE_NONE;
        _NSSUTIL_EvaluateConfigDir(nssdir, &dbType, &appName);
        if (appName) {
            PORT_Free(appName);
        }
        local_errno = print_DBType(dbType, phex);
    }
    if (nssdefault.options[opt_SystemFips].activated) {
        local_errno = print_Bool("System FIPS: ", NSS_GetSystemFIPSEnabled(),
                                 phex);
    }
    if (nssdefault.options[opt_Fips].activated) {
        local_errno = print_Bool("FIPS: ", PK11_IsFIPS(), phex);
    }
    if (nssdefault.options[opt_Policy].activated) {
        local_errno = print_Policy(nssdefault.options[opt_Policy].arg,
                                   phex, all);
    }
    if (nssdefault.options[opt_Option].activated) {
        local_errno = print_Option(nssdefault.options[opt_Option].arg, phex);
    }
    if (local_errno == ERR_NO_COMMANDS_FOUND) {
        printf("no data request made\n");
    }

done:
    if (NSS_Shutdown() != SECSuccess) {
        local_errno = 1;
    }
    PL_ArenaFinish();
    PR_Cleanup();
    return local_errno;
}
