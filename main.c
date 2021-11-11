#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <locale.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "include/sqlite3.h"

#define LVS_EX_AUTOSIZECOLUMNS 0x10000000

#define WMU_UPDATE_GRID        WM_USER + 1
#define WMU_UPDATE_DATA        WM_USER + 2
#define WMU_UPDATE_ROW_COUNT   WM_USER + 3
#define WMU_UPDATE_FILTER_SIZE WM_USER + 4
#define WMU_SET_HEADER_FILTERS WM_USER + 5
#define WMU_AUTO_COLUMN_SIZE   WM_USER + 6
#define WMU_RESET_CACHE        WM_USER + 7
#define WMU_SET_FONT           WM_USER + 8
#define WMU_SET_THEME          WM_USER + 9

#define IDC_MAIN               100
#define IDC_TABLELIST          101
#define IDC_GRID               102
#define IDC_STATUSBAR          103
#define IDC_HEADER_EDIT        1000

#define IDM_COPY_CELL          5000
#define IDM_COPY_ROW           5001
#define IDM_FILTER_ROW         5002
#define IDM_DARK_THEME         5003
#define IDM_DDL                5010

#define SB_VERSION             0
#define SB_TABLE_COUNT         1
#define SB_VIEW_COUNT          2
#define SB_TYPE                3
#define SB_ROW_COUNT           4
#define SB_CURRENT_ROW         5
#define SB_AUXILIARY           6

#define SPLITTER_WIDTH         5
#define MAX_TEXT_LENGTH        32000
#define MAX_COLUMN_LENGTH      2000
#define MAX_TABLE_LENGTH       2000

#define APP_NAME               TEXT("sqlite-wlx")
#define APP_VERSION            TEXT("0.9.7")

#define LCS_FINDFIRST          1
#define LCS_MATCHCASE          2
#define LCS_WHOLEWORDS         4
#define LCS_BACKWARDS          8

typedef struct {
	int size;
	DWORD PluginInterfaceVersionLow;
	DWORD PluginInterfaceVersionHi;
	char DefaultIniName[MAX_PATH];
} ListDefaultParamStruct;

static TCHAR iniPath[MAX_PATH] = {0};

LRESULT CALLBACK cbNewMain (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK cbNewHeader(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK cbNewFilterEdit (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK cbHotKey(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void bindFilterValues(HWND hHeader, sqlite3_stmt* stmt);
HWND getMainWindow(HWND hWnd);
void setStoredValue(TCHAR* name, int value);
int getStoredValue(TCHAR* name, int defValue);
TCHAR* getStoredString(TCHAR* name, TCHAR* defValue);
int CALLBACK cbEnumTabStopChildren (HWND hWnd, LPARAM lParam);
TCHAR* utf8to16(const char* in);
char* utf16to8(const TCHAR* in);
int findString(TCHAR* text, TCHAR* word, BOOL isMatchCase, BOOL isWholeWords);
void setClipboardText(const TCHAR* text);
int ListView_AddColumn(HWND hListWnd, TCHAR* colName, int fmt);
int Header_GetItemText(HWND hWnd, int i, TCHAR* pszText, int cchTextMax);
void Menu_SetItemState(HMENU hMenu, UINT wID, UINT fState);

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH && iniPath[0] == 0) {
		TCHAR path[MAX_PATH + 1] = {0};
		GetModuleFileName(hModule, path, MAX_PATH);
		TCHAR* dot = _tcsrchr(path, TEXT('.'));
		_tcsncpy(dot, TEXT(".ini"), 5);
		if (_taccess(path, 0) == 0)
			_tcscpy(iniPath, path);	
	}
	return TRUE;
}

void __stdcall ListGetDetectString(char* DetectString, int maxlen) {
	snprintf(DetectString, maxlen, "MULTIMEDIA & ([0]=\"S\" & [1]=\"Q\" & [2]=\"L\" & [3]=\"i\" & [4]=\"t\" & [5]=\"e\")");
}

void __stdcall ListSetDefaultParams(ListDefaultParamStruct* dps) {
	if (iniPath[0] == 0) {
		DWORD size = MultiByteToWideChar(CP_ACP, 0, dps->DefaultIniName, -1, NULL, 0);
		MultiByteToWideChar(CP_ACP, 0, dps->DefaultIniName, -1, iniPath, size);
	}
}

int __stdcall ListSearchTextW(HWND hWnd, TCHAR* searchString, int searchParameter) {
	HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
	HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);	
	
	TCHAR*** cache = (TCHAR***)GetProp(hWnd, TEXT("CACHE"));
	int rowCount = *(int*)GetProp(hWnd, TEXT("ROWCOUNT"));
	int colCount = Header_GetItemCount(ListView_GetHeader(hGridWnd));
		
	BOOL isBackward = searchParameter & LCS_BACKWARDS;
	BOOL isMatchCase = searchParameter & LCS_MATCHCASE;
	BOOL isWholeWords = searchParameter & LCS_WHOLEWORDS;	
		
	int rowNo = ListView_GetNextItem(hGridWnd, -1, LVNI_SELECTED);
	if (rowNo == -1)
		rowNo = isBackward ? rowCount - 1 : 0;
		
	int* pColNo = (int*)GetProp(hWnd, TEXT("SEARCHCOLNO"));

	int pos = -1;
	do {
		int colNo = *pColNo;
		for (; (pos == -1) && colNo < colCount; colNo++) 
			pos = findString(cache[rowNo][colNo], searchString, isMatchCase, isWholeWords);
		*pColNo = pos != -1 ? colNo - 1 : 0;
		rowNo += (pos == -1) && (isBackward ? -1 : 1); 	
	} while ((pos == -1) && (isBackward ? rowNo > 0 : rowNo < rowCount - 1));

	TCHAR buf[256] = {0};
	if (pos != -1) {
		ListView_EnsureVisible(hGridWnd, rowNo, FALSE);
		ListView_SetItemState(hGridWnd, rowNo, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		
		TCHAR colName[MAX_COLUMN_LENGTH + 1];
		Header_GetItemText(ListView_GetHeader(hGridWnd), *pColNo, colName, MAX_COLUMN_LENGTH);
		TCHAR* val = cache[rowNo][*pColNo];
		int len = _tcslen(searchString);
		_sntprintf(buf, 255, TEXT("%ls: %ls%.*ls%ls"), colName, 
			pos >= 10 ? TEXT("...") : TEXT(""), 
			len + (pos < 10 ? pos : 10) + 10, pos < 10 ? val : val + pos - 10,
			_tcslen(val + pos + len) > 10 ? TEXT("...") : TEXT(""));
		*pColNo += 1;
	} else { 
		MessageBox(hWnd, searchString, TEXT("Not found:"), MB_OK);
	}
	SendMessage(hStatusWnd, SB_SETTEXT, SB_AUXILIARY, (LPARAM)buf);	
	SetFocus(hGridWnd);	

	return 0;
}

int __stdcall ListSearchText(HWND hWnd, char* searchString, int searchParameter) {
	DWORD len = MultiByteToWideChar(CP_ACP, 0, searchString, -1, NULL, 0);
	TCHAR* searchString16 = (TCHAR*)calloc (len, sizeof (TCHAR));
	MultiByteToWideChar(CP_ACP, 0, searchString, -1, searchString16, len);
	int rc = ListSearchTextW(hWnd, searchString16, searchParameter);
	free(searchString16);
	return rc;
}

HWND APIENTRY ListLoadW (HWND hListerWnd, TCHAR* fileToLoad, int showFlags) {
	int len = _tcslen(fileToLoad) + 10;
	TCHAR* journal = calloc(len + 1, sizeof(TCHAR));
	_sntprintf(journal, len, TEXT("%ls-wal"), fileToLoad);
	BOOL hasJournal = _taccess(journal, 0) == 0;
	free(journal);
	
	char* fileToLoad8 = utf16to8(fileToLoad);

	sqlite3 *db = 0;
	if (SQLITE_OK != sqlite3_open_v2(fileToLoad8, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL)) {
		MessageBox(hListerWnd, TEXT("Error to open database"), fileToLoad, MB_OK);
		free(fileToLoad8);
		return NULL;
	}
	free(fileToLoad8);

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);
	
	setlocale(LC_CTYPE, "");

	BOOL isStandalone = GetParent(hListerWnd) == HWND_DESKTOP;
	HWND hMainWnd = CreateWindow(WC_STATIC, APP_NAME, WS_CHILD | (isStandalone ? SS_SUNKEN : 0),
		0, 0, 100, 100, hListerWnd, (HMENU)IDC_MAIN, GetModuleHandle(0), NULL);

	int cacheSize = getStoredValue(TEXT("cache-size"), 2000);
	SetProp(hMainWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, (LONG_PTR)&cbNewMain));
	SetProp(hMainWnd, TEXT("FILTERROW"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("CACHE"), calloc(cacheSize, sizeof(TCHAR*)));
	SetProp(hMainWnd, TEXT("CACHESIZE"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("DB"), (HANDLE)db);
	SetProp(hMainWnd, TEXT("HASJOURNAL"), calloc(1, sizeof(int)));	
	SetProp(hMainWnd, TEXT("ORDERBY"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("CACHEOFFSET"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("TABLENAME8"), calloc(MAX_TABLE_LENGTH, sizeof(char)));
	SetProp(hMainWnd, TEXT("WHERE8"), calloc(MAX_TEXT_LENGTH, sizeof(char)));
	SetProp(hMainWnd, TEXT("ROWCOUNT"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("TOTALROWCOUNT"), calloc(1, sizeof(int)));	
	SetProp(hMainWnd, TEXT("COLNO"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("SEARCHCOLNO"), calloc(1, sizeof(int)));	
	SetProp(hMainWnd, TEXT("SPLITTERPOSITION"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("FONT"), 0);
	SetProp(hMainWnd, TEXT("FONTFAMILY"), getStoredString(TEXT("font"), TEXT("Arial")));	
	SetProp(hMainWnd, TEXT("FONTSIZE"), calloc(1, sizeof(int)));	
	SetProp(hMainWnd, TEXT("FILTERALIGN"), calloc(1, sizeof(int)));		

	SetProp(hMainWnd, TEXT("DARKTHEME"), calloc(1, sizeof(int)));			
	SetProp(hMainWnd, TEXT("TEXTCOLOR"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("BACKCOLOR"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("FILTERTEXTCOLOR"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("FILTERBACKCOLOR"), calloc(1, sizeof(int)));	
	SetProp(hMainWnd, TEXT("SPLITTERCOLOR"), calloc(1, sizeof(int)));

	*(int*)GetProp(hMainWnd, TEXT("HASJOURNAL")) = hasJournal;	
	*(int*)GetProp(hMainWnd, TEXT("SPLITTERPOSITION")) = getStoredValue(TEXT("splitter-position"), 200);
	*(int*)GetProp(hMainWnd, TEXT("FONTSIZE")) = getStoredValue(TEXT("font-size"), 16);
	*(int*)GetProp(hMainWnd, TEXT("CACHESIZE")) = cacheSize;
	*(int*)GetProp(hMainWnd, TEXT("CACHEOFFSET")) = -1;	
	*(int*)GetProp(hMainWnd, TEXT("FILTERROW")) = getStoredValue(TEXT("filter-row"), 1);
	*(int*)GetProp(hMainWnd, TEXT("DARKTHEME")) = getStoredValue(TEXT("dark-theme"), 0);
	*(int*)GetProp(hMainWnd, TEXT("FILTERALIGN")) = getStoredValue(TEXT("filter-align"), 0);		

	HWND hStatusWnd = CreateStatusWindow(WS_CHILD | WS_VISIBLE |  (isStandalone ? SBARS_SIZEGRIP : 0), NULL, hMainWnd, IDC_STATUSBAR);
	HDC hDC = GetDC(hMainWnd);
	float z = GetDeviceCaps(hDC, LOGPIXELSX) / 96.0; // 96 = 100%, 120 = 125%, 144 = 150%
	ReleaseDC(hMainWnd, hDC);	
	int sizes[7] = {35 * z, 110 * z, 180 * z, 225 * z, 420 * z, 500 * z, -1};
	SendMessage(hStatusWnd, SB_SETPARTS, 7, (LPARAM)&sizes);

	HWND hListWnd = CreateWindow(TEXT("LISTBOX"), NULL, WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP | WS_HSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_TABLELIST, GetModuleHandle(0), NULL);
	SetProp(hListWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hListWnd, GWLP_WNDPROC, (LONG_PTR)cbHotKey));	

	HWND hGridWnd = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_OWNERDATA | WS_TABSTOP,
		205, 0, 100, 100, hMainWnd, (HMENU)IDC_GRID, GetModuleHandle(0), NULL);
	ListView_SetExtendedListViewStyle(hGridWnd, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
	SetProp(hGridWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hGridWnd, GWLP_WNDPROC, (LONG_PTR)cbHotKey));
	
	HWND hHeader = ListView_GetHeader(hGridWnd);
	LONG_PTR styles = GetWindowLongPtr(hHeader, GWL_STYLE);
	SetWindowLongPtr(hHeader, GWL_STYLE, styles | HDS_FILTERBAR);
	SetWindowTheme(hHeader, TEXT(" "), TEXT(" "));
	SetProp(hHeader, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hHeader, GWLP_WNDPROC, (LONG_PTR)cbNewHeader));

	HMENU hDataMenu = CreatePopupMenu();
	AppendMenu(hDataMenu, MF_STRING, IDM_COPY_CELL, TEXT("Copy cell"));
	AppendMenu(hDataMenu, MF_STRING, IDM_COPY_ROW, TEXT("Copy row"));
	AppendMenu(hDataMenu, MF_STRING, 0, NULL);
	AppendMenu(hDataMenu, (*(int*)GetProp(hMainWnd, TEXT("FILTERROW")) != 0 ? MF_CHECKED : 0) | MF_STRING, IDM_FILTER_ROW, TEXT("Filters"));		
	AppendMenu(hDataMenu, (*(int*)GetProp(hMainWnd, TEXT("DARKTHEME")) != 0 ? MF_CHECKED : 0) | MF_STRING, IDM_DARK_THEME, TEXT("Dark theme"));				
	SetProp(hMainWnd, TEXT("DATAMENU"), hDataMenu);

	HMENU hListMenu = CreatePopupMenu();
	AppendMenu(hListMenu, MF_STRING, IDM_DDL, TEXT("View DDL"));
	SetProp(hMainWnd, TEXT("LISTMENU"), hListMenu);

	int tCount = 0, vCount = 0;
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "select name, type from sqlite_master where type in ('table', 'view') order by type, name", -1, &stmt, 0);
	while(SQLITE_ROW == sqlite3_step(stmt)) {
		TCHAR* name16 = utf8to16((char*)sqlite3_column_text(stmt, 0));
		int pos = ListBox_AddString(hListWnd, name16);
		free(name16);

		BOOL isTable = strcmp((char*)sqlite3_column_text(stmt, 1), "table") == 0;
		SendMessage(hListWnd, LB_SETITEMDATA, pos, isTable);

		tCount += isTable;
		vCount += !isTable;
	}
	sqlite3_finalize(stmt);
	TCHAR buf[255];
	_sntprintf(buf, 32, TEXT(" %ls"), APP_VERSION);	
	SendMessage(hStatusWnd, SB_SETTEXT, SB_VERSION, (LPARAM)buf);	
	_sntprintf(buf, 255, TEXT(" Tables: %i"), tCount);
	SendMessage(hStatusWnd, SB_SETTEXT, SB_TABLE_COUNT, (LPARAM)buf);
	_sntprintf(buf, 255, TEXT(" Views: %i"), vCount);
	SendMessage(hStatusWnd, SB_SETTEXT, SB_VIEW_COUNT, (LPARAM)buf);

	SendMessage(hMainWnd, WMU_SET_FONT, 0, 0);
	SendMessage(hMainWnd, WMU_SET_THEME, 0, 0);
	ListBox_SetCurSel(hListWnd, 0);
	SendMessage(hMainWnd, WMU_UPDATE_GRID, 0, 0);
	ShowWindow(hMainWnd, SW_SHOW);
	SetFocus(hListWnd);

	return hMainWnd;
}

HWND APIENTRY ListLoad (HWND hListerWnd, char* fileToLoad, int showFlags) {
	DWORD size = MultiByteToWideChar(CP_ACP, 0, fileToLoad, -1, NULL, 0);
	TCHAR* fileToLoadW = (TCHAR*)calloc (size, sizeof (TCHAR));
	MultiByteToWideChar(CP_ACP, 0, fileToLoad, -1, fileToLoadW, size);
	HWND hWnd = ListLoadW(hListerWnd, fileToLoadW, showFlags);
	free(fileToLoadW);
	return hWnd;
}

void __stdcall ListCloseWindow(HWND hWnd) {
	setStoredValue(TEXT("splitter-position"), *(int*)GetProp(hWnd, TEXT("SPLITTERPOSITION")));
	setStoredValue(TEXT("font-size"), *(int*)GetProp(hWnd, TEXT("FONTSIZE")));
	setStoredValue(TEXT("filter-row"), *(int*)GetProp(hWnd, TEXT("FILTERROW")));	
	setStoredValue(TEXT("dark-theme"), *(int*)GetProp(hWnd, TEXT("DARKTHEME")));	

	SendMessage(hWnd, WMU_RESET_CACHE, 0, 0);
	sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
	BOOL hasJournal = *(int*)GetProp(hWnd, TEXT("HASJOURNAL"));
	int deleteMode = getStoredValue(TEXT("delete-journal"), 0);
	TCHAR* dbpath = utf8to16(sqlite3_db_filename(db, 0));	
	sqlite3_close(db);

	if (!hasJournal && deleteMode) {
		int len = _tcslen(dbpath) + 10;
		TCHAR* journal = calloc(len + 1, sizeof(TCHAR));
		_sntprintf(journal, len, TEXT("%ls-wal"), dbpath);
		
		if (_taccess(journal, 0) == 0 && (deleteMode == 2 || deleteMode == 1 && IDYES == MessageBox(hWnd, TEXT("Delete journal files?"), TEXT("Confirmation"), MB_YESNO | MB_ICONQUESTION))) {
			DeleteFile(journal);
			_sntprintf(journal, len, TEXT("%ls-shm"), dbpath);
			DeleteFile(journal);
		}
		free(journal);
	}	
	free(dbpath);
	
	free((int*)GetProp(hWnd, TEXT("HASJOURNAL")));
	free((int*)GetProp(hWnd, TEXT("FILTERROW")));
	free((int*)GetProp(hWnd, TEXT("DARKTHEME")));	
	free((TCHAR***)GetProp(hWnd, TEXT("CACHE")));
	free((TCHAR***)GetProp(hWnd, TEXT("CACHESIZE")));	
	free((int*)GetProp(hWnd, TEXT("ORDERBY")));
	free((int*)GetProp(hWnd, TEXT("CACHEOFFSET")));
	free((char*)GetProp(hWnd, TEXT("TABLENAME8")));
	free((char*)GetProp(hWnd, TEXT("WHERE8")));
	free((int*)GetProp(hWnd, TEXT("ROWCOUNT")));
	free((int*)GetProp(hWnd, TEXT("TOTALROWCOUNT")));	
	free((int*)GetProp(hWnd, TEXT("SPLITTERPOSITION")));
	free((int*)GetProp(hWnd, TEXT("FONTSIZE")));	
	free((int*)GetProp(hWnd, TEXT("COLNO")));
	free((int*)GetProp(hWnd, TEXT("SEARCHCOLNO")));	
	free((TCHAR*)GetProp(hWnd, TEXT("FONTFAMILY")));
	free((int*)GetProp(hWnd, TEXT("FILTERALIGN")));
	
	free((int*)GetProp(hWnd, TEXT("TEXTCOLOR")));
	free((int*)GetProp(hWnd, TEXT("BACKCOLOR")));
	free((int*)GetProp(hWnd, TEXT("FILTERTEXTCOLOR")));
	free((int*)GetProp(hWnd, TEXT("FILTERBACKCOLOR")));	
	free((int*)GetProp(hWnd, TEXT("SPLITTERCOLOR")));	

	DeleteFont(GetProp(hWnd, TEXT("FONT")));
	DeleteObject(GetProp(hWnd, TEXT("BACKBRUSH")));	
	DeleteObject(GetProp(hWnd, TEXT("FILTERBACKBRUSH")));
	DeleteObject(GetProp(hWnd, TEXT("SPLITTERBRUSH")));
	DestroyMenu(GetProp(hWnd, TEXT("DATAMENU")));
	DestroyMenu(GetProp(hWnd, TEXT("LISTMENU")));

	RemoveProp(hWnd, TEXT("CACHESIZE"));
	RemoveProp(hWnd, TEXT("FILTERROW"));
	RemoveProp(hWnd, TEXT("DARKTHEME"));	
	RemoveProp(hWnd, TEXT("WNDPROC"));
	RemoveProp(hWnd, TEXT("DB"));
	RemoveProp(hWnd, TEXT("HASJOURNAL"));	
	RemoveProp(hWnd, TEXT("ORDERBY"));
	RemoveProp(hWnd, TEXT("CACHEOFFSET"));
	RemoveProp(hWnd, TEXT("TABLENAME8"));
	RemoveProp(hWnd, TEXT("WHERE8"));
	RemoveProp(hWnd, TEXT("ROWCOUNT"));
	RemoveProp(hWnd, TEXT("TOTALROWCOUNT"));	
	RemoveProp(hWnd, TEXT("SPLITTERPOSITION"));
	RemoveProp(hWnd, TEXT("COLNO"));
	RemoveProp(hWnd, TEXT("SEARCHCOLNO"));
	RemoveProp(hWnd, TEXT("FILTERALIGN"));
	
	RemoveProp(hWnd, TEXT("FONT"));
	RemoveProp(hWnd, TEXT("FONTFAMILY"));
	RemoveProp(hWnd, TEXT("FONTSIZE"));			
	RemoveProp(hWnd, TEXT("TEXTCOLOR"));
	RemoveProp(hWnd, TEXT("BACKCOLOR"));
	RemoveProp(hWnd, TEXT("FILTERTEXTCOLOR"));
	RemoveProp(hWnd, TEXT("FILTERBACKCOLOR"));
	RemoveProp(hWnd, TEXT("SPLITTERCOLOR"));
	RemoveProp(hWnd, TEXT("BACKBRUSH"));
	RemoveProp(hWnd, TEXT("FILTERBACKBRUSH"));		
	RemoveProp(hWnd, TEXT("SPLITTERBRUSH"));	
	RemoveProp(hWnd, TEXT("DATAMENU"));
	RemoveProp(hWnd, TEXT("LISTMENU"));	

	DestroyWindow(hWnd);
}

LRESULT CALLBACK cbNewMain(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_SIZE: {
			HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);
			SendMessage(hStatusWnd, WM_SIZE, 0, 0);
			RECT rc;
			GetClientRect(hStatusWnd, &rc);
			int statusH = rc.bottom;

			int splitterW = *(int*)GetProp(hWnd, TEXT("SPLITTERPOSITION"));
			GetClientRect(hWnd, &rc);
			HWND hListWnd = GetDlgItem(hWnd, IDC_TABLELIST);
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			SetWindowPos(hListWnd, 0, 0, 0, splitterW, rc.bottom - statusH, SWP_NOMOVE | SWP_NOZORDER);
			SetWindowPos(hGridWnd, 0, splitterW + SPLITTER_WIDTH, 0, rc.right - splitterW - SPLITTER_WIDTH, rc.bottom - statusH, SWP_NOZORDER);
		}
		break;
		
		case WM_PAINT: {
			PAINTSTRUCT ps = {0};
			HDC hDC = BeginPaint(hWnd, &ps);

			RECT rc;
			GetClientRect(hWnd, &rc);
			rc.left = *(int*)GetProp(hWnd, TEXT("SPLITTERPOSITION"));
			rc.right = rc.left + SPLITTER_WIDTH;
			FillRect(hDC, &rc, (HBRUSH)GetProp(hWnd, TEXT("SPLITTERBRUSH")));
			EndPaint(hWnd, &ps);

			return 0;
		}
		break;

		// https://groups.google.com/g/comp.os.ms-windows.programmer.win32/c/1XhCKATRXws
		case WM_NCHITTEST: {
			return 1;
		}
		break;
		
		case WM_SETCURSOR: {
			SetCursor(LoadCursor(0, GetProp(hWnd, TEXT("ISMOUSEHOVER")) ? IDC_SIZEWE : IDC_ARROW));
			return TRUE;
		}
		break;			

		case WM_LBUTTONDOWN: {
			int x = GET_X_LPARAM(lParam);
			int pos = *(int*)GetProp(hWnd, TEXT("SPLITTERPOSITION"));
			if (x >= pos && x <= pos + SPLITTER_WIDTH) {
				SetProp(hWnd, TEXT("ISMOUSEDOWN"), (HANDLE)1);
				SetCapture(hWnd);
			}
			return 0;
		}
		break;

		case WM_LBUTTONUP: {
			ReleaseCapture();
			RemoveProp(hWnd, TEXT("ISMOUSEDOWN"));
		}
		break;

		case WM_MOUSEMOVE: {
			DWORD x = GET_X_LPARAM(lParam);
			int* pPos = (int*)GetProp(hWnd, TEXT("SPLITTERPOSITION"));
			
			if (!GetProp(hWnd, TEXT("ISMOUSEHOVER")) && *pPos <= x && x <= *pPos + SPLITTER_WIDTH) {
				TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0};
				TrackMouseEvent(&tme);	
				SetProp(hWnd, TEXT("ISMOUSEHOVER"), (HANDLE)1);
			}
			
			if (GetProp(hWnd, TEXT("ISMOUSEDOWN")) && x > 0 && x < 32000) {
				*pPos = x;
				SendMessage(hWnd, WM_SIZE, 0, 0);
			}
		}
		break;
		
		case WM_MOUSELEAVE: {
			SetProp(hWnd, TEXT("ISMOUSEHOVER"), 0);
		}
		break;

		case WM_MOUSEWHEEL: {
			if (LOWORD(wParam) == MK_CONTROL) {
				SendMessage(hWnd, WMU_SET_FONT, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1: -1, 0);
				return 1;
			}
		}
		break;
		
		case WM_KEYDOWN: {
			if (wParam == VK_ESCAPE)
				SendMessage(GetParent(hWnd), WM_CLOSE, 0, 0);

			if (wParam == VK_TAB) {
				HWND hFocus = GetFocus();
				HWND wnds[1000] = {0};
				EnumChildWindows(hWnd, (WNDENUMPROC)cbEnumTabStopChildren, (LPARAM)wnds);

				int no = 0;
				while(wnds[no] && wnds[no] != hFocus)
					no++;

				int cnt = no;
				while(wnds[cnt])
					cnt++;

				BOOL isBackward = HIWORD(GetKeyState(VK_CONTROL));
				no += isBackward ? -1 : 1;
				SetFocus(wnds[no] && no >= 0 ? wnds[no] : (isBackward ? wnds[cnt - 1] : wnds[0]));
				return TRUE;
			}
			
			if (wParam == VK_F1) {
				ShellExecute(0, 0, TEXT("https://github.com/little-brother/sqlite-wlx/wiki"), 0, 0 , SW_SHOW);
				return TRUE;
			}			
		}
		break;		

		case WM_CONTEXTMENU: {
			if ((HWND)wParam == GetDlgItem(hWnd, IDC_TABLELIST)) {
				HWND hListWnd = (HWND)wParam;

				POINT p;
				GetCursorPos(&p);
				ScreenToClient(hListWnd, &p);

				int pos = SendMessage(hListWnd, LB_ITEMFROMPOINT, 0, (WPARAM)MAKELPARAM(p.x, p.y));
				int currPos = ListBox_GetCurSel(hListWnd);

				if (pos != -1 && currPos != pos) {
					ListBox_SetCurSel(hListWnd, pos);
					SendMessage(hWnd, WM_COMMAND, MAKELPARAM(IDC_TABLELIST, LBN_SELCHANGE), (LPARAM)hListWnd);
				}

				if (pos != -1) {
					ClientToScreen(hListWnd, &p);
					TrackPopupMenu(GetProp(hWnd, TEXT("LISTMENU")), TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hWnd, NULL);
				}
			}
		}
		break;
		
		case WM_CTLCOLORLISTBOX: {
			SetBkColor((HDC)wParam, *(int*)GetProp(hWnd, TEXT("BACKCOLOR")));
			SetTextColor((HDC)wParam, *(int*)GetProp(hWnd, TEXT("TEXTCOLOR")));
			return (INT_PTR)(HBRUSH)GetProp(hWnd, TEXT("BACKBRUSH"));	
		}
		break;		
		
		case WM_COMMAND: {
			WORD cmd = LOWORD(wParam);
			if (cmd == IDC_TABLELIST && HIWORD(wParam) == LBN_SELCHANGE)
				SendMessage(hWnd, WMU_UPDATE_GRID, 0, 0);

			if (cmd == IDM_COPY_CELL || cmd == IDM_COPY_ROW) {
				HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
				HWND hHeader = ListView_GetHeader(hGridWnd);
				int rowNo = ListView_GetNextItem(hGridWnd, -1, LVNI_SELECTED);
				if (rowNo != -1) {
					int cacheSize = *(int*)GetProp(hWnd, TEXT("CACHESIZE"));
					int* pCacheOffset = (int*)GetProp(hWnd, TEXT("CACHEOFFSET"));				
					TCHAR*** cache = (TCHAR***)GetProp(hWnd, TEXT("CACHE"));

					if (rowNo < *pCacheOffset || rowNo >= *pCacheOffset + cacheSize)
						return 0;

					int colCount = Header_GetItemCount(hHeader);

					int startNo = cmd == IDM_COPY_CELL ? *(int*)GetProp(hWnd, TEXT("COLNO")) : 0;
					int endNo = cmd == IDM_COPY_CELL ? startNo + 1 : colCount;
					if (startNo > colCount || endNo > colCount)
						return 0;

					int len = 0;
					for (int colNo = startNo; colNo < endNo; colNo++)
						len += _tcslen(cache[rowNo - *pCacheOffset][colNo]) + 1 /* column delimiter: TAB */;

					TCHAR buf[len + 1];
					buf[0] = 0;
					for (int colNo = startNo; colNo < endNo; colNo++) {
						_tcscat(buf, cache[rowNo - *pCacheOffset][colNo]);
						if (colNo != endNo - 1)
							_tcscat(buf, TEXT("\t"));
					}

					setClipboardText(buf);
				}
			}

			if (cmd == IDM_DDL) {
				sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
				char* tablename8 = (char*)GetProp(hWnd, TEXT("TABLENAME8"));

				sqlite3_stmt *stmt;
				if (SQLITE_OK == sqlite3_prepare_v2(db,
					"select group_concat(sql || ';', char(10) || char(10)) " \
					"from sqlite_master where tbl_name = ?1 order by iif(type = 'table' or type = 'view', 'a', type)", -1, &stmt, 0)) {
					sqlite3_bind_text(stmt, 1, tablename8, strlen(tablename8), SQLITE_TRANSIENT);

					if (SQLITE_ROW == sqlite3_step(stmt)) {
						TCHAR* sql = utf8to16((char*)sqlite3_column_text(stmt, 0));
						MessageBox(hWnd, sql, TEXT("DDL"), MB_OK);
						free(sql);
					}
				}
				sqlite3_finalize(stmt);
			}
			
			if (cmd == IDM_FILTER_ROW || cmd == IDM_DARK_THEME) {
				HMENU hMenu = (HMENU)GetProp(hWnd, TEXT("GRIDMENU"));
				int* pOpt = (int*)GetProp(hWnd, cmd == IDM_FILTER_ROW ? TEXT("FILTERROW") : TEXT("DARKTHEME"));
				*pOpt = (*pOpt + 1) % 2;
				Menu_SetItemState(hMenu, cmd, *pOpt ? MFS_CHECKED : 0);
				
				UINT msg = cmd == IDM_FILTER_ROW ? WMU_SET_HEADER_FILTERS : WMU_SET_THEME;
				SendMessage(hWnd, msg, 0, 0);				
			}			
		}
		break;

		case WM_NOTIFY : {
			NMHDR* pHdr = (LPNMHDR)lParam;
			if (pHdr->idFrom == IDC_GRID && pHdr->code == LVN_GETDISPINFO) {
				HWND hGridWnd = pHdr->hwndFrom;
				sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
				int orderBy = *(int*)GetProp(hWnd, TEXT("ORDERBY"));

				LV_DISPINFO* pDispInfo = (LV_DISPINFO*)lParam;
				LV_ITEM* pItem= &(pDispInfo)->item;
				int cacheSize = *(int*)GetProp(hWnd, TEXT("CACHESIZE"));
				int* pCacheOffset = (int*)GetProp(hWnd, TEXT("CACHEOFFSET"));
				TCHAR*** cache = (TCHAR***)GetProp(hWnd, TEXT("CACHE"));

			   	if ((pItem->iItem < *pCacheOffset) || (pItem->iItem >= *pCacheOffset + cacheSize) || (*pCacheOffset == -1)) {
			   		SendMessage(hWnd, WMU_RESET_CACHE, 0, 0);
			   		*pCacheOffset = pItem->iItem;

			   		HWND hHeader = ListView_GetHeader(hGridWnd);
			   		int colCount = Header_GetItemCount(hHeader);
					sqlite3_stmt *stmt;

					char* tablename8 = (char*)GetProp(hWnd, TEXT("TABLENAME8"));
					char* where8 = (char*)GetProp(hWnd, TEXT("WHERE8"));

					char query8[1024 + strlen(tablename8) + strlen(where8)];
					char orderBy8[32] = {0};
					if (orderBy > 0)
						sprintf(orderBy8, "order by %i", orderBy);
					if (orderBy < 0)
						sprintf(orderBy8, "order by %i desc", -orderBy);

					sprintf(query8, "select * from \"%s\" %s %s limit %i offset %i", tablename8, where8, orderBy8, cacheSize, *pCacheOffset);
					if (SQLITE_OK == sqlite3_prepare_v2(db, query8, -1, &stmt, 0)) {
						bindFilterValues(hHeader, stmt);

						int rowNo = 0;
						while (SQLITE_ROW == sqlite3_step(stmt)) {
							cache[rowNo] = (TCHAR**)calloc (colCount, sizeof (TCHAR*));

							for (int colNo = 0; colNo < colCount; colNo++) {
								cache[rowNo][colNo] = sqlite3_column_type(stmt, colNo) != SQLITE_BLOB ?
									utf8to16((const char*)sqlite3_column_text(stmt, colNo)) :
									utf8to16("(BLOB)");
							}

							rowNo++;
						}
					}
					sqlite3_finalize(stmt);
			   	}

				if(pItem->mask & LVIF_TEXT)
					pItem->pszText = cache[pItem->iItem - *pCacheOffset][pItem->iSubItem];
			}

			if (pHdr->idFrom == IDC_GRID && pHdr->code == LVN_COLUMNCLICK) {
				NMLISTVIEW* pLV = (NMLISTVIEW*)lParam;
				int colNo = pLV->iSubItem + 1;
				int* pOrderBy = (int*)GetProp(hWnd, TEXT("ORDERBY"));
				int orderBy = *pOrderBy;
				*pOrderBy = colNo == orderBy || colNo == -orderBy ? -orderBy : colNo;
				SendMessage(hWnd, WMU_UPDATE_DATA, 0, 0);
			}

			if (pHdr->idFrom == IDC_GRID && pHdr->code == (DWORD)NM_RCLICK) {
				NMITEMACTIVATE* ia = (LPNMITEMACTIVATE) lParam;

				POINT p;
				GetCursorPos(&p);
				*(int*)GetProp(hWnd, TEXT("COLNO")) = ia->iSubItem;
				TrackPopupMenu(GetProp(hWnd, TEXT("DATAMENU")), TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hWnd, NULL);
			}

			if (pHdr->idFrom == IDC_GRID && pHdr->code == (DWORD)LVN_ITEMCHANGED) {
				HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);

				TCHAR buf[255] = {0};
				int pos = ListView_GetNextItem(pHdr->hwndFrom, -1, LVNI_SELECTED);
				if (pos != -1)
					_sntprintf(buf, 255, TEXT(" %i"), pos + 1);
				SendMessage(hStatusWnd, SB_SETTEXT, SB_CURRENT_ROW, (LPARAM)buf);
				SendMessage(hStatusWnd, SB_SETTEXT, SB_AUXILIARY, (LPARAM)0);
			}

			if (pHdr->idFrom == IDC_GRID && pHdr->code == (DWORD)LVN_KEYDOWN) {
				NMLVKEYDOWN* kd = (LPNMLVKEYDOWN) lParam;
				if (kd->wVKey == 0x43 && GetKeyState(VK_CONTROL)) // Ctrl + C
					SendMessage(hWnd, WM_COMMAND, IDM_COPY_ROW, 0);
			}

			if (pHdr->code == HDN_ITEMCHANGED && pHdr->hwndFrom == ListView_GetHeader(GetDlgItem(hWnd, IDC_GRID)))
				SendMessage(hWnd, WMU_UPDATE_FILTER_SIZE, 0, 0);
		}
		break;

		case WMU_UPDATE_GRID: {
			HWND hListWnd = GetDlgItem(hWnd, IDC_TABLELIST);
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);
			sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
			int filterAlign = *(int*)GetProp(hWnd, TEXT("FILTERALIGN"));
			
			SendMessage(hGridWnd, WM_SETREDRAW, FALSE, 0);
			HWND hHeader = ListView_GetHeader(hGridWnd);

			SendMessage(hWnd, WMU_RESET_CACHE, 0, 0);
			ListView_SetItemCount(hGridWnd, 0);

			int colCount = Header_GetItemCount(hHeader);
			for (int colNo = 0; colNo < colCount; colNo++) 
				DestroyWindow(GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo));

			for (int colNo = 0; colNo < colCount; colNo++)
				ListView_DeleteColumn(hGridWnd, colCount - colNo - 1);

			char* tablename8 = (char*)GetProp(hWnd, TEXT("TABLENAME8"));
			int pos = ListBox_GetCurSel(hListWnd);
			TCHAR name16[256];
			ListBox_GetText(hListWnd, pos, name16);
			char* name8 = utf16to8(name16);
			sprintf(tablename8, name8);
			free(name8);

			TCHAR buf[255];
			int type = SendMessage(hListWnd, LB_GETITEMDATA, pos, 0);
			_sntprintf(buf, 255, type ? TEXT(" TABLE"): TEXT("  VIEW"));
			SendMessage(hStatusWnd, SB_SETTEXT, SB_TYPE, (LPARAM)buf);
			SendMessage(hStatusWnd, SB_SETTEXT, SB_CURRENT_ROW, (LPARAM)0);

			ListView_SetItemCount(hGridWnd, 0);

			sqlite3_stmt *stmt;
			char query8[1024 + strlen(tablename8)];
			sprintf(query8, "select * from \"%s\" limit 1", tablename8);
			if (SQLITE_OK == sqlite3_prepare_v2(db, query8, -1, &stmt, 0)) {
				colCount = sqlite3_column_count(stmt);
				for (int colNo = 0; colNo < colCount; colNo++) {
					TCHAR* name16 = utf8to16(sqlite3_column_name(stmt, colNo));
					const char* decltype8 = sqlite3_column_decltype(stmt, colNo);
					int fmt = LVCFMT_LEFT;
					if (decltype8) {
						char type8[strlen(decltype8) + 1];
						sprintf(type8, decltype8);
						strlwr(type8);

						if (strstr(type8, "int") || strstr(type8, "float") || strstr(type8, "double") ||
							strstr(type8, "real") || strstr(type8, "decimal") || strstr(type8, "numeric"))
							fmt = LVCFMT_RIGHT;
					}
					
					ListView_AddColumn(hGridWnd, name16, fmt);
					free(name16);
				}

				int align = filterAlign == -1 ? ES_LEFT : filterAlign == 1 ? ES_RIGHT : ES_CENTER;
				for (int colNo = 0; colNo < colCount; colNo++) {
					RECT rc;
					Header_GetItemRect(hHeader, colNo, &rc);
					HWND hEdit = CreateWindowEx(WS_EX_TOPMOST, WC_EDIT, NULL, align | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER | WS_TABSTOP, 0, 0, 0, 0, hHeader, (HMENU)(INT_PTR)(IDC_HEADER_EDIT + colNo), GetModuleHandle(0), NULL);
					SendMessage(hEdit, WM_SETFONT, (LPARAM)GetProp(hWnd, TEXT("FONT")), TRUE);
					SetProp(hEdit, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)cbNewFilterEdit));
				}
				SendMessage(hWnd, WMU_UPDATE_FILTER_SIZE, 0, 0);
			}
			sqlite3_finalize(stmt);

			*(int*)GetProp(hWnd, TEXT("ORDERBY")) = 0;
			SendMessage(hWnd, WMU_UPDATE_ROW_COUNT, 0, 0);
			SendMessage(hWnd, WMU_UPDATE_DATA, 0, 0);
			SendMessage(hGridWnd, WM_SETREDRAW, TRUE, 0);

			SendMessage(hWnd, WMU_SET_HEADER_FILTERS, 0, 0);
			PostMessage(hWnd, WMU_AUTO_COLUMN_SIZE, 0, 0);
		}
		break;

		case WMU_UPDATE_DATA: {
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			*(int*)GetProp(hWnd, TEXT("CACHEOFFSET")) = -1;
			ListView_SetItemCount(hGridWnd, ListView_GetItemCount(hGridWnd));
			UpdateWindow(hGridWnd);
		}
		break;

		case WMU_UPDATE_ROW_COUNT: {
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);
			HWND hHeader = ListView_GetHeader(hGridWnd);
			int colCount = Header_GetItemCount(hHeader);
			sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
			char* tablename8 = (char*)GetProp(hWnd, TEXT("TABLENAME8"));
			char* where8 = (char*)GetProp(hWnd, TEXT("WHERE8"));
			int* pTotalRowCount = (int*)GetProp(hWnd, TEXT("TOTALROWCOUNT"));
			int* pRowCount = (int*)GetProp(hWnd, TEXT("ROWCOUNT"));			

			TCHAR where16[MAX_TEXT_LENGTH] = {0};
			_tcscat(where16, TEXT("where (1 = 1) "));

			for (int colNo = 0; colNo < colCount; colNo++) {
				HWND hEdit = GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo);
				if (GetWindowTextLength(hEdit) > 0) {
					TCHAR colname16[256] = {0};
					Header_GetItemText(hHeader, colNo, colname16, 255);
					_tcscat(where16, TEXT(" and \""));
					_tcscat(where16, colname16);

					TCHAR buf16[2] = {0};
					GetWindowText(hEdit, buf16, 2);
					_tcscat(where16,
						buf16[0] == TEXT('=') ? TEXT("\" = ? ") :
						buf16[0] == TEXT('!') ? TEXT("\" not like '%%' || ? || '%%' ") :
						buf16[0] == TEXT('>') ? TEXT("\" > ? ") :
						buf16[0] == TEXT('<') ? TEXT("\" < ? ") :
						TEXT("\" like '%%' || ? || '%%' "));
				}
			}
			char* _where8 = utf16to8(where16);
			sprintf(where8, _where8);
			free(_where8);

			sqlite3_stmt *stmt;
			char query8[1024 + strlen(tablename8) + strlen(where8)];
			sprintf(query8, "select count(*) from \"%s\" %s", tablename8, where8);
			int rowCount = -1;
			if (SQLITE_OK == sqlite3_prepare_v2(db, query8, -1, &stmt, 0)) {
				bindFilterValues(hHeader, stmt);

				if (SQLITE_ROW == sqlite3_step(stmt))
					rowCount = sqlite3_column_int(stmt, 0);
			}
			sqlite3_finalize(stmt);

			if (strcmp(where8, "where (1 = 1) ") == 0)
				*pTotalRowCount = rowCount;
			*pRowCount = rowCount;				

			if (rowCount == -1)
				MessageBox(hWnd, TEXT("Error occured while fetching data"), NULL, MB_OK);
			else
				ListView_SetItemCount(hGridWnd, rowCount);

			TCHAR buf[255];
			_sntprintf(buf, 255, TEXT(" Rows: %i/%i"), rowCount, *pTotalRowCount);
			SendMessage(hStatusWnd, SB_SETTEXT, SB_ROW_COUNT, (LPARAM)buf);

			return rowCount;
		}
		break;

		case WMU_UPDATE_FILTER_SIZE: {
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			HWND hHeader = ListView_GetHeader(hGridWnd);
			int colCount = Header_GetItemCount(hHeader);
			SendMessage(hHeader, WM_SIZE, 0, 0);
			for (int colNo = 0; colNo < colCount; colNo++) {
				RECT rc;
				Header_GetItemRect(hHeader, colNo, &rc);
				int h2 = round((rc.bottom - rc.top) / 2);
				SetWindowPos(GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo), 0, rc.left - (colNo > 0), h2, rc.right - rc.left + 1, h2 + 1, SWP_NOZORDER);							
			}
		}
		break;
		
		case WMU_SET_HEADER_FILTERS: {
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			HWND hHeader = ListView_GetHeader(hGridWnd);
			int isFilterRow = *(int*)GetProp(hWnd, TEXT("FILTERROW"));
			int colCount = Header_GetItemCount(hHeader);
			
			SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);
			LONG_PTR styles = GetWindowLongPtr(hHeader, GWL_STYLE);
			styles = isFilterRow ? styles | HDS_FILTERBAR : styles & (~HDS_FILTERBAR);
			SetWindowLongPtr(hHeader, GWL_STYLE, styles);
					
			for (int colNo = 0; colNo < colCount; colNo++) 		
				ShowWindow(GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo), isFilterRow ? SW_SHOW : SW_HIDE);

			if (isFilterRow)				
				SendMessage(hWnd, WMU_UPDATE_FILTER_SIZE, 0, 0);											

			// Bug fix: force Windows to redraw header
			int w = ListView_GetColumnWidth(hGridWnd, 0);
			ListView_SetColumnWidth(hGridWnd, 0, w + 1);
			ListView_SetColumnWidth(hGridWnd, 0, w);			
			SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
			InvalidateRect(hWnd, NULL, TRUE);
		}
		break;

		case WMU_AUTO_COLUMN_SIZE: {
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			SendMessage(hGridWnd, WM_SETREDRAW, FALSE, 0);
			HWND hHeader = ListView_GetHeader(hGridWnd);
			int colCount = Header_GetItemCount(hHeader);

			for (int colNo = 0; colNo < colCount - 1; colNo++)
				ListView_SetColumnWidth(hGridWnd, colNo, colNo < colCount - 1 ? LVSCW_AUTOSIZE_USEHEADER : LVSCW_AUTOSIZE);

			if (colCount == 1 && ListView_GetColumnWidth(hGridWnd, 0) < 100)
				ListView_SetColumnWidth(hGridWnd, 0, 100);
				
			int maxWidth = getStoredValue(TEXT("max-column-width"), 300);
			if (colCount > 1) {
				for (int colNo = 0; colNo < colCount; colNo++) {
					if (ListView_GetColumnWidth(hGridWnd, colNo) > maxWidth)
						ListView_SetColumnWidth(hGridWnd, colNo, maxWidth);
				}
			}

			// Fix last column				
			if (colCount > 1) {
				int colNo = colCount - 1;
				ListView_SetColumnWidth(hGridWnd, colNo, LVSCW_AUTOSIZE);
				TCHAR name16[MAX_COLUMN_LENGTH + 1];
				Header_GetItemText(hHeader, colNo, name16, MAX_COLUMN_LENGTH);

				SIZE s = {0};
				HDC hDC = GetDC(hHeader);
				HFONT hOldFont = (HFONT)SelectObject(hDC, (HFONT)GetProp(hWnd, TEXT("FONT")));
				GetTextExtentPoint32(hDC, name16, _tcslen(name16), &s);
				SelectObject(hDC, hOldFont);
				ReleaseDC(hHeader, hDC);

				int w = s.cx + 12;
				if (ListView_GetColumnWidth(hGridWnd, colNo) < w)
					ListView_SetColumnWidth(hGridWnd, colNo, w);
					
				if (ListView_GetColumnWidth(hGridWnd, colNo) > maxWidth)
					ListView_SetColumnWidth(hGridWnd, colNo, maxWidth);					
			}

			SendMessage(hGridWnd, WM_SETREDRAW, TRUE, 0);
			InvalidateRect(hGridWnd, NULL, TRUE);

			PostMessage(hWnd, WMU_UPDATE_FILTER_SIZE, 0, 0);
		}
		break;

		case WMU_RESET_CACHE: {
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			int cacheSize = *(int*)GetProp(hWnd, TEXT("CACHESIZE"));
			TCHAR*** cache = (TCHAR***)GetProp(hWnd, TEXT("CACHE"));
			*(int*)GetProp(hWnd, TEXT("CACHEOFFSET")) = -1;
			int colCount = Header_GetItemCount(ListView_GetHeader(hGridWnd));
			if (colCount == 0)
				return 0;
			for (int rowNo = 0;  rowNo < cacheSize; rowNo++) {
				if (cache[rowNo]) {
					for (int colNo = 0; colNo < colCount; colNo++)
						if (cache[rowNo][colNo])
							free(cache[rowNo][colNo]);

					free(cache[rowNo]);
				}
				cache[rowNo] = 0;
			}
		}
		break;
		
		// wParam - size delta
		case WMU_SET_FONT: {
			int* pFontSize = (int*)GetProp(hWnd, TEXT("FONTSIZE"));
			if (*pFontSize + wParam < 10 || *pFontSize + wParam > 48)
				return 0;
			*pFontSize += wParam;
			DeleteFont(GetProp(hWnd, TEXT("FONT")));

			HFONT hFont = CreateFont (*pFontSize, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, (TCHAR*)GetProp(hWnd, TEXT("FONTFAMILY")));
			HWND hListWnd = GetDlgItem(hWnd, IDC_TABLELIST);
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			SendMessage(hListWnd, WM_SETFONT, (LPARAM)hFont, TRUE);
			SendMessage(hGridWnd, WM_SETFONT, (LPARAM)hFont, TRUE);

			HWND hHeader = ListView_GetHeader(hGridWnd);
			for (int colNo = 0; colNo < Header_GetItemCount(hHeader); colNo++)
				SendMessage(GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo), WM_SETFONT, (LPARAM)hFont, TRUE);
				
			int w = 0;
			HDC hDC = GetDC(hListWnd);
			HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
			TCHAR buf[MAX_TABLE_LENGTH]; 
			for (int i = 0; i < ListBox_GetCount(hListWnd); i++) {
				SIZE s = {0};
				ListBox_GetText(hListWnd, i, buf);
				GetTextExtentPoint32(hDC, buf, _tcslen(buf), &s);			
				if (w < s.cx)
					w = s.cx;
			}
			SelectObject(hDC, hOldFont);
			ReleaseDC(hHeader, hDC);
			SendMessage(hListWnd, LB_SETHORIZONTALEXTENT, w, 0);
				
			SetProp(hWnd, TEXT("FONT"), hFont);
			PostMessage(hWnd, WMU_AUTO_COLUMN_SIZE, 0, 0);
		}
		break;
		
		case WMU_SET_THEME: {
			HWND hGridWnd = GetDlgItem(hWnd, IDC_GRID);
			BOOL isDark = *(int*)GetProp(hWnd, TEXT("DARKTHEME"));
			
			int textColor = !isDark ? getStoredValue(TEXT("text-color"), RGB(0, 0, 0)) : getStoredValue(TEXT("text-color-dark"), RGB(220, 220, 220));
			int backColor = !isDark ? getStoredValue(TEXT("back-color"), RGB(255, 255, 255)) : getStoredValue(TEXT("back-color-dark"), RGB(32, 32, 32));
			int filterTextColor = !isDark ? getStoredValue(TEXT("filter-text-color"), RGB(0, 0, 0)) : getStoredValue(TEXT("filter-text-color-dark"), RGB(255, 255, 255));
			int filterBackColor = !isDark ? getStoredValue(TEXT("filter-back-color"), RGB(240, 240, 240)) : getStoredValue(TEXT("filter-back-color-dark"), RGB(60, 60, 60));
			int splitterColor = !isDark ? getStoredValue(TEXT("splitter-color"), GetSysColor(COLOR_BTNFACE)) : getStoredValue(TEXT("splitter-color-dark"), GetSysColor(COLOR_BTNFACE));			
			
			*(int*)GetProp(hWnd, TEXT("TEXTCOLOR")) = textColor;
			*(int*)GetProp(hWnd, TEXT("BACKCOLOR")) = backColor;
			*(int*)GetProp(hWnd, TEXT("FILTERTEXTCOLOR")) = filterTextColor;
			*(int*)GetProp(hWnd, TEXT("FILTERBACKCOLOR")) = filterBackColor;
			*(int*)GetProp(hWnd, TEXT("SPLITTERCOLOR")) = splitterColor;

			DeleteObject(GetProp(hWnd, TEXT("BACKBRUSH")));			
			DeleteObject(GetProp(hWnd, TEXT("FILTERBACKBRUSH")));			
			DeleteObject(GetProp(hWnd, TEXT("SPLITTERBRUSH")));			
			SetProp(hWnd, TEXT("BACKBRUSH"), CreateSolidBrush(backColor));
			SetProp(hWnd, TEXT("FILTERBACKBRUSH"), CreateSolidBrush(filterBackColor));
			SetProp(hWnd, TEXT("SPLITTERBRUSH"), CreateSolidBrush(splitterColor));			

			ListView_SetTextColor(hGridWnd, textColor);			
			ListView_SetBkColor(hGridWnd, backColor);
			ListView_SetTextBkColor(hGridWnd, backColor);
			InvalidateRect(hWnd, NULL, TRUE);	
		}
		break;	
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK cbNewHeader(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CTLCOLOREDIT) {
		HWND hMainWnd = getMainWindow(hWnd);
		SetBkColor((HDC)wParam, *(int*)GetProp(hMainWnd, TEXT("FILTERBACKCOLOR")));
		SetTextColor((HDC)wParam, *(int*)GetProp(hMainWnd, TEXT("FILTERTEXTCOLOR")));
		return (INT_PTR)(HBRUSH)GetProp(hMainWnd, TEXT("FILTERBACKBRUSH"));	
	}
	
	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK cbNewFilterEdit(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	WNDPROC cbDefault = (WNDPROC)GetProp(hWnd, TEXT("WNDPROC"));

	switch(msg){
		case WM_PAINT: {
			cbDefault(hWnd, msg, wParam, lParam);

			RECT rc;
			GetClientRect(hWnd, &rc);
			HDC hDC = GetWindowDC(hWnd);
			HPEN hPen = CreatePen(PS_SOLID, 1, *(int*)GetProp(getMainWindow(hWnd), TEXT("FILTERBACKCOLOR")));
			HPEN oldPen = SelectObject(hDC, hPen);
			MoveToEx(hDC, 1, 0, 0);
			LineTo(hDC, rc.right - 1, 0);
			SelectObject(hDC, oldPen);
			DeleteObject(hPen);
			ReleaseDC(hWnd, hDC);

			return 0;
		}

		case WM_CHAR: {
			return CallWindowProc(cbHotKey, hWnd, msg, wParam, lParam);
		}
		break;

		case WM_KEYDOWN: {
			if (wParam == VK_RETURN) {
				HWND hMainWnd = getMainWindow(hWnd);
				SendMessage(hMainWnd, WMU_UPDATE_ROW_COUNT, 0, 0);
				SendMessage(hMainWnd, WMU_UPDATE_DATA, 0, 0);
				return 0;
			}
			
			if (wParam == VK_TAB || wParam == VK_ESCAPE)
				return CallWindowProc(cbHotKey, hWnd, msg, wParam, lParam);			
		}
		break;

		case WM_DESTROY: {
			RemoveProp(hWnd, TEXT("WNDPROC"));
		}
		break;
	}

	return CallWindowProc(cbDefault, hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK cbHotKey(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_KEYDOWN && (
		wParam == VK_TAB || wParam == VK_ESCAPE || 
		wParam == VK_F3 || wParam == VK_F5 || wParam == VK_F7 || (HIWORD(GetKeyState(VK_CONTROL)) && wParam == 0x46) || // Ctrl + F
		wParam == VK_F1 || wParam == VK_F11 ||
		(wParam >= 0x31 && wParam <= 0x42) && !getStoredValue(TEXT("disable-num-keys"), 0) || // 1 - 8
		(wParam == 0x4E || wParam == 0x50) && !getStoredValue(TEXT("disable-np-keys"), 0))) { // N, P
		HWND hMainWnd = getMainWindow(hWnd);
			
		if (wParam == VK_F3 || wParam == VK_F5 || wParam == VK_F7 || wParam == 0x46) {
			int cacheSize = *(int*)GetProp(hMainWnd, TEXT("CACHESIZE"));	
			int rowCount = *(int*)GetProp(hMainWnd, TEXT("ROWCOUNT"));
			if (cacheSize < rowCount) {
				TCHAR msg[1024];
				_sntprintf(msg, 1024, TEXT("Standard search is available only for full-cached resultset.\nThis resultset has %i rows and a cache size is %i.\nCheck Wiki to learn how to increase the cache size."), rowCount, cacheSize);
				MessageBox(hMainWnd, msg, 0, 0);
				return 0;
			}	
		}
		if (wParam == VK_F7 || wParam == 0x46)
			*(int*)GetProp(hMainWnd, TEXT("SEARCHCOLNO")) = 0;
		SendMessage(wParam == VK_TAB || wParam == VK_ESCAPE || wParam == VK_F1 ? hMainWnd : GetParent(hMainWnd), WM_KEYDOWN, wParam, lParam);
		return 0;
	}
	
	// Prevent beep
	if (msg == WM_CHAR && (wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_TAB))
		return 0;
	
	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}

void bindFilterValues(HWND hHeader, sqlite3_stmt* stmt) {
	int colCount = Header_GetItemCount(hHeader);
	int bindNo = 1;
	for (int colNo = 0; colNo < colCount; colNo++) {
		HWND hEdit = GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo);
		int size = GetWindowTextLength(hEdit);
		if (size > 0) {
			TCHAR value16[size + 1];
			GetWindowText(hEdit, value16, size + 1);
			char* value8 = utf16to8(value16[0] == TEXT('=') || value16[0] == TEXT('/') || value16[0] == TEXT('!') || value16[0] == TEXT('<') || value16[0] == TEXT('>') ? value16 + 1 : value16);

			int len = strlen(value8);
			BOOL isNum = TRUE;
			int dotCount = 0;
			for (int i = +(value8[0] == '-' /* is negative */); i < len; i++) {
				isNum = isNum && (isdigit(value8[i]) || value8[i] == '.' || value8[i] == ',');
				dotCount += value8[i] == '.' || value8[i] == ',';
			}

			if (isNum && dotCount == 0 && len < 10) // 2147483647
				sqlite3_bind_int(stmt, bindNo, atoi(value8));
			else if (isNum && dotCount == 0 && len >= 10)
					sqlite3_bind_int64(stmt, bindNo, atoll(value8));
			else if (isNum && dotCount == 1) {
				double d;
				char *endptr;
				errno = 0;
				d = strtold(value8, &endptr);
				sqlite3_bind_double(stmt, bindNo, d);
			} else {
				sqlite3_bind_text(stmt, bindNo, value8, len, SQLITE_TRANSIENT);
			}

			free(value8);
			bindNo++;
		}
	}
}

HWND getMainWindow(HWND hWnd) {
	HWND hMainWnd = hWnd;
	while (hMainWnd && GetDlgCtrlID(hMainWnd) != IDC_MAIN)
		hMainWnd = GetParent(hMainWnd);
	return hMainWnd;	
}

void setStoredValue(TCHAR* name, int value) {
	TCHAR buf[128];
	_sntprintf(buf, 128, TEXT("%i"), value);
	WritePrivateProfileString(APP_NAME, name, buf, iniPath);
}

int getStoredValue(TCHAR* name, int defValue) {
	TCHAR buf[128];
	return GetPrivateProfileString(APP_NAME, name, NULL, buf, 128, iniPath) ? _ttoi(buf) : defValue;
}

TCHAR* getStoredString(TCHAR* name, TCHAR* defValue) { 
	TCHAR* buf = calloc(256, sizeof(TCHAR));
	if (0 == GetPrivateProfileString(APP_NAME, name, NULL, buf, 128, iniPath) && defValue)
		_tcsncpy(buf, defValue, 255);
	return buf;	
}

int CALLBACK cbEnumTabStopChildren (HWND hWnd, LPARAM lParam) {
	if (GetWindowLong(hWnd, GWL_STYLE) & WS_TABSTOP && IsWindowVisible(hWnd)) {
		int no = 0;
		HWND* wnds = (HWND*)lParam;
		while (wnds[no])
			no++;
		wnds[no] = hWnd;
	}

	return TRUE;
}

TCHAR* utf8to16(const char* in) {
	TCHAR *out;
	if (!in || strlen(in) == 0) {
		out = (TCHAR*)calloc (1, sizeof (TCHAR));
	} else  {
		DWORD size = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
		out = (TCHAR*)calloc (size, sizeof (TCHAR));
		MultiByteToWideChar(CP_UTF8, 0, in, -1, out, size);
	}
	return out;
}

char* utf16to8(const TCHAR* in) {
	char* out;
	if (!in || _tcslen(in) == 0) {
		out = (char*)calloc (1, sizeof(char));
	} else  {
		int len = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, 0, 0);
		out = (char*)calloc (len, sizeof(char));
		WideCharToMultiByte(CP_UTF8, 0, in, -1, out, len, 0, 0);
	}
	return out;
}

int findString(TCHAR* text, TCHAR* word, BOOL isMatchCase, BOOL isWholeWords) {
	if (!text || !word)
		return -1;
		
	int res = -1;
	int tlen = _tcslen(text);
	int wlen = _tcslen(word);	
	if (!tlen || !wlen)
		return res;
	
	if (!isMatchCase) {
		TCHAR* ltext = calloc(tlen + 1, sizeof(TCHAR));
		_tcsncpy(ltext, text, tlen);
		text = _tcslwr(ltext);

		TCHAR* lword = calloc(wlen + 1, sizeof(TCHAR));
		_tcsncpy(lword, word, wlen);
		word = _tcslwr(lword);
	}

	if (isWholeWords) {
		for (int pos = 0; (res  == -1) && (pos <= tlen - wlen); pos++) 
			res = (pos == 0 || pos > 0 && !_istalnum(text[pos - 1])) && 
				!_istalnum(text[pos + wlen]) && 
				_tcsncmp(text + pos, word, wlen) == 0 ? pos : -1;
	} else {
		TCHAR* s = _tcsstr(text, word);
		res = s != NULL ? s - text : -1;
	}
	
	if (!isMatchCase) {
		free(text);
		free(word);
	}

	return res; 
}	

void setClipboardText(const TCHAR* text) {
	int len = (_tcslen(text) + 1) * sizeof(TCHAR);
	HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), text, len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, hMem);
	CloseClipboard();
}

int ListView_AddColumn(HWND hListWnd, TCHAR* colName, int fmt) {
	int colNo = Header_GetItemCount(ListView_GetHeader(hListWnd));
	LVCOLUMN lvc = {0};
	lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT;
	lvc.iSubItem = colNo;
	lvc.pszText = colName;
	lvc.cchTextMax = _tcslen(colName) + 1;
	lvc.cx = 100;
	lvc.fmt = fmt;
	return ListView_InsertColumn(hListWnd, colNo, &lvc);
}

int Header_GetItemText(HWND hWnd, int i, TCHAR* pszText, int cchTextMax) {
	if (i < 0)
		return FALSE;

	TCHAR buf[cchTextMax];

	HDITEM hdi = {0};
	hdi.mask = HDI_TEXT;
	hdi.pszText = buf;
	hdi.cchTextMax = cchTextMax;
	int rc = Header_GetItem(hWnd, i, &hdi);

	_tcsncpy(pszText, buf, cchTextMax);
	return rc;
}

void Menu_SetItemState(HMENU hMenu, UINT wID, UINT fState) {
	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;
	mii.fState = fState;
	SetMenuItemInfo(hMenu, wID, FALSE, &mii);
}