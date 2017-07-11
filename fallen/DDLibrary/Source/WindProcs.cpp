// WindProcs.cpp
// Guy Simmons, 14th November 1997.

#include	"DDLib.h"
//#include	"finaleng.h"
#include	"BinkClient.h"
#include	"..\headers\music.h"
#include	"..\headers\game.h"

extern void MFX_QUICK_stop(void);

#ifndef TARGET_DC


//---------------------------------------------------------------

BOOL	DlgDriversInit(HWND hDlg)
{
	SLONG			result;
	ChangeDDInfo	*change_info;
	DDDriverInfo	*current_driver,
					*driver_list;
	GUID			*DD_guid;
	TCHAR			szBuff[80];


	// Check Parameters
	change_info	=	(ChangeDDInfo*)(void *)GetWindowLong (hDlg, DWL_USER);
	if(!change_info)
		return FALSE;

	current_driver	=	change_info->DriverNew;

	// Validate Driver
	if(!current_driver)
		DD_guid	=	NULL;
	else
		DD_guid	=	current_driver->GetGuid();
	current_driver	=	ValidateDriver(DD_guid);
	if(!current_driver)
		return FALSE;

	// Dump Driver list to Combo Box
	driver_list	=	the_manager.DriverList;
	while(driver_list)
	{
		if(driver_list->IsPrimary())
			wsprintf(szBuff,TEXT("%s (Primary)"),driver_list->szName);
		else
			wsprintf(szBuff,TEXT("%s"),driver_list->szName);

		// Add String to Combo Box
		result	=	SendDlgItemMessage(hDlg, IDC_DRIVERS, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBuff);

		// Set up pointer to driver for this item
		SendDlgItemMessage(hDlg, IDC_DRIVERS, CB_SETITEMDATA, (WPARAM)result, (LPARAM)(void *)driver_list);

		// Is it the current Driver
		if(current_driver==driver_list)
		{
			// Set as our current selection
			SendDlgItemMessage (hDlg, IDC_DRIVERS, CB_SETCURSEL, (WPARAM)result, 0L);
		}
		driver_list	=	driver_list->Next;
	}

	// Success
	return TRUE;
}


BOOL	DlgDevicesInit(HWND hDlg)
{
	SLONG			result;
	ChangeDDInfo	*change_info;
	D3DDeviceInfo	*current_device,
					*device_list;
	DDDriverInfo	*current_driver;
	GUID			*D3D_guid;
	TCHAR			szBuff[80];


	// Check Parameters
	change_info	=	(ChangeDDInfo*)(void *)GetWindowLong(hDlg, DWL_USER);
	if(!change_info)
		return FALSE;

	current_driver	=	change_info->DriverNew;
	if(!current_driver)
		return FALSE;

	current_device	=	change_info->DeviceNew;

	// Validate Device
	if(!current_device)
		D3D_guid	=	NULL;
	else
		D3D_guid	=	&(current_device->guid);
	current_device	=	ValidateDevice(current_driver, D3D_guid, NULL);
	if(!current_device)
		return FALSE;
	
	// Dump Device list to Combo Box
	device_list	=	current_driver->DeviceList;
	while(device_list)
	{
		// Get Device String
		wsprintf(szBuff,TEXT("%s"),device_list->szName);

		// Add String to Combo Box
		result	=	SendDlgItemMessage(hDlg, IDC_DEVICES, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBuff);

		// Set up pointer to device for this item
		SendDlgItemMessage(hDlg, IDC_DEVICES, CB_SETITEMDATA, (WPARAM)result, (LPARAM)(void *)device_list);

		// Is it the current device
		if(current_device==device_list)
		{
			// Set as our current selection
			SendDlgItemMessage(hDlg, IDC_DEVICES, CB_SETCURSEL, (WPARAM)result, 0L);
		}
		device_list	=	device_list->Next;
	}

	// Success
	return TRUE;
}
    

int _cdecl CompareModes(const void *element1, const void *element2)
{
	int				compare	=	0;
	SLONG			w1,w2,
					h1,h2,
					bpp1,bpp2,
					refresh1,refresh2;
	DDModeInfo		*mode1,
					*mode2;


    mode1	=	*((DDModeInfo**)element1);
    mode2	=	*((DDModeInfo**)element2);

	// Handle Null pointers
	if((!mode1) && (!mode2))
		return	0;
	if((mode1) && (!mode2))
		return	-1;
	if((!mode1) && (mode2))
		return	1;

	// Get Mode Data
	mode1->GetMode(&w1,&h1,&bpp1,&refresh1);
	mode2->GetMode(&w2,&h2,&bpp2,&refresh2);

	// Sort first on BPP then width then height
    if(bpp1 < bpp2)
		compare	=	-1;
    else if(bpp1 > bpp2)
		compare	=	1;
    else if(w1 < w2)
		compare	=	-1;
    else if(w1 > w2)
		compare	=	1;
    else if(h1 < h2)
		compare	=	-1;
    else if(h1 > h2)
		compare	=	1;

	// Equality
    return	compare;
}


BOOL DlgModesInit (HWND hDlg)
{
	SLONG			buffer_size,
					index,
					mode_count,
					result,
					w,h,bpp,refresh;
	ChangeDDInfo	*change_info;
	D3DDeviceInfo	*current_device;
	DDDriverInfo	*current_driver;
	DDModeInfo		*current_mode,
					*mode_list,
					**mode_buffer;
	TCHAR			szBuff[80];


	// Check Parameters
	change_info	=	(ChangeDDInfo*)(void *)GetWindowLong(hDlg, DWL_USER);
	if(!change_info)
		return FALSE;

	current_driver	=	change_info->DriverNew;
	current_device	=	change_info->DeviceNew;
	current_mode	=	change_info->ModeNew;

	if(!current_driver)
		return FALSE;

	if(!current_device)
	{
		current_device	=	ValidateDevice(current_driver, NULL, NULL);
		if(!current_device)
			return FALSE;
	}

	mode_count	=	current_driver->CountModes();
	if(!mode_count)
		return FALSE;
	
	if(!current_mode)
	{
		current_mode	=	ValidateMode(
											current_driver,
											DEFAULT_WIDTH,
											DEFAULT_HEIGHT,
											DEFAULT_DEPTH,
											0,
											current_device
										);
	}

	// Get memory for Mode list
	buffer_size	=	mode_count * sizeof(DDModeInfo*);
	mode_buffer	=	(DDModeInfo**)MemAlloc(buffer_size);
	if(!mode_buffer)
		return	FALSE;

	// Create Mode List
	index		=	0;
	mode_list	=	current_driver->ModeList;
	while(mode_list)
	{
		mode_buffer[index]	=	mode_list;

		index++;

		mode_list	=	mode_list->Next;
	}

	// Sort Mode list
    qsort((void*)mode_buffer,(size_t)mode_count,sizeof(DDModeInfo*),CompareModes);

	// Dump Mode list to Combo Box
	for(index=0;index<mode_count;index++)
	{
		// Make sure mode is supported by D3D device
		if((mode_buffer[index]) && (mode_buffer[index]->ModeSupported(current_device)))
		{
			mode_buffer[index]->GetMode(&w,&h,&bpp,&refresh);
			
			// Set up Mode String
			if(refresh)
				wsprintf(szBuff, TEXT("%4d x %4d x %4d (%4d Hz)"), w, h, bpp, refresh);
			else
				wsprintf (szBuff, TEXT("%4d x %4d x %4d"), w, h, bpp);

			// Add String to Combo Box
			result	=	SendDlgItemMessage(hDlg, IDC_MODES, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBuff);

			// Set up pointer to Mode Info for this item
			SendDlgItemMessage(hDlg, IDC_MODES, CB_SETITEMDATA, (WPARAM)result, (LPARAM)(void *)mode_buffer[index]);

			// Is it the current Mode 
			if(current_mode==mode_buffer[index])
			{
				// Set as our current selection
				SendDlgItemMessage (hDlg, IDC_MODES, CB_SETCURSEL, (WPARAM)result, 0L);
			}
		}
	}	

	// Cleanup Memory
	MemFree(mode_buffer);

	// Success
	return TRUE;
}

//---------------------------------------------------------------

DDDriverInfo	*DlgGetDriver(HWND hDlg)
{
	SLONG			index;


	index	=	SendDlgItemMessage(hDlg, IDC_DRIVERS, CB_GETCURSEL, 0, 0);
	if(index!=CB_ERR)
	{
		// Return pointer to driver
		return	(DDDriverInfo*)SendDlgItemMessage(hDlg, IDC_DRIVERS, CB_GETITEMDATA, (WPARAM)index, (LPARAM)0);
	}			

	// Failure
	return NULL;
}

//---------------------------------------------------------------

D3DDeviceInfo	*DlgGetDevice(HWND hDlg)
{
	SLONG	index;
	
	
	index	=	SendDlgItemMessage(hDlg, IDC_DEVICES, CB_GETCURSEL, 0, 0);
	if(index!=CB_ERR)
	{
		// Get pointer to device
		return	(D3DDeviceInfo*)SendDlgItemMessage(hDlg, IDC_DEVICES, CB_GETITEMDATA, (WPARAM)index, (LPARAM)0);
	}			

	// Failure
	return NULL;
}

//---------------------------------------------------------------

DDModeInfo	*DlgGetMode(HWND hDlg)
{
	SLONG	index;


	index	 =	SendDlgItemMessage(hDlg, IDC_MODES, CB_GETCURSEL, 0, 0);
	if(index!=CB_ERR)
	{
		// Get pointer to device
		return	(DDModeInfo*)SendDlgItemMessage(hDlg, IDC_MODES, CB_GETITEMDATA, (WPARAM)index, (LPARAM)0);
	}			

	// Failure
	return NULL;
}

#endif //#ifndef TARGET_DC


//---------------------------------------------------------------

SLONG app_inactive;
SLONG restore_surfaces;

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam);

LRESULT	CALLBACK	DDLibShellProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int				result;
	SLONG			w,h,bpp,refresh;
	ChangeDDInfo	change_info;
	GUID			*D3D_guid,
					*DD_guid;
	HMENU			hMenu;
	HINSTANCE		hInstance;

#ifndef TARGET_DC
	BinkMessage(hWnd, message, wParam, lParam);
#endif
	
	switch(message)
	{
#ifndef TARGET_DC
		case WM_ACTIVATEAPP:

			if (!wParam)
			{
				//
				// Lost focus...
				//

				app_inactive = TRUE;
			}
			else
			{
				app_inactive     = FALSE;
				restore_surfaces = TRUE;
			}

			break;

	    case WM_SIZE:
	    case WM_MOVE:

			//
			// Tell the new engine the that window has moved.
			//

//			FINALENG_window_moved(LOWORD(lParam),HIWORD(lParam));

	        if(the_display.IsFullScreen())
	        {
				SetRect(&the_display.DisplayRect,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
	        }
			else
			{
	            GetClientRect(hWnd,&the_display.DisplayRect);
	            ClientToScreen(hWnd,(LPPOINT)&the_display.DisplayRect);
	            ClientToScreen(hWnd,(LPPOINT)&the_display.DisplayRect+1);
			}
			break;
		case	WM_MOUSEMOVE:
		case	WM_RBUTTONUP:
		case	WM_RBUTTONDOWN:
		case	WM_RBUTTONDBLCLK:
		case	WM_LBUTTONUP:
		case	WM_LBUTTONDOWN:
		case	WM_LBUTTONDBLCLK:
		case	WM_MBUTTONUP:
		case	WM_MBUTTONDOWN:
		case	WM_MBUTTONDBLCLK:
			MouseProc(message,wParam,lParam);
			break;
#endif //#ifndef TARGET_DC

		case	WM_KEYDOWN:
		case	WM_KEYUP:
			KeyboardProc(message,wParam,lParam);
			break;
		case	WM_CLOSE:
			// normally, we should call DestroyWindow().
			// instead, let's set flags so the normal quit process goes thru.
			GAME_STATE=0;
			break;
		case	WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return	DefWindowProc(hWnd,message,wParam,lParam);
	}
	return	0;
}

//---------------------------------------------------------------

