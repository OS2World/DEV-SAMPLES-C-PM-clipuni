DEV-SAMPLES-C-PM-clipuni
========================

CLIPUNI demonstrates how to implement support for the `text/unicode` clipboard
format used by Warpzilla (Mozilla for OS/2).  It converts between the current
codepage and UCS-2 when pasting or copying text.

## Technique

| Operation | Description |
|-----------|-------------|
| Paste     | Prefers `text/unicode` clipboard format; falls back to `CF_TEXT` |
| Copy/Cut  | Writes both `text/unicode` (UCS-2) and `CF_TEXT` to the clipboard |

Keyboard shortcuts handled by the MLE subclass:

| Key        | Action |
|------------|--------|
| Shift+Ins  | Paste  |
| Ctrl+Ins   | Copy   |
| Shift+Del  | Cut    |

## PM API Used

`WinInitialize`, `WinCreateMsgQueue`, `WinRegisterClass`, `WinCreateStdWindow`,
`WinCreateWindow`, `WinSubclassWindow`, `WinSetPresParam`,
`WinQuerySystemAtomTable`, `WinAddAtom`, `WinDeleteAtom`,
`WinGetMsg`, `WinDispatchMsg`, `WinDestroyWindow`, `WinDestroyMsgQueue`,
`WinTerminate`, `WinDefWindowProc`, `WinMessageBox`, `WinPostMsg`,
`WinSendDlgItemMsg`, `WinBeginPaint`, `WinQueryWindowRect`, `WinFillRect`,
`WinSetWindowPos`, `WinEndPaint`, `WinWindowFromID`, `WinSendMsg`,
`WinOpenClipbrd`, `WinQueryClipbrdData`, `WinCloseClipbrd`,
`WinEmptyClipbrd`, `WinSetClipbrdData`, `WinQueryCp`, `WinGetLastError`

Unicode API: `UniCreateUconvObject`, `UniStrFromUcs`, `UniStrToUcs`,
`UniFreeUconvObject`, `UniMapCpToUcsCp`, `UniStrcat`, `UniStrlen`,
`UniStrchr`, `UniStrncpy`

## Directory Layout

```
DEV-SAMPLES-C-PM-clipuni/
├── src/
│   ├── clipuni.c          main source
│   ├── ids.h              resource IDs
│   ├── clipuni.rc         resources (menu, accelerators, icon)
│   ├── clipuni.ico        application icon
│   ├── clipuni.def        module definition (OpenWatcom)
│   ├── clipuni-gcc.def    module definition (GCC — no STUB/PROTMODE)
│   └── clipuni-ow.lnk     OpenWatcom linker response file
├── makefile-gcc           GNU make file for GCC/kLIBC
├── makefile-ow            wmake file for OpenWatcom 2.0
├── compile-gcc.cmd        build script for GCC (sets EMXOMFLD vars, logs output)
├── compile-ow.cmd         build script for OpenWatcom (logs output)
├── .gitignore
└── README.md
```

Output binaries go into `bin-gcc/` (GCC) or `bin-ow/` (OpenWatcom).

## Build Instructions

### GCC / kLIBC (bitwiseworks)

Run on OS/2 or ArcaOS:

```
compile-gcc.cmd
```

Build log saved to `compile-gcc.log`.  Requires GCC 9.2, `wrc` (OpenWatcom),
and `uconv.lib` on the OS/2 toolkit path.

### OpenWatcom 2.0

```
compile-ow.cmd
```

Build log saved to `compile-ow.log`.  Requires OpenWatcom 2.0 (`wcc386`,
`wlink`, `wrc`).

## License

Public Domain — original author Alex Taylor.

## Authors

- Alex Taylor (original)
- Martin Iturbide (dual GCC/OW build, warning fixes)

## Links

- https://github.com/OS2World/DEV-SAMPLES-C-PM-clipuni
