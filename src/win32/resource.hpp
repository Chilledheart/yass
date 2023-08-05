// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */
// Used by yass.rc

#ifndef YASS_WIN32_RESOURCE_H
#define YASS_WIN32_RESOURCE_H

// (from ntdef.h)
#define LANG_ENGLISH 0x09
#define SUBLANG_ENGLISH_US 0x01  // English (USA)

// (from winres.h)
#ifdef IDC_STATIC
#undef IDC_STATIC
#endif
#define IDC_STATIC      (-1)

/////////////////////////////////////////////////////////////////////////////
// Standard app configurable strings

// for application title (defaults to EXE name or name in constructor)
#define IDS_APP_TITLE 0xE000
// idle message bar line
#define IDS_IDLEMESSAGE 0xE001
// message bar line when in shift-F1 help mode
#define IDS_HELPMODEMESSAGE 0xE002
// document title when editing OLE embedding
#define IDS_APP_TITLE_EMBEDDING 0xE003
// company name
#define IDS_COMPANY_NAME 0xE004
// object name when server is inplace
#define IDS_OBJ_TITLE_INPLACE 0xE005
// Application User Model ID
#define IDS_APP_ID 0xE006
// Help and App commands
#define ID_APP_ABOUT 0xE140
#define ID_APP_EXIT 0xE141

#define WMAPP_NOTIFYCALLBACK (WM_APP+1)

#define IDD_ABOUTBOX 100
#define IDS_ABOUTBOX 101
#define ID_APP_OPTION 0x0020
#define IDD_OPTIONBOX 200
#define IDS_OPTIONBOX 201
#define IDR_MAINFRAME 300
#define IDI_APPICON 400
#define IDI_TRAYICON 401

#define IDC_START 1000
#define IDC_STOP 1001
#define IDC_EDIT_SERVER_HOST 1002
#define IDC_EDIT_SERVER_PORT 1003
#define IDC_EDIT_USERNAME 1004
#define IDC_EDIT_PASSWORD 1005
#define IDC_COMBOBOX_METHOD 1006
#define IDC_EDIT_LOCAL_HOST 1007
#define IDC_EDIT_LOCAL_PORT 1008
#define IDC_EDIT_TIMEOUT 1009
#define IDC_AUTOSTART_CHECKBOX 1010
#define IDC_SYSTEMPROXY_CHECKBOX 1011

#define IDC_CHECKBOX_TCP_KEEP_ALIVE 1025
#define IDC_EDIT_TCP_KEEP_ALIVE_TIMEOUT 1026
#define IDC_EDIT_TCP_KEEP_ALIVE_INTERVAL 1027

#define IDC_YASS 2000
#define IDC_CONTEXTMENU 2500

#define ID_APP_MSG 128

#define IDT_UPDATE_STATUS_BAR 3000

#define IDR_CA_BUNDLE 4000

#endif  // YASS_WIN32_RESOURCE_H
