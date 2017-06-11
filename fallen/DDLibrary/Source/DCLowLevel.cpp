// ========================================================
//
// THIS IS RIPPED FROM THE DC EXAMPLE CODE
//
// ========================================================


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Copyright (c) 1996, 1997 Microsoft Corporation

Module Name:

    DSUtil.cpp

Abstract:

   Contains routines for handling sounds from resources

-------------------------------------------------------------------*/


#include <tchar.h>
#include <windows.h>
#include "MFStdLib.h"
#include <dsound.h>
#include <math.h>
#include <SDL_audio.h>
#ifdef TARGET_DC
#include <floatmathlib.h>

// On DC, it's called "pows" for some reason ("s" = "single"?)
#define powf pows

#endif

// ++++ Global Variables ++++++++++++++++++++++++++++++++++++++++++++
LPDIRECTSOUND			g_pds = NULL;              // The DirectSound object
LPDIRECTSOUND3DLISTENER g_pds3dl = NULL;
LPDIRECTSOUNDBUFFER		g_pdsbPrimary = NULL;

HRESULT                 g_errLast;

#define CheckError(anything) (FALSE)


#ifdef TARGET_DC
// Stream Yamaha ADPCM, not normal PCM.
#define STREAMING_BUFFER_IS_ADPCM defined

// Pants hack for the ADPCM buffer.
//#define CRAPPY_STREAMING_ADPCM_HACK defined
#endif


//extern HWND g_hwndApp;

extern volatile HWND hDDLibWindow;


bool m_bSoundHasActuallyStartedPlaying = FALSE;

#ifdef DCLL_OUTPUT_STEREO_WAVS
FILE *DCLL_handle;
#endif


#ifdef DEBUG
#define SHARON TRACE
//#define SHARON sizeof
#else
#define SHARON sizeof
#endif


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

	InitDirectSound

Description:

	Initialize the DirectSound object

Arguments:

	None

Return Value:

	TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/
BOOL
InitDirectSound()
{
	// Create the DirectSound object
	g_errLast = DirectSoundCreate(NULL, &g_pds, NULL);

	if (CheckError(TEXT("DirectSoundCreate")))
		return FALSE;

	// Set the DirectSound cooperative level
	if (g_pds->SetCooperativeLevel(hDDLibWindow, DSSCL_NORMAL) != DS_OK)
	{
		ASSERT(0);
	}

	return TRUE;
}


BOOL
InitDirectSound3D()
{
	DSBUFFERDESC    dsbd;

	// Create the primary sound buffer (needed for the listener interface)
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D;
	g_errLast = g_pds->CreateSoundBuffer(&dsbd, &g_pdsbPrimary, NULL);
	if (CheckError(TEXT("Create SoundBuffer")))
		return FALSE;

	// Get a pointer to the IDirectSound3DListener interface
	g_errLast = g_pdsbPrimary->QueryInterface(IID_IDirectSound3DListener, (void **)&g_pds3dl);
	if (CheckError(TEXT("QueryInterface for IDirectSound3DListener interface")))
		return FALSE;

	//
	// How many metres in an UC block? 256 => 2 metres?
	//

	g_pds3dl->SetDistanceFactor(0.25F / 256.0F, DS3D_IMMEDIATE);

	// We no longer need the primary buffer, just the Listener interface
	// g_pdsbPrimary->Release();

	// Set the doppler factor to the maximum, so we can more easily notice it
	// g_errLast = g_pds3dl->SetDopplerFactor(DS3D_MAXDOPPLERFACTOR, DS3D_IMMEDIATE);

	//
	// Set the primary buffer playing all the time to avoid nasty clicks...
	//

	g_pdsbPrimary->Play(0, 0, 0);

	return TRUE;
}

bool LoadWave(const char* fileName, unsigned char** buffer, unsigned int* size, WAVEFORMATEX* format)
{
	SDL_AudioSpec spec;
	if (!SDL_LoadWAV(fileName, &spec, buffer, size) || spec.format != AUDIO_S16)
	{
		TRACE("Could not load sound file: %s\n", fileName);
		return false;
	}

	memset(format, 0, sizeof(WAVEFORMATEX));
	format->wFormatTag = WAVE_FORMAT_PCM;
	format->nChannels = spec.channels;
	format->nSamplesPerSec = spec.freq;
	format->wBitsPerSample = 16;
	format->nBlockAlign = format->wBitsPerSample / 8 * format->nChannels;
	format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;

	return true;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    FillSoundBuffer

Description:

    Copies the Sound data to the specified DirecSoundBuffer's data file
    
Arguments:

    LPCTSTR      tszName         -  Name of the WAV file to load

    WAVEFORMATEX **ppWaveHeader  -  Fill this with pointer to wave header

    BYTE         **ppbWaveData   -  Fill this with pointer to wave data

    DWORD        **pcbWaveSize   -  Fill this with wave data size.

Return Value:

    TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/
BOOL
FillSoundBuffer(IDirectSoundBuffer *pdsb, BYTE *pbWaveData, DWORD dwWaveSize)
{
    LPVOID pMem1, pMem2;
    DWORD  dwSize1, dwSize2;

    if (!pdsb || !pbWaveData || !dwWaveSize)
        return FALSE;

    g_errLast = pdsb->Lock(0, dwWaveSize, &pMem1, &dwSize1, &pMem2, &dwSize2, 0);

	switch(g_errLast)
	{
		case DS_OK:
			break;

		case DSERR_BUFFERLOST:		OutputDebugString("DSERR_BUFFERLOST:      \n"); break;
		case DSERR_INVALIDCALL:		OutputDebugString("DSERR_INVALIDCALL:     \n"); break;
		case DSERR_INVALIDPARAM:	OutputDebugString("DSERR_INVALIDPARAM:    \n"); break;
		case DSERR_PRIOLEVELNEEDED:	OutputDebugString("DSERR_PRIOLEVELNEEDED: \n"); break;

		default:
			ASSERT(0);
			break;
	}

    if (CheckError(TEXT("Lock SoundBuffer")))
        return FALSE;


// Have you learned nothing of what I have taught you, young Adami?
    memcpy(pMem1, pbWaveData, dwSize1);

    if (dwSize2 != 0)
        memcpy(pMem2, pbWaveData+dwSize1, dwSize2);

    pdsb->Unlock(pMem1, dwSize1, pMem2, dwSize2);

    return TRUE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    LoadSoundBuffer

Description:

    Creates a DirectSoundBuffer and loads the specified file into it.
    
Arguments:

    LPCTSTR      tszName         -  Name of the WAV file to load

Return Value:

    TRUE on success, FALSE on failure.

-------------------------------------------------------------------*/

SLONG DCLL_bytes_of_sound_memory_used = 0;

#ifdef DEBUG
DWORD dwTotalSampleSize = 0;
#endif

IDirectSoundBuffer *
LoadSoundBuffer(char *szName, SLONG is_3d)
{
    IDirectSoundBuffer *pdsb = NULL;

	DCLL_bytes_of_sound_memory_used = 0;

	SDL_AudioSpec spec;
	unsigned char* audioBuf;
	unsigned int audioLen;
	WAVEFORMATEX format;
    if (LoadWave(szName, &audioBuf, &audioLen, &format))
    {
		DSBUFFERDESC dsbd = { 0 };

        dsbd.dwSize = sizeof(dsbd);
        dsbd.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2;

		dsbd.dwBufferBytes = audioLen;
		dsbd.lpwfxFormat = &format;


		if (is_3d)
		{
			//dsbd.dwFlags |= DSBCAPS_CTRL3D | DSBCAPS_MUTE3DATMAXDISTANCE;
		}

#ifdef TARGET_DC
		// Do a compact, just in case we got fragmentation, though it's unlikely.
		HRESULT hres = g_pds->Compact();
		ASSERT ( SUCCEEDED ( hres ) );
#endif

		pdsb = NULL;

        if (SUCCEEDED(g_pds->CreateSoundBuffer(&dsbd, &pdsb, NULL)))
        {
            if (!FillSoundBuffer(pdsb, audioBuf, dsbd.dwBufferBytes))
            {
                pdsb->Release();
                pdsb = NULL;
            }

			DCLL_bytes_of_sound_memory_used = dsbd.dwBufferBytes;


			#ifdef TARGET_DC
			
			DSBCAPS dsbcaps;
			ZeroMemory ( (void *)&dsbcaps, sizeof ( dsbcaps ) );
			dsbcaps.dwSize = sizeof ( dsbcaps );
			pdsb->GetCaps ( &dsbcaps );

			if ( dsbcaps.dwFlags & DSBCAPS_LOCHARDWARE )
			{
				//SHARON ( "Hardware buffer 0x%x bytes, %i CPU overhead\n", dsbcaps.dwBufferBytes, dsbcaps.dwPlayCpuOverhead );
				// Make sure this is the right length.
				if ( ( dsbd.dwBufferBytes & 31 ) != 0 )
				{
					TRACE ( "Hardware buffer wasn't a multiple of 32 bytes in length - don't loop it! %i \n", (dsbd.dwBufferBytes & 31) );
				}
				if ( dsbd.dwBufferBytes > 16383 )
				{
					TRACE ( "Hardware buffer more than 32k samples - don't loop it! %i \n", (dsbd.dwBufferBytes) );
				}
			}
			else
			{
				SHARON ( "Software buffer <%s>, 0x%x bytes, %iHz\n", szName, dsbcaps.dwBufferBytes, dsbd.lpwfxFormat->nSamplesPerSec );
				SHARON ( "Binning it for now.\n" );
				ASSERT ( FALSE );
				pdsb->Release();
				pdsb = NULL;
			}

#if 0
#ifdef DEBUG

			CBYTE *words_for_11khz[] =
			{
				"Suspension",
				"Scrape",
				"Balrog",
				"Footstep",
				"PAIN",
				"TAUNT",
				"darci\\",
				"roper\\"
				"bthug1\\",
				"wthug1\\",
				"wthug2\\",
				"cop\\COP",
				"misc\\",
				"Barrel",
				"Ambience",
				"CarX",
				"!"
			};

			// Shall we chop these down even more?
			bool bChopIt = FALSE;
			for (int j = 0; words_for_11khz[j][0] != '!'; j++)
			{
				if (strstr(szName, words_for_11khz[j]))
				{
					bChopIt = TRUE;
					break;
				}
			}


			if ( bChopIt )
			{
				// Chop it down to 8kHz.
				dwTotalSampleSize += ( dsbcaps.dwBufferBytes * 8000 / dsbd.lpwfxFormat->nSamplesPerSec );
			}
			else if ( dsbd.lpwfxFormat->nSamplesPerSec > 11100 )
			{
				// Pretend you halved the sample rate.
				dwTotalSampleSize += dsbcaps.dwBufferBytes / 2;
			}
			else
			{
				dwTotalSampleSize += dsbcaps.dwBufferBytes;
			}
			SHARON ( "Total desired sample memory 0x%x\n", dwTotalSampleSize );
#endif
#endif

			#endif

        }
        else
		{
            pdsb = NULL;
		}
		SDL_FreeWAV(audioBuf);
    }

    return pdsb;
}


IDirectSoundBuffer *CreateStreamingSoundBuffer(WAVEFORMATEX* format, DWORD dwBufferSize)
{
    IDirectSoundBuffer *pdsb        = NULL;
    DSBUFFERDESC       dsbd         = {0};
    
    dsbd.dwSize                  = sizeof(dsbd);
    dsbd.dwBufferBytes           = dwBufferSize;
    dsbd.dwFlags                 = DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY;// | DSBCAPS_LOCSOFTWARE;
    dsbd.lpwfxFormat             = format;

    g_errLast = g_pds->CreateSoundBuffer(&dsbd, &pdsb, NULL);
    if (CheckError(TEXT("Create DirectSound Buffer")))
        return NULL;

    return pdsb;
}

void SetupNofications(IDirectSoundBuffer* buffer, HANDLE event, unsigned int bufferSize)
{
	IDirectSoundNotify *pdsn;
	DSBPOSITIONNOTIFY   rgdsbpn[2];

	buffer->QueryInterface(IID_IDirectSoundNotify, (void **)&pdsn);

	rgdsbpn[0].hEventNotify = event;
	rgdsbpn[1].hEventNotify = event;
	rgdsbpn[0].dwOffset = bufferSize / 2;
	rgdsbpn[1].dwOffset = bufferSize - 2;

	pdsn->SetNotificationPositions(2, rgdsbpn);
	pdsn->Release();
}


// ========================================================
//
// FROM HERE ONWARDS IT'S MY CODE!
//
// ========================================================

#include "dclowlevel.h"


typedef struct dcll_sound
{
	SLONG used;
	IDirectSoundBuffer   *dsb;
	IDirectSound3DBuffer *dsb_3d;

} DCLL_Sound;

#define DCLL_MAX_SOUNDS 1024

DCLL_Sound DCLL_sound[DCLL_MAX_SOUNDS];






//
// Nasty streaming stuff...
//

// How many bytes to do at a time.
#define DCLL_GRANULARITY 256

#ifdef STREAMING_BUFFER_IS_ADPCM

// We need a much smaller buffer for 1 second's worth.
#define DCLL_STREAM_BUFFER_SIZE ( DCLL_GRANULARITY * 64 )

#else
//#define DCLL_STREAM_BUFFER_SIZE (22050 * sizeof(UWORD) * 1)		// Number of bytes for 1 seconds of 22khz 16-bit mono sound
#define DCLL_STREAM_BUFFER_SIZE ( DCLL_GRANULARITY * 128 )
#endif


#define DCLL_STREAM_COMMAND_NOTHING         0
#define DCLL_STREAM_COMMAND_START_NEW_SOUND 1

SLONG DCLL_stream_loop;
SLONG DCLL_stream_command;
CBYTE DCLL_stream_new_fname[MAX_PATH];
SLONG DCLL_stream_data_offset;
SLONG DCLL_stream_silence_count;

IDirectSoundBuffer *DCLL_stream_dsb = nullptr;
Uint8*				DCLL_stream_sound_buffer = nullptr;
unsigned int		DCLL_stream_sound_buffer_length;
unsigned int		DCLL_stream_sound_buffer_position;
HANDLE              DCLL_stream_event;
HANDLE              DCLL_stream_thread;			// Streaming sound thread
SLONG				DCLL_stream_volume_value = DSBVOLUME_MAX;
float				DCLL_stream_volume_range = 1.0F;


// Used for buffering streaming reads.
char pcDCLL_stream_buffer[DCLL_GRANULARITY];


void DCLL_stream_set_volume_range(float max_vol)
{
	SATURATE(max_vol, 0.0F, 1.0F);

	//DCLL_stream_volume_range = max_vol;

	DCLL_stream_volume(max_vol);

}


#if 0

#define NUM_TRACIES 16
#define MAX_LENGTH_TRACIE 100


char pcTracieString[NUM_TRACIES][MAX_LENGTH_TRACIE];
int m_iDumpedTracieNum = 0;
int m_iTracieNum = 0;


void Tracie ( char *fmt, ... )
{
	va_list va;
	va_start ( va, fmt );
	vsprintf ( pcTracieString[m_iTracieNum++], fmt, va );
	if ( m_iTracieNum >= NUM_TRACIES )
	{
		m_iTracieNum = 0;
	}
	va_end ( va );
}


void DumpTracies ( void )
{
	while ( m_iDumpedTracieNum != m_iTracieNum )
	{
		TRACE ( pcTracieString[m_iDumpedTracieNum] );
		m_iDumpedTracieNum++;
		if ( m_iDumpedTracieNum >= NUM_TRACIES )
		{
			m_iDumpedTracieNum = 0;
		}
	}
}


// A sort of pseudo-trace thingie for threads.
// Use it like TRACIE(("thing %i\n", iThing ));
#define TRACIE(arg) Tracie arg;


#else

#define TRACIE sizeof

#endif




// Set this to 0 for new-fangled goodness on the DC.
#define OLD_STYLE_THING_THAT_DIDNT_WORK_VERY_WELL 0



//
// The thread that handles streaming.
//

DWORD DCLL_stream_process(int nUnused)
{
	while(1)
	{
		WaitForSingleObject(DCLL_stream_event, INFINITE);

		//
		// What's hapenned to our event?
		//
		switch(DCLL_stream_command)
		{
			case DCLL_STREAM_COMMAND_NOTHING:
				{
					DWORD write_pos;
					SLONG start;

					//
					// Find out which bit of the buffer we should overwrite.
					//

					DWORD dummy;

					DCLL_stream_dsb->GetCurrentPosition(&write_pos, &dummy);

					// Very occasionally, the read position is just before the end of the buffer, which causes this to fall over.
					// So add a little something to tweak it.
#define SOUND_POS_TWEAK_FACTOR 64
					if ( ( write_pos >= ( ( DCLL_STREAM_BUFFER_SIZE / 2 ) - SOUND_POS_TWEAK_FACTOR ) ) &&
						 ( write_pos <  ( ( DCLL_STREAM_BUFFER_SIZE ) - SOUND_POS_TWEAK_FACTOR ) ) )
					{
						//
						// Overwrite the first half.
						//

						start = 0;
						TRACIE (( "Write pos 0x%x - overwriting first half\n", write_pos ));
					}
					else
					{
						//
						// Overwrite the second half.
						//

						start = DCLL_STREAM_BUFFER_SIZE / 2;
						TRACIE (( "Write pos 0x%x - overwriting second half\n", write_pos ));
					}

					if (DCLL_stream_sound_buffer == NULL)
					{
						//
						// No more sound data, so fill the sound buffer
						// with silence.
						//

						DCLL_stream_silence_count += 1;

						if (DCLL_stream_silence_count >= 2)
						{
							//
							// We can safely stop the sound from playing.
							//

							DCLL_stream_silence_count = 0;

							DCLL_stream_dsb->Stop();

							break;
						}
					}

					UBYTE *block1_mem = NULL;
					UBYTE *block2_mem = NULL;
					DWORD block1_length;
					DWORD block2_length;

					DCLL_stream_dsb->Lock(start, DCLL_STREAM_BUFFER_SIZE / 2, (void **) &block1_mem, &block1_length, (void **) &block2_mem, &block2_length, 0 );

					if ( block1_mem == NULL )
					{
						// NADS! Has happened though. Which is pretty damn scary.
						ASSERT ( FALSE );
						TRACE (( "Eek - the Lock just failed for the sound\n." ));
					}
					else
					{

						if (DCLL_stream_sound_buffer == NULL)
						{
							//
							// Silence...
							//

							// Can't use memset - must use DWORD writes.
							//memset(block1_mem, 0, block1_length);
							DWORD *dst1 = (DWORD *)block1_mem;
							DWORD count = block1_length;

							count >>= 2;
							while ( (count--) > 0 )
							{
								*dst1++ = 0;
							}
						}
						else
						{
							DWORD bytes_available = min(DCLL_stream_sound_buffer_length - DCLL_stream_sound_buffer_position, block1_length);

							memcpy(block1_mem, DCLL_stream_sound_buffer + DCLL_stream_sound_buffer_position, bytes_available);
							DCLL_stream_sound_buffer_position += bytes_available;

							if (bytes_available < block1_length)
							{
								if (DCLL_stream_loop)
								{
									//
									// Go back to the beginning of the buffer and
									// start looping again.
									//
									DCLL_stream_sound_buffer_position = 0;

									memcpy(block1_mem + bytes_available, DCLL_stream_sound_buffer, block1_length - bytes_available);

									DCLL_stream_sound_buffer_position += block1_length - bytes_available;
								}
								else
								{
									//
									// Fill the remainder of the buffer with silence and free the buffer.
									//

									// Can't use memset - must be DWORD writes.
									//memset(block1_mem + bytes_read, 0, block1_length - bytes_read);
									DWORD *dst1 = (DWORD *)( block1_mem + bytes_available);
									DWORD count = block1_length - bytes_available;

									count >>= 2;
									while ( (count--) > 0 )
									{
										*dst1++ = 0;
									}

									SDL_FreeWAV (DCLL_stream_sound_buffer);

									DCLL_stream_sound_buffer = nullptr;
								}
							}
						}
		
						DCLL_stream_dsb->Unlock(block1_mem, block1_length, block2_mem, block2_length);
					}
				}

				break;

			case DCLL_STREAM_COMMAND_START_NEW_SOUND:

				{
					DWORD status = 0;

					if(DCLL_stream_dsb)
					{
						DCLL_stream_dsb->GetStatus(&status);

						if (status & (DSBSTATUS_LOOPING | DSBSTATUS_PLAYING))
						{
							//
							// If the buffer is playing, then we free
							// the current buffer and stop the sound playing.
							//

							if (DCLL_stream_sound_buffer)
							{
								SDL_FreeWAV(DCLL_stream_sound_buffer);

								DCLL_stream_sound_buffer = NULL;
								DCLL_stream_silence_count = 0;
							}

							DCLL_stream_dsb->Stop();
						}
					}

					{
						ULONG         bytes_read;
						DWORD         size;
						WAVEFORMATEX pwfx;
						BYTE         *data_start;

						SDL_AudioSpec spec;
						if( !LoadWave(DCLL_stream_new_fname, &DCLL_stream_sound_buffer, &DCLL_stream_sound_buffer_length, &pwfx))
						{
							DCLL_stream_sound_buffer = nullptr;

							DCLL_stream_dsb->Stop();
						}
						else
						{
							if (DCLL_stream_dsb)
							{
								DCLL_stream_dsb->Stop();
								DCLL_stream_dsb = nullptr;
							}
							DCLL_stream_dsb = CreateStreamingSoundBuffer(&pwfx, DCLL_STREAM_BUFFER_SIZE);
							DCLL_stream_dsb->SetVolume(DCLL_stream_volume_value);
							SetupNofications(DCLL_stream_dsb, DCLL_stream_event, DCLL_STREAM_BUFFER_SIZE);

							DCLL_stream_sound_buffer_position = 0;

							//
							// Fill up the sound buffer with the data.
							//

							UBYTE *block1_mem = NULL;
							UBYTE *block2_mem = NULL;
							DWORD block1_length;
							DWORD block2_length;

							DCLL_stream_dsb->Lock(0, 0, (void **) &block1_mem, &block1_length, (void **) &block2_mem, &block2_length, DSBLOCK_ENTIREBUFFER);

							if (block1_mem)
							{
								DWORD bytes_available = min(DCLL_stream_sound_buffer_length - DCLL_stream_sound_buffer_position, block1_length);

								memcpy(block1_mem, DCLL_stream_sound_buffer + DCLL_stream_sound_buffer_position, bytes_available);
								DCLL_stream_sound_buffer_position += bytes_available;
								if (bytes_available < block1_length)
								{
									//
									// Fill the remainder of the buffer with silence and free the buffer.
									//
								
									// Can't use memset - must use DWORD writes.
									//memset(block1_mem + bytes_read, 0, block1_length - bytes_read);
									DWORD *dst1 = (DWORD *)( block1_mem + bytes_available);
									DWORD count = block1_length - bytes_available;
									count >>= 2;
									while ( (count--) > 0 )
									{
										*dst1++ = 0;
									}
									SDL_FreeWAV ( DCLL_stream_sound_buffer );

									DCLL_stream_sound_buffer = nullptr;
								}

							}

							DCLL_stream_dsb->Unlock(block1_mem, block1_length, block2_mem, block2_length);

							ASSERT(block2_mem == NULL);

							//
							// Start the buffer playing.
							//

							DCLL_stream_dsb->SetCurrentPosition(0);
							DCLL_stream_dsb->Play(0, 0, DSBPLAY_LOOPING);
						}
					}
				}

				DCLL_stream_command = DCLL_STREAM_COMMAND_NOTHING;

				break;

			default:
				ASSERT(0);
				break;
		}
	}

	return 0;

}

void DCLL_stream_init(void)
{
	DWORD thread_id;

	//
	// Create the event that we use to communicate with
	// the streaming thread.
	//

	DCLL_stream_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	//
	// Spawn off the new thread.
	//

	DCLL_stream_thread  = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) DCLL_stream_process, NULL, 0, &thread_id);
	ASSERT(DCLL_stream_thread);
}

SLONG DCLL_stream_play(CBYTE *fname, SLONG loop)
{
	if (DCLL_stream_command != DCLL_STREAM_COMMAND_NOTHING)
	{
		//
		// Waiting to play another sample.
		//

		if (loop && !DCLL_stream_loop)
		{
			return FALSE;
		}
	}

	//
	// The command to the thread.
	//

	DCLL_stream_loop    = loop;
	DCLL_stream_command = DCLL_STREAM_COMMAND_START_NEW_SOUND;

#ifdef TARGET_DC
	// Some filenames begin with ".\" which confuses the poor DC
	if ( ( fname[0] == '.' ) && ( fname[1] == '\\' ) )
	{
		fname += 2;
	}
#endif

	// Some missions have empty briefing names.
	if ( fname[0] == '\0' )
	{
		return FALSE;
	}


#ifndef TARGET_DC
	strcpy(DCLL_stream_new_fname, fname);
#else
	// Some filename have more than one '.', which GDWorkshop converts to an underline.
	// They are all of the format blah.ucmfoobar.wav, which changes to
	// blah.ucmfoobar_wav
	char *pcSrc = fname;
	char *pcDst = DCLL_stream_new_fname;
	bool bFoundFullStop = FALSE;
	while ( *pcSrc != '\0' )
	{
		*pcDst = *pcSrc++;
		if ( *pcDst == '.' )
		{
			if ( bFoundFullStop )
			{
				*pcDst = '_';
			}
			else
			{
				bFoundFullStop = TRUE;
			}
		}
		pcDst++;
	}
	*pcDst = '\0';


#if 0
// No longer needed - everything is ADPCM.
#ifdef STREAMING_BUFFER_IS_ADPCM
	// If the file ends in MONO, add a further AD to it.
	int iTemp = strlen ( DCLL_stream_new_fname );
	if ( ( DCLL_stream_new_fname[iTemp-8] == 'M' ) &&
		 ( DCLL_stream_new_fname[iTemp-7] == 'O' ) &&
		 ( DCLL_stream_new_fname[iTemp-6] == 'N' ) &&
		 ( DCLL_stream_new_fname[iTemp-5] == 'O' ) )
	{
		strcpy ( &(DCLL_stream_new_fname[iTemp-4]), "AD.wav" );
	}
#endif
#endif


#endif

	//
	// Wake up the process.
	//

	SetEvent(DCLL_stream_event);

	return TRUE;
}


void DCLL_stream_stop()
{
	if (!DCLL_stream_dsb)
	{
		return;
	}
	//
	// Make sure a sound doesn't sneakily start playing...
	//

	DCLL_stream_command = DCLL_STREAM_COMMAND_NOTHING;

	DCLL_stream_dsb->Stop();

	//
	// Free the streaming buffer and stop the buffer from playing.
	//

	if (DCLL_stream_sound_buffer)
	{
		SDL_FreeWAV(DCLL_stream_sound_buffer);

		DCLL_stream_sound_buffer = NULL;
		DCLL_stream_silence_count = 0;
	}

	while(1)
	{
		ULONG   status;
		HRESULT res;

		res = DCLL_stream_dsb->GetStatus(&status);

		ASSERT(res == DS_OK);

		if ((status & DSBSTATUS_PLAYING) ||
			(status & DSBSTATUS_LOOPING))
		{
			Sleep(10);

			DCLL_stream_dsb->Stop();
		}
		else
		{
			return;
		}
	}
}

SLONG DCLL_stream_is_playing()
{
	ULONG   status;
	HRESULT res;

	if (!DCLL_stream_dsb)
	{
		return FALSE;
	}

	if (DCLL_stream_command == DCLL_STREAM_COMMAND_START_NEW_SOUND)
	{
		return TRUE;
	}

	res = DCLL_stream_dsb->GetStatus(&status);

	ASSERT(res == DS_OK);

	if ((status & DSBSTATUS_PLAYING) ||
		(status & DSBSTATUS_LOOPING))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


// Returns TRUE if the stream has actually started playing,
// i.e. the disk has seeked, etc.
bool DCLL_stream_has_started_streaming ( void )
{
	ULONG   status;
	HRESULT res;

	res = DCLL_stream_dsb->GetStatus(&status);

	ASSERT(res == DS_OK);

	if ((status & DSBSTATUS_PLAYING) ||
		(status & DSBSTATUS_LOOPING))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}



void DCLL_stream_wait()
{
	while(1)
	{
		if (DCLL_stream_is_playing())
		{
			Sleep(10);
		}
		else
		{
			return;
		}
	}
}

void DCLL_stream_volume(float volume)
{
	if (DCLL_stream_dsb == NULL)
	{
		return;
	}

	SATURATE(volume, 0.0F, 1.0F);

	// Done elsewhere.
	//volume *= DCLL_stream_volume_range;

	volume = powf( volume, 0.1F);

	DCLL_stream_volume_value = SLONG(DSBVOLUME_MIN + (DSBVOLUME_MAX - DSBVOLUME_MIN) * volume);

	DCLL_stream_dsb->SetVolume(DCLL_stream_volume_value);
}







void DCLL_memstream_init();


void DCLL_init()
{
	InitDirectSound();
	InitDirectSound3D();

	memset(DCLL_sound, 0, sizeof(DCLL_sound));

	DCLL_stream_init();
	DCLL_memstream_init();

	#ifdef DCLL_OUTPUT_STEREO_WAVS

	DCLL_handle = fopen("d:\\stereowavs.txt", "wb");

	#endif
}





DCLL_Sound *DCLL_load_sound(CBYTE *fname)
{
	SLONG i;

	DCLL_Sound *ds;

	for (i = 0; i < DCLL_MAX_SOUNDS; i++)
	{
		ds = &DCLL_sound[i];

		if (!ds->used)
		{
			goto found_unused_sound;
		}
	}

	//
	// No more sounds left!
	//

	ASSERT(0);

	return NULL;

found_unused_sound:;

	ds->used = TRUE;

	ds->dsb  = LoadSoundBuffer(fname, TRUE);

	if (!ds->dsb)
	{
		ds->used = FALSE;

		return NULL;
	}

	ASSERT(ds->dsb);

	(ds->dsb)->QueryInterface(IID_IDirectSound3DBuffer, (void **) &ds->dsb_3d);

	return ds;
}


void DCLL_set_volume(DCLL_Sound *ds, float volume)
{
	if (ds == NULL)
	{
		return;
	}
	
	SATURATE(volume, 0.0F, 1.0F);

	volume = powf(volume, 0.1F);

	SLONG ivolume = SLONG(DSBVOLUME_MIN + (DSBVOLUME_MAX - DSBVOLUME_MIN) * volume);

	HRESULT hres = ds->dsb->SetVolume(ivolume);

	//TRACE ( "Vol %i\n", ivolume );

}

SLONG	DCLL_still_playing(DCLL_Sound *ds)
{
	ULONG status;
	if(ds==NULL)
		return(0);
	if(ds->dsb==NULL)
		return(0);

	ds->dsb->GetStatus(&status);
	if ((status & DSBSTATUS_PLAYING) || (status & DSBSTATUS_LOOPING))
	{
		return(1);
	}
	else
		return(0);


}


void DCLL_2d_play_sound(DCLL_Sound *ds, SLONG flag)
{
	if (ds == NULL)
	{
		return;
	}

	ULONG status;

	ASSERT ( ds->dsb != NULL );
	if ( ds->dsb_3d != NULL )
	{
		ds->dsb_3d->SetMode(DS3DMODE_DISABLE, DS3D_IMMEDIATE);
	}

	ds->dsb->GetStatus(&status);

	if ((status & DSBSTATUS_PLAYING) ||
		(status & DSBSTATUS_LOOPING))
	{
		if (flag & DCLL_FLAG_INTERRUPT)
		{
			ds->dsb->SetCurrentPosition(0);
		}
	}
	else
	{
		switch(ds->dsb->Play(0, 0, (flag & DCLL_FLAG_LOOP) ? DSBPLAY_LOOPING : 0))
		{
			case DS_OK:
				break;

			case DSERR_PRIOLEVELNEEDED:
				ASSERT(0);
				break;

			default:
				ASSERT(0);
				break;
		}
	}
}

float DCLL_3d_listener_x;
float DCLL_3d_listener_y;
float DCLL_3d_listener_z;



void DCLL_3d_play_sound(DCLL_Sound *ds, float x, float y, float z, SLONG flag)
{
	if (ds == NULL)
	{
		return;
	}

	ASSERT(ds->dsb);
	ASSERT(ds->dsb_3d);

	ds->dsb_3d->SetMode(DS3DMODE_NORMAL, DS3D_IMMEDIATE);

	//
	// Start playing- or restart if it's already playing.
	//

	ULONG status;

	ds->dsb->GetStatus(&status);

	if ((status & DSBSTATUS_PLAYING) ||
		(status & DSBSTATUS_LOOPING))
	{
		if (flag & DCLL_FLAG_INTERRUPT)
		{
			ds->dsb->SetCurrentPosition(0);
		}
	}
	else
	{
		ds->dsb->Play(0,0,(flag & DCLL_FLAG_LOOP) ? DSBPLAY_LOOPING : 0);
	}

	//
	// Set position.
	//

	ds->dsb_3d->SetPosition(x,y,z, DS3D_IMMEDIATE);
}


void DCLL_3d_set_listener(
		float x,
		float y,
		float z,
		float matrix[9])
{
	DCLL_3d_listener_x = x;
	DCLL_3d_listener_y = y;
	DCLL_3d_listener_z = z;

	if (!g_pds3dl)
	{
		return;
	}

	g_pds3dl->SetPosition(x, y, z, DS3D_DEFERRED);

	g_pds3dl->SetOrientation(
		matrix[6],
		matrix[7],
		matrix[8],
		matrix[3],
		matrix[4],
		matrix[5],
		DS3D_DEFERRED);

	g_pds3dl->CommitDeferredSettings();
}


void DCLL_stop_sound(DCLL_Sound *ds)
{
	if (ds == NULL)
	{
		return;
	}

	//ds->dsb->Stop();
}


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(arg) if ( (arg) != NULL ) { (arg)->Release(); (arg) = NULL; }
#endif


void DCLL_free_sound(DCLL_Sound *ds)
{
	if (ds == NULL || !ds->used)
	{
		return;
	}

	SAFE_RELEASE ( ds->dsb_3d );
	SAFE_RELEASE ( ds->dsb );
	ds->used   = FALSE;
}


void DCLL_fini(void)
{
	SLONG i;

	DCLL_Sound *ds;

	for (i = 0; i < DCLL_MAX_SOUNDS; i++)
	{
		ds = &DCLL_sound[i];

		if (ds->used)
		{
			DCLL_free_sound(ds);
		}
	}

	//
	// Release objects...
	//

	g_pds3dl->Release();
	g_pdsbPrimary->Release();
	g_pds->Release();

}







void init_my_dialog(HWND h)
{
}

void my_dialogs_over(HWND h)
{
}

void MilesTerm(void)
{
}

// ========================================================
//
// STREAMING FILES FROM MEMORY
//
// ========================================================

//
// The sound to stream.
//

UBYTE *DCLL_memstream_sound = nullptr;
unsigned int  DCLL_memstream_sound_length;	// length in bytes
UBYTE *DCLL_memstream_sound_upto;		// Where we have played upto.


#define DCLL_MEMSTREAM_BUFFER_SIZE (32768)

IDirectSoundBuffer *DCLL_memstream_dsb = nullptr;
HANDLE              DCLL_memstream_event;
HANDLE              DCLL_memstream_thread;			// Streaming sound thread
SLONG				DCLL_memstream_volume_value = DSBVOLUME_MAX;


//
// The thread that handles memstreaming.
//

DWORD DCLL_memstream_process(void)
{
	while(1)
	{
		WaitForSingleObject(DCLL_memstream_event, INFINITE);


// Debugging - disable it.
#if 1



		{
			DWORD write_pos;
			SLONG start;

			//
			// Find out which bit of the buffer we should overwrite.
			//

			DWORD dummy;

			#ifdef TARGET_DC
			// We need to find the write position.
#if OLD_STYLE_THING_THAT_DIDNT_WORK_VERY_WELL
			DCLL_memstream_dsb->GetCurrentPosition(&dummy, &write_pos);
#else
			DCLL_memstream_dsb->GetCurrentPosition(&write_pos, &dummy);
#endif
			#else
			// Different on the PC.
			DCLL_memstream_dsb->GetCurrentPosition(&write_pos, &dummy);
			#endif



#if OLD_STYLE_THING_THAT_DIDNT_WORK_VERY_WELL
			if ( write_pos >= ( DCLL_STREAM_BUFFER_SIZE / 2 ) )
#else
			if ( ( write_pos >= ( ( DCLL_STREAM_BUFFER_SIZE / 2 ) - SOUND_POS_TWEAK_FACTOR ) ) &&
				 ( write_pos <  ( ( DCLL_STREAM_BUFFER_SIZE ) - SOUND_POS_TWEAK_FACTOR ) ) )
#endif
			{
				//
				// Overwrite the first half.
				//

				start = 0;
				TRACIE (( "Memstream: writing first half\n" ));
			}
			else
			{
				//
				// Overwrite the second half.
				//

				start = DCLL_MEMSTREAM_BUFFER_SIZE / 2;
				TRACIE (( "Memstream: writing second half\n" ));
			}

			if (DCLL_memstream_sound == NULL)
			{
				//
				// No more sound data so stop the buffer! Something
				// bad must be happening.
				//

				DCLL_memstream_dsb->Stop();
			}

			UBYTE *block1_mem    = NULL;
			UBYTE *block2_mem    = NULL;
			DWORD  block1_length = 0;
			DWORD  block2_length = 0;

#if OLD_STYLE_THING_THAT_DIDNT_WORK_VERY_WELL
			DCLL_memstream_dsb->Lock(start, DCLL_MEMSTREAM_BUFFER_SIZE / 2, (void **) &block1_mem, &block1_length, (void **) &block2_mem, &block2_length, DSBLOCK_FROMWRITECURSOR);
#else
			DCLL_memstream_dsb->Lock(start, DCLL_MEMSTREAM_BUFFER_SIZE / 2, (void **) &block1_mem, &block1_length, (void **) &block2_mem, &block2_length, 0);
#endif


			//
			// Fill the buffer with data.
			//

			SLONG  i;
			SLONG  count = block1_length >> 2;
			SLONG *dst   = (SLONG *)  block1_mem;
			SLONG *src   = (SLONG *)  DCLL_memstream_sound_upto;
			SLONG *end   = (SLONG *) (DCLL_memstream_sound + DCLL_memstream_sound_length);

			#ifdef TARGET_DC

			ASSERT((SLONG(dst) & 3) == 0);
			ASSERT((count      & 3) == 0);

			#endif

			for (i = count; i > 0; i--)
			{
				*dst++ = *src++;

				if (src >= end)
				{
					src = (SLONG *) DCLL_memstream_sound;
				}
			}

			DCLL_memstream_sound_upto = (UBYTE *) src;

			DCLL_memstream_dsb->Unlock(block1_mem, block1_length, block2_mem, block2_length);
		}


#endif


	}	

	// Doesn't ever exit, but the DC compiler
	// complains that it doesn't return a value.
	return 0;
}



void DCLL_memstream_init()
{
	DWORD thread_id;

	//
	// Create the event that we use to communicate with the
	// memstream thread.
	//

	DCLL_memstream_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	//
	// Spawn off the new thread.
	//

	DCLL_memstream_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) DCLL_memstream_process, NULL, 0, &thread_id);

	ASSERT(DCLL_memstream_thread);

	#ifdef TARGET_DC
	SetThreadPriority(DCLL_memstream_thread, THREAD_PRIORITY_HIGHEST);
	#endif
}

void DCLL_memstream_load(CBYTE *fname)
{
	BYTE          temp[256];
	ULONG         bytes_read;
	DWORD         size;
	WAVEFORMATEX pwfx;
	BYTE         *data_start;

	//
	// Stop the buffer.
	//
	if (DCLL_memstream_dsb)
	{
		DCLL_memstream_dsb->Stop();
	}

	//
	// Initialise the sound.
	//

	DCLL_memstream_unload();

	//
	// Open the sound file.
	//

	if (!LoadWave(fname, &DCLL_memstream_sound, &DCLL_memstream_sound_length, &pwfx))
	{
		return;
	}
	DCLL_memstream_sound_upto = DCLL_memstream_sound;

	if (DCLL_memstream_dsb)
	{
		DCLL_memstream_dsb->Stop();
		DCLL_memstream_dsb = nullptr;
	}
	DCLL_memstream_dsb = CreateStreamingSoundBuffer(&pwfx, DCLL_MEMSTREAM_BUFFER_SIZE);
	DCLL_memstream_dsb->SetVolume(DCLL_memstream_volume_value);
	SetupNofications(DCLL_memstream_dsb, DCLL_memstream_event, DCLL_MEMSTREAM_BUFFER_SIZE);

	//
	// Lock the sound buffer.
	//

	UBYTE *block1_mem    = NULL;
	UBYTE *block2_mem    = NULL;
	DWORD  block1_length = 0;
	DWORD  block2_length = 0;

	DCLL_memstream_dsb->Lock(0, 0, (void **) &block1_mem, &block1_length, (void **) &block2_mem, &block2_length, DSBLOCK_ENTIREBUFFER);

	//
	// Make sure what happens is what we expect.
	//

	ASSERT(block2_mem    == NULL);
	ASSERT(block1_length == DCLL_MEMSTREAM_BUFFER_SIZE);
	ASSERT(block2_length == 0);

	//
	// Fill the buffer with data.
	//

	SLONG  i;
	SLONG  count = DCLL_MEMSTREAM_BUFFER_SIZE >> 2;
	SLONG *dst   = (SLONG *) block1_mem;
	SLONG *src   = (SLONG *) DCLL_memstream_sound;
	SLONG *end   = (SLONG *) (DCLL_memstream_sound + DCLL_memstream_sound_length);

	ASSERT((SLONG(dst) & 3) == 0);
	ASSERT((count      & 3) == 0);

	for (i = count; i > 0; i--)
	{
		*dst++ = *src++;

		if (src >= end)
		{
			src = (SLONG *) DCLL_memstream_sound;
		}
	}

	//
	// Unlock the buffer.
	//

	DCLL_memstream_dsb->Unlock(block1_mem, block1_length, block2_mem, block2_length);
}

void DCLL_memstream_volume(float volume)
{
	if (DCLL_memstream_dsb == NULL)
	{
		return;
	}

	SATURATE(volume, 0.0F, 1.0F);

	volume = powf( volume, 0.1F);

	DCLL_memstream_volume_value = SLONG(DSBVOLUME_MIN + (DSBVOLUME_MAX - DSBVOLUME_MIN) * volume);

	DCLL_memstream_dsb->SetVolume(DCLL_memstream_volume_value);
}


void DCLL_memstream_play()
{
	//
	// Start the buffer playing.
	//

	ASSERT(DCLL_memstream_sound);

#if 0
	// Don't reset it! It's so short, and if you reset it,
	// it sounds a bit crappy. Just start playing it again from the
	// current position.
	DCLL_memstream_dsb->SetCurrentPosition(0);
#endif
	DCLL_memstream_dsb->Play(0, 0, DSBPLAY_LOOPING);

	DCLL_memstream_volume(DCLL_stream_volume_range * 0.5F);
}

void DCLL_memstream_stop()
{
	//
	// Stop the sound.
	//
	if (DCLL_memstream_dsb)
	{
		DCLL_memstream_dsb->Stop();
	}
}

void DCLL_memstream_unload()
{
	Sleep(100);	// Give the streaming thread enough time to finish what it's doing!

	if (DCLL_memstream_sound)
	{
		SDL_FreeWAV(DCLL_memstream_sound);
	}

	DCLL_memstream_sound        = NULL;
	DCLL_memstream_sound_length = 0;
	DCLL_memstream_sound_upto   = NULL;
}

