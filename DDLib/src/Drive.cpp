// drive.cpp
//
// CD-ROM drives

#include "ddlib.h"
#include "drive.h"
#include "..\..\fallen\Headers\Env.h"

static char Path[MAX_PATH];			// CD-ROM path
static bool	TexturesCD;				// textures on CD?
static bool	SFX_CD;					// sound effects on CD?
static bool	MoviesCD;				// movies on CD?
static bool	SpeechCD;				// speech on CD?

// Exit
//
// exit on failure

static void Exit(void)
{
#ifndef TARGET_DC
	MessageBoxA(NULL, "Cannot locate Urban Chaos CD-ROM", NULL, MB_ICONERROR);
#else
	ASSERT(FALSE);
#endif
	exit(1);
}

// LocateCDROM
//
// locate the CD drive containing the Urban Chaos CD


#ifdef TARGET_DC


void LocateCDROM(void)
{
	// Er... I know where it is on the DreamCast :-)
#ifdef FILE_PC
#error SPONG!
	// It's on the PC.
	strcpy ( Path, "\\PC\\" );
#else
	// It's on the DC itself.
	strcpy ( Path, "\\CD-ROM\\" );
#endif

}


#else //#ifdef TARGET_DC


void LocateCDROM(void)
{
	TexturesCD = ENV_get_value_number((CBYTE*)"textures", 1, (CBYTE*)"LocalInstall") ? false : true;
	SFX_CD = ENV_get_value_number((CBYTE*)"sfx", 1, (CBYTE*)"LocalInstall") ? false : true;
	MoviesCD = ENV_get_value_number((CBYTE*)"movies", 1, (CBYTE*)"LocalInstall") ? false : true;
	SpeechCD = ENV_get_value_number((CBYTE*)"speech", 1, (CBYTE*)"LocalInstall") ? false : true;

	if (!TexturesCD && !SFX_CD && !MoviesCD && !SpeechCD)	return;	// don't need CD-ROM

	char	strings[256];

	if (!GetLogicalDriveStringsA(255, strings))
	{
		MessageBoxA(NULL, "Cannot locate system devices - serious internal error", NULL, MB_ICONERROR);
		exit(0);
	}

	for (;;)
	{
		char*	sptr = strings;

		while (*sptr)
		{
			if (GetDriveTypeA(sptr) == DRIVE_CDROM)
			{
				char	filename[MAX_PATH];

				sprintf(filename, "%sclumps\\mib.txc", sptr); // fallen.ex_ doesnt exist on eidos funny fanny builds

				FILE*	fd = MF_Fopen(filename, "rb");
				
				if (fd)
				{
					// found it!
					MF_Fclose(fd);
					strcpy(Path, sptr);
					return;
				}
			}

			sptr += strlen(sptr) + 1;
		}

		if (MessageBoxA(NULL, "Cannot locate Urban Chaos CD-ROM", NULL, MB_ICONERROR | MB_RETRYCANCEL) == IDCANCEL)
		{
			break;
		}
	}
	exit(0);
}


#endif //#else //#ifdef TARGET_DC

// Get*Path

static inline const char* GetPath(bool on_cd)	{ return (on_cd ? Path : ".\\"); }

char* GetCDPath(void)		{ return Path; }
char* GetTexturePath(void)	{ return (char*)GetPath(TexturesCD); }
char* GetSFXPath(void)		{ return (char*)GetPath(SFX_CD); }
char* GetMoviesPath(void)	{ return (char*)GetPath(MoviesCD); }
char* GetSpeechPath(void)	{ return (char*)GetPath(SpeechCD); }
