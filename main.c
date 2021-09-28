#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "include/sqlite3.h"

#define LISTPLUGIN_OK      0
#define LISTPLUGIN_ERROR   1

#define LVS_EX_AUTOSIZECOLUMNS 0x10000000
#define WMU_UPDATE_GRID        WM_USER + 1
#define WMU_UPDATE_DATA        WM_USER + 2
#define WMU_UPDATE_ROW_COUNT   WM_USER + 3
#define WMU_UPDATE_COLSIZE     WM_USER + 4
#define WMU_AUTO_COLSIZE       WM_USER + 5
#define WMU_RESET_CACHE        WM_USER + 6

#define IDC_MAIN               100
#define IDC_TABLELIST          101
#define IDC_DATAGRID           102
#define IDC_STATUSBAR          103
#define IDC_HEADER_EDIT        1000
#define IDC_HEADER_STATIC      2000

#define IDM_COPY_CELL          5000
#define IDM_COPY_ROW           5001
#define IDM_DDL                5010

#define SB_TABLE_COUNT         0
#define SB_VIEW_COUNT          1
#define SB_TYPE                2
#define SB_ROW_COUNT           3
#define SB_CURRENT_ROW         4

#define MAX_TEXT_LENGTH        32000
#define CACHE_SIZE             100

LRESULT CALLBACK cbNewMain (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK cbNewFilterEdit (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK cbNewFilterStatic (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int Header_GetItemText(HWND hWnd, int i, TCHAR* pszText, int cchTextMax);
void bindFilterValues(HWND hHeader, sqlite3_stmt* stmt);

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

BOOL APIENTRY DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	return TRUE;
}

HWND APIENTRY ListLoad (HWND hListerWnd, char* fileToLoad, int showFlags) {
	DWORD size = MultiByteToWideChar(CP_ACP, 0, fileToLoad, -1, NULL, 0);
	TCHAR* fileToLoad16 = (TCHAR*)calloc (size, sizeof (TCHAR));
	MultiByteToWideChar(CP_ACP, 0, fileToLoad, -1, fileToLoad16, size);
	char* fileToLoad8 = utf16to8(fileToLoad16);

	sqlite3 *db = 0;
	if (SQLITE_OK != sqlite3_open_v2(fileToLoad8, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL)) {
		MessageBox(hListerWnd, TEXT("Error to open database"), fileToLoad16, MB_OK);
		free(fileToLoad16);
		free(fileToLoad8);
		return NULL;
	}
	free(fileToLoad16);
	free(fileToLoad8);

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	HWND hMainWnd = CreateWindow(WC_STATIC, TEXT("sqlite-wlx"), WS_CHILD | WS_VISIBLE | SS_SUNKEN,
		0, 0, 100, 100, hListerWnd, (HMENU)IDC_MAIN, GetModuleHandle(0), NULL);

	SetProp(hMainWnd, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, (LONG_PTR)&cbNewMain));
	SetProp(hMainWnd, TEXT("CACHE"), calloc(CACHE_SIZE, sizeof(TCHAR*)));
	SetProp(hMainWnd, TEXT("DB"), (HANDLE)db);
	SetProp(hMainWnd, TEXT("ORDERBY"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("CACHEOFFSET"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("TABLENAME8"), calloc(1024, sizeof(char)));
	SetProp(hMainWnd, TEXT("WHERE8"), calloc(MAX_TEXT_LENGTH, sizeof(char)));
	SetProp(hMainWnd, TEXT("ROWCOUNT"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("SPLITTERWIDTH"), calloc(1, sizeof(int)));
	SetProp(hMainWnd, TEXT("COLNO"), calloc(1, sizeof(int)));

	*(int*)GetProp(hMainWnd, TEXT("CACHEOFFSET")) = -1;
	*(int*)GetProp(hMainWnd, TEXT("SPLITTERWIDTH")) = 200;

	HWND hStatusWnd = CreateStatusWindow(WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, NULL, hMainWnd, IDC_STATUSBAR);
	int sizes[6] = {75, 150, 200, 400, 500,-1};
	SendMessage(hStatusWnd, SB_SETPARTS, 6, (LPARAM)&sizes);

	HWND hListWnd = CreateWindow(TEXT("LISTBOX"), NULL, WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
		0, 0, 100, 100, hMainWnd, (HMENU)IDC_TABLELIST, GetModuleHandle(0), NULL);

	HWND hDataWnd = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_OWNERDATA,
		205, 0, 100, 100, hMainWnd, (HMENU)IDC_DATAGRID, GetModuleHandle(0), NULL);
	ListView_SetExtendedListViewStyle(hDataWnd, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);

	HWND hHeader = ListView_GetHeader(hDataWnd);
	LONG_PTR styles = GetWindowLongPtr(hHeader, GWL_STYLE);
	SetWindowLongPtr(hHeader, GWL_STYLE, styles | HDS_FILTERBAR);

	HMENU hDataMenu = CreatePopupMenu();
	AppendMenu(hDataMenu, MF_STRING, IDM_COPY_CELL, TEXT("Copy cell"));
	AppendMenu(hDataMenu, MF_STRING, IDM_COPY_ROW, TEXT("Copy row"));
	SetProp(hMainWnd, TEXT("DATAMENU"), hDataMenu);

	HMENU hListMenu = CreatePopupMenu();
	AppendMenu(hListMenu, MF_STRING, IDM_DDL, TEXT("Get DDL"));
	SetProp(hMainWnd, TEXT("LISTMENU"), hListMenu);

	HFONT hFont = CreateFont (16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
	SendMessage(hListWnd, WM_SETFONT, (LPARAM)hFont, TRUE);
	SendMessage(hDataWnd, WM_SETFONT, (LPARAM)hFont, TRUE);
	SetProp(hMainWnd, TEXT("FONT"), hFont);

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
	_sntprintf(buf, 255, TEXT(" Tables: %i"), tCount);
	SendMessage(hStatusWnd, SB_SETTEXT, SB_TABLE_COUNT, (LPARAM)buf);
	_sntprintf(buf, 255, TEXT(" Views: %i"), vCount);
	SendMessage(hStatusWnd, SB_SETTEXT, SB_VIEW_COUNT, (LPARAM)buf);


	ListBox_SetCurSel(hListWnd, 0);
	SendMessage(hMainWnd, WMU_UPDATE_GRID, 0, 0);
	RegisterHotKey(hMainWnd, 0, 0, VK_ESCAPE);

	return hMainWnd;
}

void __stdcall ListCloseWindow(HWND hWnd) {
	SendMessage(hWnd, WMU_RESET_CACHE, 0, 0);
	sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
	sqlite3_close(db);

	free((TCHAR***)GetProp(hWnd, TEXT("CACHE")));
	free((int*)GetProp(hWnd, TEXT("ORDERBY")));
	free((int*)GetProp(hWnd, TEXT("CACHEOFFSET")));
	free((char*)GetProp(hWnd, TEXT("TABLENAME8")));
	free((char*)GetProp(hWnd, TEXT("WHERE8")));
	free((int*)GetProp(hWnd, TEXT("ROWCOUNT")));
	free((int*)GetProp(hWnd, TEXT("SPLITTERWIDTH")));
	free((int*)GetProp(hWnd, TEXT("COLNO")));

	DeleteFont(GetProp(hWnd, TEXT("FONT")));
	DestroyMenu(GetProp(hWnd, TEXT("DATAMENU")));
	DestroyMenu(GetProp(hWnd, TEXT("LISTMENU")));

	RemoveProp(hWnd, TEXT("WNDPROC"));
	RemoveProp(hWnd, TEXT("DB"));
	RemoveProp(hWnd, TEXT("ORDERBY"));
	RemoveProp(hWnd, TEXT("CACHEOFFSET"));
	RemoveProp(hWnd, TEXT("TABLENAME8"));
	RemoveProp(hWnd, TEXT("WHERE8"));
	RemoveProp(hWnd, TEXT("ROWCOUNT"));
	RemoveProp(hWnd, TEXT("SPLITTERWIDTH"));
	RemoveProp(hWnd, TEXT("COLNO"));
	RemoveProp(hWnd, TEXT("FONT"));
	RemoveProp(hWnd, TEXT("DATAMENU"));

	DestroyWindow(hWnd);
	return;
}

void __stdcall ListGetDetectString(char* DetectString, int maxlen) {
	snprintf(DetectString, maxlen, "MULTIMEDIA & (ext=\"SQLITE\" | ext=\"SQLITE3\" | ext=\"DB\" | ext=\"DB3\")");
}

LRESULT CALLBACK cbNewMain(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_HOTKEY: {
			SendMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
		}
		break;

		case WM_SIZE: {
			HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);
			SendMessage(hStatusWnd, WM_SIZE, 0, 0);
			RECT rc;
			GetClientRect(hStatusWnd, &rc);
			int statusH = rc.bottom;

			int splitterW = *(int*)GetProp(hWnd, TEXT("SPLITTERWIDTH"));
			GetClientRect(hWnd, &rc);
			HWND hListWnd = GetDlgItem(hWnd, IDC_TABLELIST);
			HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
			SetWindowPos(hListWnd, 0, 0, 0, splitterW, rc.bottom - statusH, SWP_NOMOVE | SWP_NOZORDER);
			SetWindowPos(hDataWnd, 0, splitterW + 5, 0, rc.right - splitterW - 5, rc.bottom - statusH, SWP_NOZORDER);
		}
		break;

		// https://groups.google.com/g/comp.os.ms-windows.programmer.win32/c/1XhCKATRXws
		case WM_NCHITTEST: {
			return 1;
		}
		break;

		case WM_LBUTTONDOWN: {
			SetProp(hWnd, TEXT("ISMOUSEDOWN"), (HANDLE)1);
			SetCapture(hWnd);
			return 0;
		}
		break;

		case WM_LBUTTONUP: {
			ReleaseCapture();
			RemoveProp(hWnd, TEXT("ISMOUSEDOWN"));
		}
		break;

		case WM_MOUSEMOVE: {
			if (wParam != MK_LBUTTON || !GetProp(hWnd, TEXT("ISMOUSEDOWN")))
				return 0;

			DWORD x = GET_X_LPARAM(lParam);
			if (x > 0 && x < 32000)
				*(int*)GetProp(hWnd, TEXT("SPLITTERWIDTH")) = x;
			SendMessage(hWnd, WM_SIZE, 0, 0);
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

		case WM_COMMAND: {
			WORD cmd = LOWORD(wParam);
			if (cmd == IDC_TABLELIST && HIWORD(wParam) == LBN_SELCHANGE)
				SendMessage(hWnd, WMU_UPDATE_GRID, 0, 0);

			if (cmd == IDM_COPY_CELL || cmd == IDM_COPY_ROW) {
				HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
				HWND hHeader = ListView_GetHeader(hDataWnd);
				int rowNo = ListView_GetNextItem(hDataWnd, -1, LVNI_SELECTED);
				if (rowNo != -1) {
					int* pCacheOffset = (int*)GetProp(hWnd, TEXT("CACHEOFFSET"));
					TCHAR*** cache = (TCHAR***)GetProp(hWnd, TEXT("CACHE"));

					if (rowNo < *pCacheOffset || rowNo >= *pCacheOffset + CACHE_SIZE)
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
		}
		break;

		case WM_NOTIFY : {
			NMHDR* pHdr = (LPNMHDR)lParam;
			if (pHdr->idFrom == IDC_DATAGRID && pHdr->code == LVN_GETDISPINFO) {
				HWND hDataWnd = pHdr->hwndFrom;
				sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
				int orderBy = *(int*)GetProp(hWnd, TEXT("ORDERBY"));

				LV_DISPINFO* pDispInfo = (LV_DISPINFO*)lParam;
				LV_ITEM* pItem= &(pDispInfo)->item;
				int* pCacheOffset = (int*)GetProp(hWnd, TEXT("CACHEOFFSET"));
				TCHAR*** cache = (TCHAR***)GetProp(hWnd, TEXT("CACHE"));

			   	if (pItem->iItem < *pCacheOffset || pItem->iItem > *pCacheOffset + CACHE_SIZE || *pCacheOffset == -1) {
			   		SendMessage(hWnd, WMU_RESET_CACHE, 0, 0);
			   		*pCacheOffset = pItem->iItem;

			   		HWND hHeader = ListView_GetHeader(hDataWnd);
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

					sprintf(query8, "select * from \"%s\" %s %s limit %i offset %i", tablename8, where8, orderBy8, CACHE_SIZE, *pCacheOffset);
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
					_tcsncpy(pItem->pszText, cache[pItem->iItem - *pCacheOffset][pItem->iSubItem], pItem->cchTextMax);
			}

			if (pHdr->idFrom == IDC_DATAGRID && pHdr->code == LVN_COLUMNCLICK) {
				NMLISTVIEW* pLV = (NMLISTVIEW*)lParam;
				int colNo = pLV->iSubItem + 1;
				int* pOrderBy = (int*)GetProp(hWnd, TEXT("ORDERBY"));
				int orderBy = *pOrderBy;
				*pOrderBy = colNo == orderBy || colNo == -orderBy ? -orderBy : colNo;
				SendMessage(hWnd, WMU_UPDATE_DATA, 0, 0);
			}

			if (pHdr->idFrom == IDC_DATAGRID && pHdr->code == (DWORD)NM_RCLICK) {
				NMITEMACTIVATE* ia = (LPNMITEMACTIVATE) lParam;

				POINT p;
				GetCursorPos(&p);
				*(int*)GetProp(hWnd, TEXT("COLNO")) = ia->iSubItem;
				TrackPopupMenu(GetProp(hWnd, TEXT("DATAMENU")), TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hWnd, NULL);
			}

			if (pHdr->idFrom == IDC_DATAGRID && pHdr->code == (DWORD)LVN_ITEMCHANGED) {
				HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);

				TCHAR buf[255] = {0};
				int pos = ListView_GetNextItem(pHdr->hwndFrom, -1, LVNI_SELECTED);
				if (pos != -1)
					_sntprintf(buf, 255, TEXT(" %i"), pos + 1);
				SendMessage(hStatusWnd, SB_SETTEXT, SB_CURRENT_ROW, (LPARAM)buf);
			}

			if (pHdr->idFrom == IDC_DATAGRID && pHdr->code == (DWORD)LVN_KEYDOWN) {
				NMLVKEYDOWN* kd = (LPNMLVKEYDOWN) lParam;
				if (kd->wVKey == 0x43 && GetKeyState(VK_CONTROL)) // Ctrl + C
					SendMessage(hWnd, WM_COMMAND, IDM_COPY_ROW, 0);
			}

			if (pHdr->code == HDN_ITEMCHANGED && pHdr->hwndFrom == ListView_GetHeader(GetDlgItem(hWnd, IDC_DATAGRID)))
				SendMessage(hWnd, WMU_UPDATE_COLSIZE, 0, 0);
		}
		break;

		case WMU_UPDATE_GRID: {
			HWND hListWnd = GetDlgItem(hWnd, IDC_TABLELIST);
			HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
			HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);
			sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
			SendMessage(hDataWnd, WM_SETREDRAW, FALSE, 0);
			HWND hHeader = ListView_GetHeader(hDataWnd);

			SendMessage(hWnd, WMU_RESET_CACHE, 0, 0);
			ListView_SetItemCount(hDataWnd, 0);

			int colCount = Header_GetItemCount(hHeader);
			for (int colNo = 0; colNo < colCount; colNo++) {
				DestroyWindow(GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo));
				DestroyWindow(GetDlgItem(hHeader, IDC_HEADER_STATIC + colNo));
			}

			for (int colNo = 0; colNo < colCount; colNo++)
				ListView_DeleteColumn(hDataWnd, colCount - colNo - 1);

			char* tablename8 = (char*)GetProp(hWnd, TEXT("TABLENAME8"));
			int pos = ListBox_GetCurSel(hListWnd);
			TCHAR name16[256];
			ListBox_GetText(hListWnd, pos, name16);
			char* name8 = utf16to8(name16);
			sprintf(tablename8, name8);
			free(name8);

			TCHAR buf[255];
			int type = SendMessage(hListWnd, LB_GETITEMDATA, pos, 0);
			_sntprintf(buf, 255, type ? TEXT("  TABLE"): TEXT("   VIEW"));
			SendMessage(hStatusWnd, SB_SETTEXT, SB_TYPE, (LPARAM)buf);
			SendMessage(hStatusWnd, SB_SETTEXT, SB_CURRENT_ROW, (LPARAM)0);

			ListView_SetItemCount(hDataWnd, 0);

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

					LVCOLUMN lvc;
					lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT;
					lvc.iSubItem = colNo;
					lvc.pszText = name16;
					lvc.cchTextMax = _tcslen(name16) + 1;
					lvc.cx = 100;
					lvc.fmt = fmt;
					ListView_InsertColumn(hDataWnd, colNo, &lvc);
					free(name16);
				}

				for (int colNo = 0; colNo < colCount; colNo++) {
					RECT rc;
					Header_GetItemRect(hHeader, colNo, &rc);
					HWND hEdit = CreateWindowEx(WS_EX_TOPMOST, WC_EDIT, NULL, ES_CENTER | ES_AUTOHSCROLL | WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hHeader, (HMENU)(INT_PTR)(IDC_HEADER_EDIT + colNo), GetModuleHandle(0), NULL);
					SendMessage(hEdit, WM_SETFONT, (LPARAM)GetProp(hWnd, TEXT("FONT")), TRUE);
					SetProp(hEdit, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)cbNewFilterEdit));
					HWND hStatic = CreateWindowEx(WS_EX_TOPMOST, WC_STATIC, NULL, WS_VISIBLE | WS_CHILD | SS_WHITERECT, 0, 0, 0, 0, hHeader, (HMENU)(INT_PTR)(IDC_HEADER_STATIC + colNo), GetModuleHandle(0), NULL);
					SetProp(hStatic, TEXT("WNDPROC"), (HANDLE)SetWindowLongPtr(hStatic, GWLP_WNDPROC, (LONG_PTR)cbNewFilterStatic));
				}
				SendMessage(hWnd, WMU_UPDATE_COLSIZE, 0, 0);
			}
			sqlite3_finalize(stmt);

			*(int*)GetProp(hWnd, TEXT("ORDERBY")) = 0;
			SendMessage(hWnd, WMU_UPDATE_ROW_COUNT, 0, 0);
			SendMessage(hWnd, WMU_UPDATE_DATA, 0, 0);
			SendMessage(hDataWnd, WM_SETREDRAW, TRUE, 0);

			PostMessage(hWnd, WMU_AUTO_COLSIZE, 0, 0);
		}
		break;

		case WMU_UPDATE_DATA: {
			HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
			*(int*)GetProp(hWnd, TEXT("CACHEOFFSET")) = -1;
			ListView_SetItemCount(hDataWnd, ListView_GetItemCount(hDataWnd));
			UpdateWindow(hDataWnd);
		}
		break;

		case WMU_UPDATE_ROW_COUNT: {
			HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
			HWND hStatusWnd = GetDlgItem(hWnd, IDC_STATUSBAR);
			HWND hHeader = ListView_GetHeader(hDataWnd);
			int colCount = Header_GetItemCount(hHeader);
			sqlite3* db = (sqlite3*)GetProp(hWnd, TEXT("DB"));
			char* tablename8 = (char*)GetProp(hWnd, TEXT("TABLENAME8"));
			char* where8 = (char*)GetProp(hWnd, TEXT("WHERE8"));
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
				*pRowCount = rowCount;

			if (rowCount == -1)
				MessageBox(hWnd, TEXT("Error occured while fetching data"), NULL, MB_OK);
			else
				ListView_SetItemCount(hDataWnd, rowCount);

			TCHAR buf[255];
			_sntprintf(buf, 255, TEXT(" Rows: %i/%i"), rowCount, *pRowCount);
			SendMessage(hStatusWnd, SB_SETTEXT, SB_ROW_COUNT, (LPARAM)buf);

			return rowCount;
		}
		break;

		case WMU_UPDATE_COLSIZE: {
			HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
			HWND hHeader = ListView_GetHeader(hDataWnd);
			int colCount = Header_GetItemCount(hHeader);
			SendMessage(hHeader, WM_SIZE, 0, 0);
			for (int colNo = 0; colNo < colCount; colNo++) {
				RECT rc;
				Header_GetItemRect(hHeader, colNo, &rc);
				SetWindowPos(GetDlgItem(hHeader, IDC_HEADER_STATIC + colNo), 0, rc.left + 1, rc.top + 19, rc.right - rc.left - 2, 3, SWP_NOZORDER);
				SetWindowPos(GetDlgItem(hHeader, IDC_HEADER_EDIT + colNo), 0, rc.left + 1, rc.top + 21, rc.right - rc.left - 2, rc.bottom - rc.top - 22, SWP_NOZORDER);
			}
		}
		break;

		case WMU_AUTO_COLSIZE: {
			HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
			SendMessage(hDataWnd, WM_SETREDRAW, FALSE, 0);
			HWND hHeader = ListView_GetHeader(hDataWnd);
			int colCount = Header_GetItemCount(hHeader);

			for (int colNo = 0; colNo < colCount - 1; colNo++)
				ListView_SetColumnWidth(hDataWnd, colNo, colNo < colCount - 1 ? LVSCW_AUTOSIZE_USEHEADER : LVSCW_AUTOSIZE);

			if (colCount == 1 && ListView_GetColumnWidth(hDataWnd, 0) < 100)
				ListView_SetColumnWidth(hDataWnd, 0, 100);

			if (colCount > 1) {
				int colNo = colCount - 1;
				ListView_SetColumnWidth(hDataWnd, colNo, LVSCW_AUTOSIZE);
				TCHAR name16[1024];
				Header_GetItemText(hHeader, colNo, name16, 1024);
				RECT rc;
				HDC hDC = GetDC(hHeader);
				DrawText(hDC, name16, _tcslen(name16), &rc, DT_NOCLIP | DT_CALCRECT);
				ReleaseDC(hHeader, hDC);

				int w = rc.right - rc.left + 10;
				if (ListView_GetColumnWidth(hDataWnd, colNo) < w)
					ListView_SetColumnWidth(hDataWnd, colNo, w);
			}

			SendMessage(hDataWnd, WM_SETREDRAW, TRUE, 0);
			InvalidateRect(hDataWnd, NULL, TRUE);

			PostMessage(hWnd, WMU_UPDATE_COLSIZE, 0, 0);
		}
		break;

		case WMU_RESET_CACHE: {
			HWND hDataWnd = GetDlgItem(hWnd, IDC_DATAGRID);
			TCHAR*** cache = (TCHAR***)GetProp(hWnd, TEXT("CACHE"));
			*(int*)GetProp(hWnd, TEXT("CACHEOFFSET")) = -1;
			int colCount = Header_GetItemCount(ListView_GetHeader(hDataWnd));
			if (colCount == 0)
				return 0;
			for (int rowNo = 0;  rowNo < CACHE_SIZE; rowNo++) {
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
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK cbNewFilterEdit(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg){
		// Prevent beep
		case WM_CHAR: {
			if (wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_TAB)
				return 0;
		}
		break;

		case WM_KEYDOWN: {
			if (wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_TAB) {
				if (wParam == VK_RETURN) {
					HWND hHeader = GetParent(hWnd);
					HWND hDataWnd = GetParent(hHeader);
					HWND hMainWnd = GetParent(hDataWnd);
					SendMessage(hMainWnd, WMU_UPDATE_ROW_COUNT, 0, 0);
					SendMessage(hMainWnd, WMU_UPDATE_DATA, 0, 0);
				}

				return 0;
			}
		}
		break;

		case WM_DESTROY: {
			RemoveProp(hWnd, TEXT("WNDPROC"));
		}
		break;
	}

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK cbNewFilterStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_ERASEBKGND) {
		RECT rc;
		GetClientRect(hWnd, &rc);
		FillRect((HDC)wParam, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

		return TRUE;
	}

	if (msg == WM_PAINT) {
		RECT rc;
		GetClientRect(hWnd, &rc);

		HPEN hPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DLIGHT));

		PAINTSTRUCT ps = {0};
		ps.fErase = TRUE;
		HDC hDC = BeginPaint(hWnd, &ps);
		SelectObject(hDC, hPen);

		MoveToEx(hDC, 0, 0, 0);
		LineTo(hDC, rc.right, 0);

		DeleteObject(hPen);
		EndPaint(hWnd, &ps);

		return TRUE;
	}

	if (msg == WM_DESTROY)
		RemoveProp(hWnd, TEXT("WNDPROC"));

	return CallWindowProc((WNDPROC)GetProp(hWnd, TEXT("WNDPROC")), hWnd, msg, wParam, lParam);
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
