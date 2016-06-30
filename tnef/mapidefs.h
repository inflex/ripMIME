/*
 *  M A P I D E F S . H
 *
 *  Definitions used by MAPI clients and service providers.
 *
 *  Copyright 1986-1996 Microsoft Corporation. All Rights Reserved.
 */

#ifndef MAPIDEFS_H
#define MAPIDEFS_H


/* Array dimension for structures with variable-sized arrays at the end. */

/* Simple data types */


typedef WORD                WCHAR;

#ifdef UNICODE
typedef WCHAR               TCHAR;
#else
typedef char                TCHAR;
#endif

typedef WCHAR *         LPWSTR;
typedef const WCHAR *   LPCWSTR;
typedef TCHAR *         LPTSTR;
typedef const TCHAR *   LPCTSTR;
typedef BYTE *          LPBYTE;

typedef ULONG *         LPULONG;

#ifndef __LHANDLE
#define __LHANDLE
typedef unsigned long   LHANDLE, * LPLHANDLE;
#endif

#if !defined(_WINBASE_) && !defined(_FILETIME_)
#define _FILETIME_
typedef struct _FILETIME
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, * LPFILETIME;
#endif

/*
 *  This flag is used in many different MAPI calls to signify that
 *  the object opened by the call should be modifiable (MAPI_MODIFY).
 *  If the flag MAPI_MAX_ACCESS is set, the object returned should be
 *  returned at the maximum access level allowed.  An additional
 *  property available on the object (PR_ACCESS_LEVEL) uses the same
 *  MAPI_MODIFY flag to say just what this new access level is.
 */

#define MAPI_MODIFY             ((ULONG) 0x00000001)

/*
 *  The following flags are used to indicate to the client what access
 *  level is permissible in the object. They appear in PR_ACCESS in
 *  message and folder objects as well as in contents and associated
 *  contents tables
 */

#define MAPI_ACCESS_MODIFY                  ((ULONG) 0x00000001)
#define MAPI_ACCESS_READ                    ((ULONG) 0x00000002)
#define MAPI_ACCESS_DELETE                  ((ULONG) 0x00000004)
#define MAPI_ACCESS_CREATE_HIERARCHY        ((ULONG) 0x00000008)
#define MAPI_ACCESS_CREATE_CONTENTS         ((ULONG) 0x00000010)
#define MAPI_ACCESS_CREATE_ASSOCIATED       ((ULONG) 0x00000020)

/*
 *  The MAPI_UNICODE flag is used in many different MAPI calls to signify
 *  that strings passed through the interface are in Unicode (a 16-bit
 *  character set). The default is an 8-bit character set.
 *
 *  The value fMapiUnicode can be used as the 'normal' value for
 *  that bit, given the application's default character set.
 */

#define MAPI_UNICODE            ((ULONG) 0x80000000)

#ifdef UNICODE
#define fMapiUnicode            MAPI_UNICODE
#else
#define fMapiUnicode            0
#endif

/* successful HRESULT */
#define hrSuccess               0



/* Recipient types */
#ifndef MAPI_ORIG               /* also defined in mapi.h */
#define MAPI_ORIG   0           /* Recipient is message originator          */
#define MAPI_TO     1           /* Recipient is a primary recipient         */
#define MAPI_CC     2           /* Recipient is a copy recipient            */
#define MAPI_BCC    3           /* Recipient is blind copy recipient        */
#define MAPI_P1     0x10000000  /* Recipient is a P1 resend recipient       */
#define MAPI_SUBMITTED 0x80000000 /* Recipient is already processed         */
/* #define MAPI_AUTHORIZE 4        recipient is a CMC authorizing user      */
/*#define MAPI_DISCRETE 0x10000000 Recipient is a P1 resend recipient       */
#endif

/* Bit definitions for abFlags[0] of ENTRYID */
#define MAPI_SHORTTERM          0x80
#define MAPI_NOTRECIP           0x40
#define MAPI_THISSESSION        0x20
#define MAPI_NOW                0x10
#define MAPI_NOTRESERVED        0x08

/* Bit definitions for abFlags[1] of ENTRYID */
#define MAPI_COMPOUND           0x80

/* ENTRYID */
typedef struct
{
    BYTE    abFlags[4];
    BYTE    ab[MAPI_DIM];
} ENTRYID, *LPENTRYID;

#define CbNewENTRYID(_cb)       (offsetof(ENTRYID,ab) + (_cb))
#define CbENTRYID(_cb)          (offsetof(ENTRYID,ab) + (_cb))

/* Byte-order-independent version of GUID (world-unique identifier) */
typedef struct _MAPIUID
{
    BYTE ab[16];
} MAPIUID, * LPMAPIUID;

/* Note:  need to include C run-times (memory.h) to use this macro */

#define IsEqualMAPIUID(lpuid1, lpuid2)  (!memcmp(lpuid1, lpuid2, sizeof(MAPIUID)))

/*
 * Constants for one-off entry ID:
 * The MAPIUID that identifies the one-off provider;
 * the flag that defines whether the embedded strings are Unicode;
 * the flag that specifies whether the recipient gets TNEF or not.
 */

#define MAPI_ONE_OFF_UID { 0x81, 0x2b, 0x1f, 0xa4, 0xbe, 0xa3, 0x10, 0x19, 0x9d, 0x6e, 0x00, 0xdd, 0x01, 0x0f, 0x54, 0x02 }
#define MAPI_ONE_OFF_UNICODE        0x8000
#define MAPI_ONE_OFF_NO_RICH_INFO   0x0001

/* Object type */

#define MAPI_STORE      ((ULONG) 0x00000001)    /* Message Store */
#define MAPI_ADDRBOOK   ((ULONG) 0x00000002)    /* Address Book */
#define MAPI_FOLDER     ((ULONG) 0x00000003)    /* Folder */
#define MAPI_ABCONT     ((ULONG) 0x00000004)    /* Address Book Container */
#define MAPI_MESSAGE    ((ULONG) 0x00000005)    /* Message */
#define MAPI_MAILUSER   ((ULONG) 0x00000006)    /* Individual Recipient */
#define MAPI_ATTACH     ((ULONG) 0x00000007)    /* Attachment */
#define MAPI_DISTLIST   ((ULONG) 0x00000008)    /* Distribution List Recipient */
#define MAPI_PROFSECT   ((ULONG) 0x00000009)    /* Profile Section */
#define MAPI_STATUS     ((ULONG) 0x0000000A)    /* Status Object */
#define MAPI_SESSION    ((ULONG) 0x0000000B)    /* Session */
#define MAPI_FORMINFO   ((ULONG) 0x0000000C)    /* Form Information */


/*
 *  Maximum length of profile names and passwords, not including
 *  the null termination character.
 */
#ifndef cchProfileNameMax
#define cchProfileNameMax   64
#define cchProfilePassMax   64
#endif


/* Property Types */

#define MV_FLAG         0x1000          /* Multi-value flag */

#define PT_UNSPECIFIED  ((ULONG)  0)    /* (Reserved for interface use) type doesn't matter to caller */
#define PT_NULL         ((ULONG)  1)    /* NULL property value */
#define PT_I2           ((ULONG)  2)    /* Signed 16-bit value */
#define PT_LONG         ((ULONG)  3)    /* Signed 32-bit value */
#define PT_R4           ((ULONG)  4)    /* 4-byte floating point */
#define PT_DOUBLE       ((ULONG)  5)    /* Floating point double */
#define PT_CURRENCY     ((ULONG)  6)    /* Signed 64-bit int (decimal w/    4 digits right of decimal pt) */
#define PT_APPTIME      ((ULONG)  7)    /* Application time */
#define PT_ERROR        ((ULONG) 10)    /* 32-bit error value */
#define PT_BOOLEAN      ((ULONG) 11)    /* 16-bit boolean (non-zero true) */
#define PT_OBJECT       ((ULONG) 13)    /* Embedded object in a property */
#define PT_I8           ((ULONG) 20)    /* 8-byte signed integer */
#define PT_STRING8      ((ULONG) 30)    /* Null terminated 8-bit character string */
#define PT_UNICODE      ((ULONG) 31)    /* Null terminated Unicode string */
#define PT_SYSTIME      ((ULONG) 64)    /* FILETIME 64-bit int w/ number of 100ns periods since Jan 1,1601 */
#define PT_CLSID        ((ULONG) 72)    /* OLE GUID */
#define PT_BINARY       ((ULONG) 258)   /* Uninterpreted (counted byte array) */
/* Changes are likely to these numbers, and to their structures. */

/* Alternate property type names for ease of use */
#define PT_SHORT    PT_I2
#define PT_I4       PT_LONG
#define PT_FLOAT    PT_R4
#define PT_R8       PT_DOUBLE
#define PT_LONGLONG PT_I8

/*
 *  The type of a MAPI-defined string property is indirected, so
 *  that it defaults to Unicode string on a Unicode platform and to
 *  String8 on an ANSI or DBCS platform.
 *
 *  Macros are defined here both for the property type, and for the
 *  field of the property value structure which should be
 *  dereferenced to obtain the string pointer.
 */

#ifdef  UNICODE
#define PT_TSTRING          PT_UNICODE
#define PT_MV_TSTRING       (MV_FLAG|PT_UNICODE)
#define LPSZ                lpszW
#define LPPSZ               lppszW
#define MVSZ                MVszW
#else
#define PT_TSTRING          PT_STRING8
#define PT_MV_TSTRING       (MV_FLAG|PT_STRING8)
#define LPSZ                lpszA
#define LPPSZ               lppszA
#define MVSZ                MVszA
#endif


/* Property Tags
 *
 * By convention, MAPI never uses 0 or FFFF as a property ID.
 * Use as null values, initializers, sentinels, or what have you.
 */

#define PROP_TYPE_MASK          ((ULONG)0x0000FFFF) /* Mask for Property type */
#define PROP_TYPE(ulPropTag)    (((ULONG)(ulPropTag))&PROP_TYPE_MASK)
#define PROP_ID(ulPropTag)      (((ULONG)(ulPropTag))>>16)
#define PROP_TAG(ulPropType,ulPropID)   ((((ULONG)(ulPropID))<<16)|((ULONG)(ulPropType)))
#define PROP_ID_NULL            0
#define PROP_ID_INVALID         0xFFFF
#define PR_NULL                 PROP_TAG( PT_NULL, PROP_ID_NULL)
#if 0
#define CHANGE_PROP_TYPE(ulPropTag, ulPropType) \
                        (((ULONG)0xFFFF0000 & ulPropTag) | ulPropType)
#endif


/* Multi-valued Property Types */

#define PT_MV_I2        (MV_FLAG|PT_I2)
#define PT_MV_LONG      (MV_FLAG|PT_LONG)
#define PT_MV_R4        (MV_FLAG|PT_R4)
#define PT_MV_DOUBLE    (MV_FLAG|PT_DOUBLE)
#define PT_MV_CURRENCY  (MV_FLAG|PT_CURRENCY)
#define PT_MV_APPTIME   (MV_FLAG|PT_APPTIME)
#define PT_MV_SYSTIME   (MV_FLAG|PT_SYSTIME)
#define PT_MV_STRING8   (MV_FLAG|PT_STRING8)
#define PT_MV_BINARY    (MV_FLAG|PT_BINARY)
#define PT_MV_UNICODE   (MV_FLAG|PT_UNICODE)
#define PT_MV_CLSID     (MV_FLAG|PT_CLSID)
#define PT_MV_I8        (MV_FLAG|PT_I8)

/* Alternate property type names for ease of use */
#define PT_MV_SHORT     PT_MV_I2
#define PT_MV_I4        PT_MV_LONG
#define PT_MV_FLOAT     PT_MV_R4
#define PT_MV_R8        PT_MV_DOUBLE
#define PT_MV_LONGLONG  PT_MV_I8

/*
 *  Property type reserved bits
 *
 *  MV_INSTANCE is used as a flag in table operations to request
 *  that a multi-valued property be presented as a single-valued
 *  property appearing in multiple rows.
 */

#define MV_INSTANCE     0x2000
#define MVI_FLAG        (MV_FLAG | MV_INSTANCE)
#define MVI_PROP(tag)   ((tag) | MVI_FLAG)



#endif /* MAPIDEFS_H */
