/*****************************************************************************
 * clipuni.c                                                                 *
 *                                                                           *
 * CLIPUNI is a very simple program that demonstrates how to implement       *
 * support for the "text/unicode" clipboard format used by Warpzilla.  This  *
 * is quite easy to do using the Unicode API, since the clipboard data is    *
 * really just a UCS-2 string).  This program handles it as follows:         *
 *   - When pasting text that was copied from a program that supports this   *
 *     clipboard format, it will be converted into the current codepage.     *
 *   - When copying text, it will be converted from the current codepage     *
 *     into UCS-2 (Unicode).                                                 *
 * No other functionality (such as loading, saving or printing) is provided. *
 *                                                                           *
 * CLIPUNI, along with its source code, is hereby placed into the public     *
 * domain.  It may freely be used for any purpose, commercial or otherwise.  *
 *                                                                           *
 * PM API USED:                                                              *
 *   WinInitialize, WinCreateMsgQueue, WinRegisterClass, WinCreateStdWindow, *
 *   WinCreateWindow, WinSubclassWindow, WinSetPresParam,                    *
 *   WinQuerySystemAtomTable, WinAddAtom, WinDeleteAtom,                     *
 *   WinGetMsg, WinDispatchMsg, WinDestroyWindow, WinDestroyMsgQueue,        *
 *   WinTerminate, WinDefWindowProc, WinMessageBox, WinPostMsg,              *
 *   WinSendDlgItemMsg, WinBeginPaint, WinQueryWindowRect, WinFillRect,      *
 *   WinSetWindowPos, WinEndPaint, WinWindowFromID, WinSendMsg,              *
 *   WinOpenClipbrd, WinQueryClipbrdData, WinCloseClipbrd,                  *
 *   WinEmptyClipbrd, WinSetClipbrdData, WinQueryCp, WinGetLastError         *
 *                                                                           *
 * HISTORY:                                                                  *
 *   Original: Alex Taylor (public domain)                                   *
 *   2026-07-27  Martin Iturbide  Reorganized to src/, added dual GCC/OW     *
 *               build, fixed compiler warnings for GCC 9.2 and OW 2.0      *
 *****************************************************************************/

#define INCL_GPI
#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uconv.h>
#include "ids.h"


// CONSTANTS
//
#define MAX_CP_NAME     12      // maximum length of a codepage name
#define MAX_CP_SPEC     64      // maximum length of a UconvObject codepage specifier
#define MAX_ERROR       256     // maximum length of an error popup message

// MACROS
//
#define ErrorPopup( text ) \
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, (PCSZ)(text), (PCSZ)"Error", 0, MB_OK | MB_ERROR )


// FUNCTION DECLARATIONS
//
MRESULT EXPENTRY ClientWndProc( HWND, ULONG, MPARAM, MPARAM );
MRESULT EXPENTRY SubMLEProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT          PaintClient( HWND );
ULONG            DoPaste( HWND hwndMLE );
ULONG            DoCopyCut( HWND hwndMLE, BOOL fCut );


// GLOBAL VARIABLES
//
HAB   hab;                      // anchor-block handle
HMQ   hmq;                      // message queue handle
ATOM  cf_Unicode;               // atom for "text/unicode" clipboard format
PFNWP pfnMLE;                   // default MLE window procedure


/* ------------------------------------------------------------------------- *
 * Main program.                                                             *
 * ------------------------------------------------------------------------- */
int main( void )
{
    QMSG     qmsg;                      // message queue
    CHAR     szClass[] = "ClipUni";     // window class name
    HWND     hwndFrame,                 // handle to the frame
             hwndClient,                // handle to the client area
             hwndMLE;                   // handle to the MLE control
    HATOMTBL hSATbl;                    // handle to system atom table
    ULONG    flStyle = FCF_STANDARD;    // main window style

    hab = WinInitialize( 0 );
    hmq = WinCreateMsgQueue( hab, 0 );

    // Create the UI controls
    if ( ! WinRegisterClass( hab, (PCSZ)szClass, ClientWndProc, CS_SIZEREDRAW, 0 )) return ( 1 );
    hwndFrame = WinCreateStdWindow( HWND_DESKTOP, WS_VISIBLE, &flStyle,
                                    (PCSZ)szClass, (PCSZ)"Unicode Clipboard Demonstration",
                                    0L, NULLHANDLE, ID_MAIN, &hwndClient       );
    if ( ! hwndFrame ) return ( 1 );

    hwndMLE = WinCreateWindow( hwndClient, WC_MLE, (PCSZ)"",
                               MLS_BORDER | MLS_VSCROLL | MLS_WORDWRAP | WS_VISIBLE,
                               0, 0, 0, 0, hwndClient, HWND_TOP, IDD_MLE, NULL, NULL );
    if ( ! hwndMLE ) return ( 1 );
    pfnMLE = WinSubclassWindow( hwndMLE, SubMLEProc );
    WinSetPresParam( hwndMLE, PP_FONTNAMESIZE,
                     (ULONG)strlen("10.Monotype Sans Duospace WT J"),
                     (PVOID) "10.Monotype Sans Duospace WT J");

    // Register the Unicode clipboard format
    hSATbl     = WinQuerySystemAtomTable();
    cf_Unicode = WinAddAtom( hSATbl, (PCSZ)"text/unicode");

    // Main program loop
    while ( WinGetMsg( hab, &qmsg, 0, 0, 0 )) WinDispatchMsg( hab, &qmsg );

    // Deregister the Unicode clipboard format
    WinDeleteAtom( hSATbl, cf_Unicode );

    // Final clean-up
    WinDestroyWindow( hwndFrame );
    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    return ( 0 );
}


/* ------------------------------------------------------------------------- *
 * ClientWndProc                                                             *
 *                                                                           *
 * Main client window procedure.  Refer to the PM documentation for argument *
 * and return value descriptions.                                            *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY ClientWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg ) {

        case WM_CREATE:
            break;

        case WM_PAINT:
            PaintClient( hwnd );
            break;

        case WM_COMMAND:
            switch( SHORT1FROMMP( mp1 )) {

                // "Edit" menu commands...
                //
                case IDM_PASTE:
                    WinSendDlgItemMsg( hwnd, IDD_MLE, MLM_PASTE, (MPARAM)0, 0 );
                    return (MRESULT) 0;

                case IDM_CUT:
                    WinSendDlgItemMsg( hwnd, IDD_MLE, MLM_CUT, 0, 0 );
                    return (MRESULT) 0;

                case IDM_COPY:
                    WinSendDlgItemMsg( hwnd, IDD_MLE, MLM_COPY, 0, 0 );
                    return (MRESULT) 0;


                // "File" menu commands...
                //
                case IDM_QUIT:
                    WinPostMsg( hwnd, WM_CLOSE, 0, 0 );
                    return (MRESULT) 0;


                // "Help" menu commands...
                //
                case IDM_ABOUT:
                    WinMessageBox( HWND_DESKTOP, hwnd,
                                   (PCSZ)"Unicode Clipboard Demonstration (v1.01)\n\n\xa9 2007 Alex Taylor\nReleased to the public domain.",
                                   (PCSZ)"Product Information", 0, MB_OK | MB_MOVEABLE | MB_INFORMATION );
                    return (MRESULT) 0;


                default: break;
            }
            break;
    }

    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * SubMLEProc                                                                *
 *                                                                           *
 * A subclass window procedure for the input (source) MLE control.  We use   *
 * this to intercept the default clipboard keyboard triggers, so that we can *
 * call our own functions for handling Unicode text.                         *
 *                                                                           *
 * Refer to the PM documentation for argument and return value descriptions. *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY SubMLEProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    USHORT  usFlags,
            usVK;
    ULONG   ulCopied = 0;


    switch( msg ) {

        case WM_CHAR:
            usFlags = SHORT1FROMMP( mp1 );
            usVK    = SHORT2FROMMP( mp2 );
            if (( usFlags & KC_VIRTUALKEY ) && ( ! (usFlags & KC_KEYUP) )) {
                // Shift+Ins (paste)
                if (( usFlags & KC_SHIFT ) && ( usVK == VK_INSERT )) {
                    ulCopied = DoPaste( hwnd );
                    return (MRESULT) TRUE;
                }
                // Ctrl+Ins (copy)
                if (( usFlags & KC_CTRL ) && ( usVK == VK_INSERT )) {
                    ulCopied = DoCopyCut( hwnd, FALSE );
                    return (MRESULT) TRUE;
                }
                // Shift+Del (cut)
                if (( usFlags & KC_SHIFT ) && ( usVK == VK_DELETE )) {
                    ulCopied = DoCopyCut( hwnd, TRUE );
                    return (MRESULT) TRUE;
                }
            }
            break;

        case MLM_CUT:
            ulCopied = DoCopyCut( hwnd, TRUE );
            return (MRESULT) ulCopied;

        case MLM_COPY:
            ulCopied = DoCopyCut( hwnd, FALSE );
            return (MRESULT) ulCopied;

        case MLM_PASTE:
            ulCopied = DoPaste( hwnd );
            return (MRESULT) ulCopied;

        default: break;
    }

    return (MRESULT) pfnMLE( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * PaintClient                                                               *
 *                                                                           *
 * Handles (re)painting the main client window (i.e. resize the MLE).        *
 * ------------------------------------------------------------------------- */
MRESULT PaintClient( HWND hwnd )
{
    HPS         hps;
    RECTL       rcl;
    LONG        x, y, cx, cy;

    hps = WinBeginPaint( hwnd, NULLHANDLE, NULLHANDLE );
    WinQueryWindowRect( hwnd, &rcl );
    WinFillRect( hps, &rcl, SYSCLR_DIALOGBACKGROUND );

    x  = rcl.xLeft + 1;
    y  = rcl.yBottom + 1;
    cx = rcl.xRight - (2*x);
    cy = rcl.yTop - (2*y);
    WinSetWindowPos( WinWindowFromID(hwnd, IDD_MLE),
                     HWND_TOP, x, y, cx, cy, SWP_SIZE | SWP_MOVE );

    WinEndPaint( hps );
    return ( 0 );
}


/* ------------------------------------------------------------------------- *
 * DoPaste                                                                   *
 *                                                                           *
 * Pastes text from the clipboard into the MLE.  Preference is given to      *
 * clipboard data in the "text/unicode" format; if not available, we use     *
 * plain text (CF_TEXT) instead.                                             *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwndMLE: Handle of the MLE that text is being pasted into.         *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   Number of bytes pasted.                                                 *
 * ------------------------------------------------------------------------- */
ULONG DoPaste( HWND hwndMLE )
{
    UconvObject uconv;                      // conversion object
    UniChar     suCodepage[ MAX_CP_SPEC ],  // conversion specifier
                *psuClipText,               // Unicode text in clipboard
                *puniC;                     // pointer into psuClipText
    CHAR        szError[ MAX_ERROR ];       // buffer for error messages
    char        *pszClipText,               // plain text in clipboard
                *pszLocalText,              // imported text
                *s;                         // pointer into pszLocalText
    ULONG       ulCP,                       // codepage to be used
                ulBufLen,                   // length of output buffer
                ulCopied,                   // number of characters copied
                ulRC;                       // return code


    ulCopied = 0;
    if ( WinOpenClipbrd(hab) ) {

        // Paste as Unicode text if available...
        if (( psuClipText = (UniChar *) WinQueryClipbrdData( hab, cf_Unicode )) != NULL ) {

            ulCP = WinQueryCp( hmq );   // Convert text to the active codepage

            // Create the conversion object
            UniMapCpToUcsCp( ulCP, suCodepage, MAX_CP_NAME );
            UniStrcat( suCodepage, (UniChar *) L"@map=cdra,path=no");

            if (( ulRC = UniCreateUconvObject( suCodepage, &uconv )) == ULS_SUCCESS )
            {
                // (CP850 erroneously maps U+0131 to the euro sign)
                if ( ulCP == 850 )
                    while (( puniC = UniStrchr( psuClipText, 0x0131 )) != NULL ) *puniC = 0xFFFD;

                // Now do the conversion
                ulBufLen = ( UniStrlen(psuClipText) * 4 ) + 1;
                pszLocalText = (char *) malloc( ulBufLen );
                if (( ulRC = UniStrFromUcs( uconv, pszLocalText,
                                            psuClipText, ulBufLen )) == ULS_SUCCESS )
                {
                    // (some codepages use 0x1A for substitutions; replace with ?)
                    while (( s = strchr( pszLocalText, 0x1A )) != NULL ) *s = '?';
                    // Output the converted text
                    WinSendMsg( hwndMLE, MLM_INSERT, MPFROMP(pszLocalText), 0 );
                    ulCopied = strlen( pszLocalText );
                } else {
                    sprintf( szError, "Error pasting Unicode text:\nUniStrFromUcs() = %08lX", ulRC );
                    ErrorPopup( szError );
                }
                UniFreeUconvObject( uconv );
                free( pszLocalText );

            } else {
                sprintf( szError, "Error pasting Unicode text:\nUniCreateUconvObject() = %08lX", ulRC );
                ErrorPopup( szError );
            }
        }

        // ...Plain text otherwise
        else if (( pszClipText = (char *) WinQueryClipbrdData( hab, CF_TEXT )) != NULL ) {
            ulBufLen = strlen(pszClipText) + 1;
            pszLocalText = (char *) malloc( ulBufLen );
            strcpy( pszLocalText, pszClipText );
            WinSendMsg( hwndMLE, MLM_INSERT, MPFROMP(pszLocalText), 0 );
            ulCopied = strlen( pszLocalText );
            free( pszLocalText );
        }

        WinCloseClipbrd( hab );
    }

    return ( ulCopied );
}


/* ------------------------------------------------------------------------- *
 * DoCopyCut                                                                 *
 *                                                                           *
 * Copies or cuts text to the clipboard from the MLE.  The text is copied in *
 * in both plain text (CF_TEXT) and Unicode ("text/unicode") formats.        *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwndMLE: Handle of the MLE that text is being copied from.         *
 *   BOOL fCut   : Indicates if this is a cut rather than a copy operation.  *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   Number of bytes copied.                                                 *
 * ------------------------------------------------------------------------- */
ULONG DoCopyCut( HWND hwndMLE, BOOL fCut )
{
    UconvObject uconv;
    UniChar suCodepage[ MAX_CP_SPEC ],  // conversion specifier
            *psuCopyText,               // Unicode text to be copied
            *psuShareMem,               // Unicode text in clipboard
            *puniC;                     // pointer into psuCopyText
    CHAR    szError[ MAX_ERROR ];       // buffer for error messages
    char    *pszCopyText,               // exported text
            *pszShareMem;               // plain text in clipboard
    ULONG   ulCP,                       // codepage to be used
            ulBufLen,                   // length of output buffer
            ulSelected,                 // number of chars in selection
            ulCopied = 0,               // number of characters copied
            ulRC;                       // return code
    BOOL    fUniCopyFailed = FALSE,     // Unicode copy failed
            fTxtCopyFailed = FALSE;     // plain text copy failed
    IPT     ipt1, ipt2;                 // MLE insertion points (for querying selection)


    // Get the selected text
    ipt1 = LONGFROMMR( WinSendMsg( hwndMLE, MLM_QUERYSEL, MPFROMSHORT(MLFQS_MINSEL), 0 ));
    ipt2 = LONGFROMMR( WinSendMsg( hwndMLE, MLM_QUERYSEL, MPFROMSHORT(MLFQS_MAXSEL), 0 ));
    ulSelected  = (ULONG)LONGFROMMR( WinSendMsg( hwndMLE, MLM_QUERYFORMATTEXTLENGTH, MPFROMLONG(ipt1), MPFROMLONG(ipt2 - ipt1) ));
    pszCopyText = (char *) malloc( ulSelected + 1 );
    ulCopied    = (ULONG)LONGFROMMR( WinSendMsg( hwndMLE, MLM_QUERYSELTEXT, MPFROMP(pszCopyText), 0 ));

    if ( WinOpenClipbrd(hab) ) {

        ulBufLen = ulCopied + 1;
        WinEmptyClipbrd( hab );

        //
        // Copy as Unicode text (converted from the current codepage)
        //

        ulCP = WinQueryCp( hmq );

        // Create the conversion object
        UniMapCpToUcsCp( ulCP, suCodepage, MAX_CP_NAME );
        UniStrcat( suCodepage, (UniChar *) L"@map=cdra,path=no");

        if (( ulRC = UniCreateUconvObject( suCodepage, &uconv )) == ULS_SUCCESS ) {

            // Do the conversion
            psuCopyText = (UniChar *) calloc( ulBufLen, sizeof(UniChar) );
            if (( ulRC = UniStrToUcs( uconv, psuCopyText,
                                      pszCopyText, ulBufLen )) == ULS_SUCCESS )
            {
                // (CP850 erroneously maps U+0131 to the euro sign)
                if ( ulCP == 850 )
                    while (( puniC = UniStrchr( psuCopyText, 0x0131 )) != NULL ) *puniC = 0xFFFD;

                // Place the UCS-2 string on the clipboard as "text/unicode"
                ulRC = DosAllocSharedMem( (PVOID) &psuShareMem, NULL, ulBufLen,
                                          PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE );
                if ( ulRC == 0 ) {
                    UniStrncpy( psuShareMem, psuCopyText, ulBufLen - 1 );
                    if ( ! WinSetClipbrdData( hab, (ULONG) psuShareMem,
                                              cf_Unicode, CFI_POINTER  ))
                    {
                        sprintf( szError, "Error copying Unicode text: WinSetClipbrdData() failed.\nError code: 0x%lX\n", WinGetLastError(hab) );
                        ErrorPopup( szError );
                        fUniCopyFailed = TRUE;
                    }
                } else {
                    sprintf( szError, "Error copying Unicode text.\nDosAllocSharedMem: 0x%lX\n", ulRC );
                    ErrorPopup( szError );
                    fUniCopyFailed = TRUE;
                }
            } else {
                sprintf( szError, "Error copying Unicode text:\nUniStrToUcs() = %08lX", ulRC );
                ErrorPopup( szError );
                fUniCopyFailed = TRUE;
            }

            UniFreeUconvObject( uconv );
            free( psuCopyText );

        } else {
            sprintf( szError, "Error copying Unicode text:\nUniCreateUconvObject() = %08lX", ulRC );
            ErrorPopup( szError );
            fUniCopyFailed = TRUE;
        }


        //
        // Copy as plain text
        //

        ulRC = DosAllocSharedMem( (PVOID) &pszShareMem, NULL, ulBufLen,
                                  PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE );
        if ( ulRC == 0 ) {
            strncpy( pszShareMem, pszCopyText, ulBufLen - 1 );
            if ( ! WinSetClipbrdData( hab, (ULONG) pszShareMem, CF_TEXT, CFI_POINTER )) {
                sprintf( szError, "Error copying plain text: WinSetClipbrdData() failed.\nError code: 0x%lX\n", WinGetLastError(hab) );
                ErrorPopup( szError );
                fTxtCopyFailed = TRUE;
            }
        } else {
            sprintf( szError, "Error copying plain text.\nDosAllocSharedMem: 0x%lX\n", ulRC );
            ErrorPopup( szError );
            fTxtCopyFailed = TRUE;
        }


        //
        // Done copying, now finish up
        //

        if (( fCut ) && ( !fUniCopyFailed ) && ( !fTxtCopyFailed ))
            WinSendMsg( hwndMLE, MLM_CLEAR, 0, 0 );

        WinCloseClipbrd( hab );
    }

    free( pszCopyText );
    if ( fTxtCopyFailed ) ulCopied = 0;

    return ( ulCopied );
}
