#include "../include/MFStdLib.h"


BOOL	LibShellMessage(const char* pMessage, const char* pFile, ULONG dwLine)
{
	BOOL		result;
	CBYTE		buff1[512],
		buff2[512];
	ULONG		flag;

	if (pMessage == NULL)
	{
		pMessage = "Looks like a coder has caught a bug.";
	}

	TRACE((char*)"LbShellMessage: %s\n", pMessage);

#ifndef TARGET_DC
	wsprintf((LPWSTR)buff1, (LPCWSTR)"Uh oh, something bad's happened!");
	wsprintf((LPWSTR)buff2, (LPCWSTR)"%s\n\n%s\n\nIn   : %s\nline : %u", buff1, pMessage, pFile, dwLine);
	strcat_s(buff2, "\n\nAbort=Kill Application, Retry=Debug, Ignore=Continue");
	flag = MB_ABORTRETRYIGNORE | MB_ICONSTOP | MB_DEFBUTTON3;

	result = FALSE;

	// TODO: Review has had to be removed due to Display class coupling
	//the_display.toGDI();

	/*switch (MessageBox(hwnd, (LPCWSTR)buff2, (LPCWSTR)"Mucky Foot Message", flag))
	{
	case	IDABORT:
		exit(1);
		break;
	case	IDCANCEL:
	case	IDIGNORE:
		break;
	case	IDRETRY:
		DebugBreak();
		break;
	}*/

	// TODO: Review
	//the_display.fromGDI();
#else
	result = TRUE;
#endif

	return	result;
}


