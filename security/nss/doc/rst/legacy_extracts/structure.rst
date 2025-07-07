.. _mozilla_projects_nss_nss_structure:

`NSS API Structure <#api_structure>`__
======================================

`Libraries <#libraries>`__
--------------------------

.. container::

   NSS compiles into the libraries described below. The Layer indicates the main layer in which the library operates. The Directory is the location of the library
   code in the NSS source tree. The Public Headers is a list of header files that contain types, and
   functions, that are publicly available to higer-level APIs.

   +----------+---------------------+---------------------+---------------+---------------------+
   | Library  | Description         | Layer               | Directory     | Public Headers      |
   +----------+---------------------+---------------------+---------------+---------------------+
   | certdb   | Provides all        | Low Cert            | lib/certdb    | cdbhdl.h, certdb.h, |
   |          | certificate         |                     |               | cert.h, certt.h     |
   |          | handling functions  |                     |               |                     |
   |          | and types. The      |                     |               |                     |
   |          | certdb library      |                     |               |                     |
   |          | manipulates the     |                     |               |                     |
   |          | certificate         |                     |               |                     |
   |          | database (add,      |                     |               |                     |
   |          | create, delete      |                     |               |                     |
   |          | certificates and    |                     |               |                     |
   |          | CRLs). It also      |                     |               |                     |
   |          | provides general    |                     |               |                     |
   |          | c                   |                     |               |                     |
   |          | ertificate-handling |                     |               |                     |
   |          | routines (create a  |                     |               |                     |
   |          | certificate,        |                     |               |                     |
   |          | verify, add/check   |                     |               |                     |
   |          | certificate         |                     |               |                     |
   |          | extensions).        |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | certhi   | Provides high-level | High Cert           | lib/certhigh  | ocsp.h, ocspt.h     |
   |          | certificate-related |                     |               |                     |
   |          | functions, that do  |                     |               |                     |
   |          | not access the      |                     |               |                     |
   |          | certificate         |                     |               |                     |
   |          | database, nor       |                     |               |                     |
   |          | individual          |                     |               |                     |
   |          | certificate data    |                     |               |                     |
   |          | directly.           |                     |               |                     |
   |          | Currently, OCSP     |                     |               |                     |
   |          | checking settings   |                     |               |                     |
   |          | are exported        |                     |               |                     |
   |          | through certhi.     |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | crmf     | Provides functions, | Same Level as SSL   | lib/crmf      | cmmf.h, crmf.h,     |
   |          | and data types, to  |                     |               | crmft.h, cmmft.h,   |
   |          | handle Certificate  |                     |               | crmffut.h           |
   |          | Management Message  |                     |               |                     |
   |          | Format (CMMF) and   |                     |               |                     |
   |          | Certificate Request |                     |               |                     |
   |          | Message Format      |                     |               |                     |
   |          | (CRMF, see `RFC     |                     |               |                     |
   |          | 2511 <https://data  |                     |               |                     |
   |          | tracker.ietf.org/do |                     |               |                     |
   |          | c/html/rfc2511>`__) |                     |               |                     |
   |          | data. CMMF no       |                     |               |                     |
   |          | longer exists as a  |                     |               |                     |
   |          | proposed standard;  |                     |               |                     |
   |          | CMMF functions have |                     |               |                     |
   |          | been incorporated   |                     |               |                     |
   |          | into the proposal   |                     |               |                     |
   |          | for `Certificate    |                     |               |                     |
   |          | Management          |                     |               |                     |
   |          | Protocols           |                     |               |                     |
   |          | (CMP) <https://data |                     |               |                     |
   |          | tracker.ietf.org/do |                     |               |                     |
   |          | c/html/rfc2510>`__. |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | cryptohi | Provides high-level | Sign/Verify         | lib/cryptohi  | cryptohi.h,         |
   |          | cryptographic       |                     |               | cryptoht.h,         |
   |          | support operations: |                     |               | hasht.h, keyhi.h,   |
   |          | such as signing,    |                     |               | keythi.h, key.h,    |
   |          | verifying           |                     |               | keyt.h, sechash.h   |
   |          | signatures, key     |                     |               |                     |
   |          | generation, key     |                     |               |                     |
   |          | manipulation,       |                     |               |                     |
   |          | hashing; and data   |                     |               |                     |
   |          | types. This code is |                     |               |                     |
   |          | above the PKCS #11  |                     |               |                     |
   |          | layer.              |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | fort     | Provides a PKCS #11 | PKCS #11            | lib/fortcrypt | cryptint.h,         |
   |          | interface, to       |                     |               | fmutex.h,           |
   |          | Fortezza crypto     |                     |               | fortsock.h,         |
   |          | services. Fortezza  |                     |               | fpkcs11.h,          |
   |          | is a set of         |                     |               | fpkcs11f.h,         |
   |          | security            |                     |               | fpkcs11t.h,         |
   |          | algorithms, used by |                     |               | fpkmem.h,           |
   |          | the U.S.            |                     |               | fpkstrs.h, genci.h, |
   |          | government. There   |                     |               | maci.h              |
   |          | is also a SWFT      |                     |               |                     |
   |          | library that        |                     |               |                     |
   |          | provides a          |                     |               |                     |
   |          | software-only       |                     |               |                     |
   |          | implementation of a |                     |               |                     |
   |          | PKCS #11 Fortezza   |                     |               |                     |
   |          | token.              |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | freebl   | Provides the API to | Within PKCS #11,    | lib/freebl    | blapi.h, blapit.h   |
   |          | actual              | wraps Crypto        |               |                     |
   |          | cryptographic       |                     |               |                     |
   |          | operations. The     |                     |               |                     |
   |          | freebl is a wrapper |                     |               |                     |
   |          | API. You must       |                     |               |                     |
   |          | supply a library    |                     |               |                     |
   |          | that implements the |                     |               |                     |
   |          | cryptographic       |                     |               |                     |
   |          | operations, such as |                     |               |                     |
   |          | BSAFE from RSA      |                     |               |                     |
   |          | Security. This is   |                     |               |                     |
   |          | also known as the   |                     |               |                     |
   |          | "bottom layer" API, |                     |               |                     |
   |          | or BLAPI.           |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | jar      | Provides support    | Port                | lib/jar       | jar-ds.h, jar.h,    |
   |          | for reading and     |                     |               | jarfile.h           |
   |          | writing data in     |                     |               |                     |
   |          | Java Archive (jar)  |                     |               |                     |
   |          | format, including   |                     |               |                     |
   |          | zlib compression.   |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | nss      | Provides high-level | Above High Cert,    | lib/nss       | nss.h               |
   |          | initialiazation and | High Key            |               |                     |
   |          | shutdown of         |                     |               |                     |
   |          | security services.  |                     |               |                     |
   |          | Specifically, this  |                     |               |                     |
   |          | library provides    |                     |               |                     |
   |          | NSS_Init() for      |                     |               |                     |
   |          | establishing        |                     |               |                     |
   |          | default             |                     |               |                     |
   |          | certificate, key,   |                     |               |                     |
   |          | module databases,   |                     |               |                     |
   |          | and initializing a  |                     |               |                     |
   |          | default random      |                     |               |                     |
   |          | number generator.   |                     |               |                     |
   |          | NSS_Shutdown()      |                     |               |                     |
   |          | closes these        |                     |               |                     |
   |          | databases, to       |                     |               |                     |
   |          | prevent further     |                     |               |                     |
   |          | access by an        |                     |               |                     |
   |          | application.        |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | pk11wrap | Provides access to  | Crypto Wrapper      | lib/pk11wrap  | pk11func.h,         |
   |          | PKCS #11 modules,   |                     |               | secmod.h, secmodt.h |
   |          | through a unified   |                     |               |                     |
   |          | interface. The      |                     |               |                     |
   |          | pkcs11wrap library  |                     |               |                     |
   |          | provides functions  |                     |               |                     |
   |          | for                 |                     |               |                     |
   |          | selecting/finding   |                     |               |                     |
   |          | PKCS #11 modules    |                     |               |                     |
   |          | and slots. It also  |                     |               |                     |
   |          | provides functions  |                     |               |                     |
   |          | that invoke         |                     |               |                     |
   |          | operations in       |                     |               |                     |
   |          | selected modules    |                     |               |                     |
   |          | and slots, such as  |                     |               |                     |
   |          | key selection and   |                     |               |                     |
   |          | generation,         |                     |               |                     |
   |          | signing, encryption |                     |               |                     |
   |          | and decryption,     |                     |               |                     |
   |          | etc.                |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | pkcs12   | Provides functions  | PKCS #12            | lib/pkcs12    | pkcs12t.h,          |
   |          | and types for       |                     |               | pkcs12.h,           |
   |          | encoding and        |                     |               | p12plcy.h, p12.h,   |
   |          | decoding PKCS #12   |                     |               | p12t.h              |
   |          | data. PKCS #12 can  |                     |               |                     |
   |          | be used to encode   |                     |               |                     |
   |          | keys, and           |                     |               |                     |
   |          | certificates, for   |                     |               |                     |
   |          | export or import    |                     |               |                     |
   |          | into other          |                     |               |                     |
   |          | applications.       |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | pkcs7    | Provides functions  | PKCS #7             | lib/pkcs7     | secmime.h,          |
   |          | and types for       |                     |               | secpkcs7.h,         |
   |          | encoding and        |                     |               | pkcs7t.h            |
   |          | decoding encrypted  |                     |               |                     |
   |          | data in PKCS #7     |                     |               |                     |
   |          | format. For         |                     |               |                     |
   |          | example, PKCS #7 is |                     |               |                     |
   |          | used to encrypt     |                     |               |                     |
   |          | certificate data to |                     |               |                     |
   |          | exchange between    |                     |               |                     |
   |          | applications, or to |                     |               |                     |
   |          | encrypt S/MIME      |                     |               |                     |
   |          | message data.       |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | softoken | Provides a software | PKCS #11:           | lib/softoken  | keydbt.h, keylow.h, |
   |          | implementation of a | implementation      |               | keytboth.h,         |
   |          | PKCS #11 module.    |                     |               | keytlow.h,          |
   |          |                     |                     |               | secpkcs5.h,         |
   |          |                     |                     |               | pkcs11.h,           |
   |          |                     |                     |               | pkcs11f.h,          |
   |          |                     |                     |               | pkcs11p.h,          |
   |          |                     |                     |               | pkcs11t.h,          |
   |          |                     |                     |               | pkcs11u.h           |
   +----------+---------------------+---------------------+---------------+---------------------+
   | ssl      | Provides an         | SSL                 | lib/ssl       | ssl.h, sslerr.h,    |
   |          | implementation of   |                     |               | sslproto.h,         |
   |          | the SSL protocol    |                     |               | preenc.h            |
   |          | using NSS and NSPR. |                     |               |                     |
   +----------+---------------------+---------------------+---------------+---------------------+
   | secutil  | Provides utility    | Utility for any     | lib/util      | base64.h,           |
   |          | functions and data  | Layer               |               | ciferfam.h,         |
   |          | types used by other |                     |               | nssb64.h,           |
   |          | libraries. The      |                     |               | nssb64t.h,          |
   |          | library supports    |                     |               | nsslocks.h,         |
   |          | base-64             |                     |               | nssrwlk.h,          |
   |          | encoding/decoding,  |                     |               | nssrwlkt.h,         |
   |          | reader-writer       |                     |               | portreg.h,          |
   |          | locks, the SECItem  |                     |               | pqgutil.h,          |
   |          | data type, DER      |                     |               | secasn1.h,          |
   |          | encoding/decoding,  |                     |               | secasn1t.h,         |
   |          | error types and     |                     |               | seccomon.h,         |
   |          | numbers, OID        |                     |               | secder.h,           |
   |          | handling, and       |                     |               | secdert.h,          |
   |          | secure random       |                     |               | secdig.h,           |
   |          | number generation.  |                     |               | secdigt.h,          |
   |          |                     |                     |               | secitem.h,          |
   |          |                     |                     |               | secoid.h,           |
   |          |                     |                     |               | secoidt.h,          |
   |          |                     |                     |               | secport.h,          |
   |          |                     |                     |               | secrng.h,           |
   |          |                     |                     |               | secrngt.h,          |
   |          |                     |                     |               | secerr.h,           |
   |          |                     |                     |               | watcomfx.h          |
   +----------+---------------------+---------------------+---------------+---------------------+

.. _naming_conventions:

`Naming Conventions <#_naming_conventions>`__
---------------------------------------------

.. container::

   This section describes the rules that (ideally) should be followed for naming and identifying new
   files, functions, and data types.

.. _header_files:

`Header Files <#header_files>`__
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. container::

   | We have a preferred naming system for include files. We had been moving towards one, for some
     time, but for the NSS 3.0 project we finally wrote it down.
   |

   ========================= =========== ===================
   \                         Data Types  Function Prototypes
   Public                    nss____t.h  nss____.h
   Friend (only if required) nss____tf.h nss____f.h
   NSS-private               \____t.h    \____.h
   Module-private            \____tm.h   \____m.h
   ========================= =========== ===================

   The files on the right include the files to their left; the files in a row include the files
   directly above them. Header files always include what they need; the files are protected against
   double inclusion (and even double opening by the compiler).

   .. note::

      Note: It's not necessary all eight files exist. Further, this is a simple ideal, and often
      reality is more complex.

   We would like to keep names to 8.3, even if we no longer support win16. This usually gives us
   four characters to identify a module of NSS.

   In short:

   #. Header files for consumption outside NSS start with "nss."
   #. Header files with types have a trailing "t", header files with prototypes don't.
      "extern" declarations of data also go in the prototypes files.
   #. "Friend" headers are for things that we really wish weren't used by non-NSS code, but which
      are. Those files have a trailing "f," and their use should be deprecated.
   #. "Module" headers are for things used only within a specific subset of NSS; things which would
      have been "static" if we had combined separate C source files together. These header files
      have a trailing "m."

.. _functions_and_types:

`Functions and Types <#functions_and_types>`__
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. container::

   There are a number of ways of doing things in our API, as well as naming decisions for functions
   that can affect the usefulness of our library. If our library is self-consistent with how we
   accomplish these tasks, it makes it easier for the developer to learn how to use our functions.
   This section of the document should grow as we develop our API.

   First some general rules. These rules are derived from existing coding practices inside the
   security library, since consistency is more important than debates about what might look nice.

   #. **Public functions** should have the form LAYER_Body(), where LAYER is an all caps prefix for
      what layer the function lives in, and Body is concatenated English words, where the beginning
      letter of each word is capitalized (also known as
      `CamelCase <https://en.wikipedia.org/wiki/Camel_case>`__). For Example:
      LAYER_CapitalizedEnglishWords() or CERT_DestroyCertificate().
   #. **Data types** and typdefs should have the Form LAYERBody, with the same definitions for LAYER
      as public functions, and Body in camel case English words. For example:
      LAYERCapitalizedEnglishWords or SECKEYPrivateKey.
   #. **Structures** should have the same name as their typedefs, with the string Str added to the
      end. For example LAYERCapitalizedEnglishWordsStr or SECKEYPrivateKeyStr.
   #. **Private functions** should have the form layer_Body(), where layer is the all lower case
      prefix for what layer the function lives in, and Body is camel case English words. Private
      functions include functions that may be "public" in a C sense, but are not exported out of the
      layer. For example: layer_CapitalizedEnglishWords() or pk11_GenerateKeyID().
   #. **Public macros** should have the form LAYER_BODY(), where LAYER is an all caps prefix for
      what layer the macro lives in, and BODY is English words, all in upper case, separated by
      underscores. For example: LAYER_UPPER_CASE_ENGLISH_WORDS() or DER_CONVERT_BIT_STRING().
   #. **Structure members** for exposed data structures should have the form capitalizedEnglishWords
      (the first letter uncapitalized). For example: PK11RSAGenParamsStr.\ **keySizeInBits**
   #. For **members of enums**, our current API has no standard (typedefs for enums should follow
      the Data types standard). There seem to be three reasonable options:

      #. Enum members have the same standard as exposed data structure members.
      #. Enum members have the same standard as data types.
      #. Enum members have the same standard as public macros (minus the '()' of course).

      Options 2 and 3 are the more preferred options. Option 1, currently the most common used for
      enums, actually creates namespace pollution.
   #. **Callback functions**, and functions used in function tables, should have a typedef used to
      define the complete signature of the given function. Function typedefs should have the
      following format: LAYERBody(), with the same definitions for LAYER as public functions, and
      Body is camel case English words. For example: LAYERCapitalizedEnglishWords or
      SECKEYPrivateKey.

.. _opaque_data_structures:

`Opaque Data Structures <#_opaque_data_structures>`__
-----------------------------------------------------

.. container::

   There are many data structures in the security library whose definition is effectively private,
   to the portion of the security library that defines and operates on those data structures.
   External code does not have access to these definitions. The goal here is to increase the
   opaqueness of these structures. This will allow us to modify the size, definition, and format of
   these data structures in future releases, without interfering with the operation of existing
   applications that use the security library.

   The first task is to ensure the data structure definition lives in a private header file, while
   its declaration lives in the public. The current standard in the security library is to typedef
   the data structure name, the easiest way to accomplish this would be to add the typedef to the
   public header file.

   For example, for the structure SECMyOpaqueData you would add:

   .. code::

          typedef struct SECMyOpaqueDataStr SECMyOpaqueData;

   and add the actual structure definition to the private header file. In this same example:

   .. code::

          struct SECMyOpaqueDataStr {
              unsigned long myPrivateData1;
              unsigned long myPrivateData2;
              char *myName;
          };

   the second task is to determine if individual data fields, within the data structure, are part of
   the API.