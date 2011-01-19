/************************************************************************

	IMPORTANT NOTE : this file contains two clearly delimited sections : 
	the ARCHITECTURE section (in two parts) and the USER section. Each section 
	is governed by its own copyright and license. Please check individually 
	each section for license and copyright information.
*************************************************************************/

/*******************BEGIN ARCHITECTURE SECTION (part 1/2)****************/

/************************************************************************
    FAUST Architecture File
	Copyright (C) 2003-2011 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This Architecture section is free software; you can redistribute it 
    and/or modify it under the terms of the GNU General Public License 
	as published by the Free Software Foundation; either version 3 of 
	the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License 
	along with this program; If not, see <http://www.gnu.org/licenses/>.

	EXCEPTION : As a special exception, you may create a larger work 
	that contains this FAUST architecture section and distribute  
	that work under terms of your choice, so long as this FAUST 
	architecture section is not modified. 


 ************************************************************************
 ************************************************************************/


/**
DEFAULT ALSA PARAMETERS CONTROLLED BY ENVIRONMENT VARIABLES

Some default parameters of Faust's ALSA applications are controlled by the following environment variables :

    FAUST2ALSA_DEVICE   = "hw:0"
    FAUST2ALSA_FREQUENCY= 44100
    FAUST2ALSA_BUFFER   = 1024
    FAUST2ALSA_PERIODS  = 2

*/


/* link with : "" */
#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>
#include <pwd.h>
#include <sys/types.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <pthread.h> 
#include <sys/wait.h>
#include <list>

#include <iostream>
#include <fstream>



using namespace std;

// handle 32/64 bits int size issues

#ifdef __x86_64__

#define uint32	unsigned int
#define uint64	unsigned long int

#define int32	int
#define int64	long int

#else

#define uint32	unsigned int
#define uint64	unsigned long long int

#define int32	int
#define int64	long long int
#endif

// check 32/64 bits issues are correctly handled

#define CHECKINTSIZE \
	assert(sizeof(int32)==4);\
	assert(sizeof(int64)==8);




// On Intel set FZ (Flush to Zero) and DAZ (Denormals Are Zero)
// flags to avoid costly denormals
#ifdef __SSE__
    #include <xmmintrin.h>
    #ifdef __SSE2__
        #define AVOIDDENORMALS _mm_setcsr(_mm_getcsr() | 0x8040)
    #else
        #define AVOIDDENORMALS _mm_setcsr(_mm_getcsr() | 0x8000)
    #endif
#else
    #define AVOIDDENORMALS 
#endif

//#define BENCHMARKMODE

// g++ -Wall -O3 -lm -lpthread -lasound `gtk-config --cflags --libs` test.cpp -o test

#define check_error(err) if (err) { printf("%s:%d, alsa error %d : %s\n", __FILE__, __LINE__, err, snd_strerror(err)); exit(1); }
#define check_error_msg(err,msg) if (err) { fprintf(stderr, "%s:%d, %s : %s(%d)\n", __FILE__, __LINE__, msg, snd_strerror(err), err); exit(1); }
#define display_error_msg(err,msg) if (err) { fprintf(stderr, "%s:%d, %s : %s(%d)\n", __FILE__, __LINE__, msg, snd_strerror(err), err); }

#define max(x,y) (((x)>(y)) ? (x) : (y))
#define min(x,y) (((x)<(y)) ? (x) : (y))

// abs is now predefined
//template<typename T> T abs (T a)			{ return (a<T(0)) ? -a : a; }


inline int		lsr (int x, int n)			{ return int(((unsigned int)x) >> n); }


inline int int2pow2 (int x)	{ int r=0; while ((1<<r)<x) r++; return r; }


/**
 * Used to set the priority and scheduling of the audio thread 
 */
bool setRealtimePriority ()
{
    struct passwd *         pw;
    int                     err;
    uid_t                   uid;
    struct sched_param      param;  

    uid = getuid ();
    pw = getpwnam ("root");
    setuid (pw->pw_uid); 
    param.sched_priority = 50; /* 0 to 99  */
    err = sched_setscheduler(0, SCHED_RR, &param); 
    setuid (uid);
    return (err != -1);
}


/******************************************************************************
*******************************************************************************

							       VECTOR INTRINSICS

*******************************************************************************
*******************************************************************************/

//inline void *aligned_calloc(size_t nmemb, size_t size) { return (void*)((unsigned)(calloc((nmemb*size)+15,sizeof(char)))+15 & 0xfffffff0); }
//inline void *aligned_calloc(size_t nmemb, size_t size) { return (void*)((size_t)(calloc((nmemb*size)+15,sizeof(char)))+15 & ~15); }


<<includeIntrinsic>>

#ifdef BENCHMARKMODE

/**
 * Returns the number of clock cycles elapsed since the last reset
 * of the processor
 */
static __inline__ uint64 rdtsc(void)
{
	union {
		uint32 i32[2];
		uint64 i64;
	} count;
	
	__asm__ __volatile__("rdtsc" : "=a" (count.i32[0]), "=d" (count.i32[1]));

     return count.i64;
}

#define KSKIP 10
#define KMESURE 1024

int mesure = 0;

uint64 starts[KMESURE];
uint64 stops [KMESURE];

#define STARTMESURE starts[mesure%KMESURE] = rdtsc();
#define STOPMESURE stops[mesure%KMESURE] = rdtsc(); mesure = mesure+1;

void printstats()
{
    uint64 low, hi, tot;
    low = hi = tot = (stops[KSKIP] - starts[KSKIP]);

    if (mesure < KMESURE) {
    
        for (int i = KSKIP+1; i<mesure; i++) {
            uint64 m = stops[i] - starts[i];
            if (m<low) low = m;
            if (m>hi) hi = m;
            tot += m;
        }
        cout << low << ' ' << tot/(mesure-KSKIP) << ' ' << hi << endl;

    } else {
    
        for (int i = KSKIP+1; i<KMESURE; i++) {
            uint64 m = stops[i] - starts[i];
            if (m<low) low = m;
            if (m>hi) hi = m;
            tot += m;
        }
        cout << low << ' ' << tot/(KMESURE-KSKIP) << ' ' << hi << endl;

    }    
}

#else

#define STARTMESURE
#define STOPMESURE

#endif

/******************************************************************************
*******************************************************************************

								AUDIO INTERFACE

*******************************************************************************
*******************************************************************************/

enum { kRead = 1, kWrite = 2, kReadWrite = 3 };


/**
 * A convenient class to pass parameters to AudioInterface
 */
class AudioParam
{
  public:
			
	const char*		fCardName;					
	unsigned int	fFrequency;
	unsigned int	fBuffering; 
	unsigned int	fPeriods; 
	
	unsigned int	fSoftInputs;
	unsigned int	fSoftOutputs;
	
  public :
	AudioParam() : 
		fCardName("hw:0"),
		fFrequency(44100),
		fBuffering(512),
		fPeriods(2),
		fSoftInputs(2),
		fSoftOutputs(2)
	{}
	
	AudioParam&	cardName(const char* n)	{ fCardName = n; 		return *this; }
	AudioParam&	frequency(int f)		{ fFrequency = f; 		return *this; }
	AudioParam&	buffering(int fpb)		{ fBuffering = fpb; 	return *this; }
	AudioParam&	periods(int p)			{ fPeriods = p; 		return *this; }
	AudioParam&	inputs(int n)			{ fSoftInputs = n; 		return *this; }
	AudioParam&	outputs(int n)			{ fSoftOutputs = n; 	return *this; }
};



/**
 * An ALSA audio interface
 */
class AudioInterface : public AudioParam
{
 public :
	snd_pcm_t*				fOutputDevice ;		
	snd_pcm_t*				fInputDevice ;			
	snd_pcm_hw_params_t* 	fInputParams;
	snd_pcm_hw_params_t* 	fOutputParams;
	
	snd_pcm_format_t 		fSampleFormat;
	snd_pcm_access_t 		fSampleAccess;
	
	unsigned int			fCardInputs;
	unsigned int			fCardOutputs;
	
	unsigned int			fChanInputs;
	unsigned int			fChanOutputs;
	
	// interleaved mode audiocard buffers
	void*		fInputCardBuffer;
	void*		fOutputCardBuffer;
	
	// non interleaved mode audiocard buffers
	void*		fInputCardChannels[256];
	void*		fOutputCardChannels[256];
	
	// non interleaved mod, floating point software buffers
	float*		fInputSoftChannels[256];
	float*		fOutputSoftChannels[256];

 public :
 
	const char*	cardName()				{ return fCardName;  	}
 	int			frequency()				{ return fFrequency; 	}
	int			buffering()				{ return fBuffering;  	}
	int			periods()				{ return fPeriods;  	}
	
	float**		inputSoftChannels()		{ return fInputSoftChannels;	}
	float**		outputSoftChannels()	{ return fOutputSoftChannels;	}

	
	AudioInterface(const AudioParam& ap = AudioParam()) : AudioParam(ap)
	{
		
		fInputDevice 			= 0;
		fOutputDevice 			= 0;
		fInputParams			= 0;
		fOutputParams			= 0;
	}
	
	
	/**
	 * Open the audio interface
	 */
	void open()
	{
		int err;
		
		// allocation d'un stream d'entree et d'un stream de sortie
		err = snd_pcm_open( &fInputDevice,  fCardName, SND_PCM_STREAM_CAPTURE, 0 ); 	check_error(err)
		err = snd_pcm_open( &fOutputDevice, fCardName, SND_PCM_STREAM_PLAYBACK, 0 ); 	check_error(err)

		// recherche des parametres d'entree
		err = snd_pcm_hw_params_malloc	( &fInputParams ); 	check_error(err);
		setAudioParams(fInputDevice, fInputParams);

		// recherche des parametres de sortie
		err = snd_pcm_hw_params_malloc	( &fOutputParams ); 		check_error(err)
		setAudioParams(fOutputDevice, fOutputParams);
		
		// set the number of physical input and output channels close to what we need
		fCardInputs 	= fSoftInputs;
		fCardOutputs 	= fSoftOutputs;
		
		snd_pcm_hw_params_set_channels_near(fInputDevice, fInputParams, &fCardInputs);
		snd_pcm_hw_params_set_channels_near(fOutputDevice, fOutputParams, &fCardOutputs);

		printf("inputs : %u, outputs : %u\n", fCardInputs, fCardOutputs);

		// enregistrement des parametres d'entree-sortie
		
		err = snd_pcm_hw_params (fInputDevice,  fInputParams );	 	check_error (err);
		err = snd_pcm_hw_params (fOutputDevice, fOutputParams );	check_error (err);

		//assert(snd_pcm_hw_params_get_period_size(fInputParams,NULL) == snd_pcm_hw_params_get_period_size(fOutputParams,NULL));

		// allocation of alsa buffers
		if (fSampleAccess == SND_PCM_ACCESS_RW_INTERLEAVED) {
			fInputCardBuffer = calloc(interleavedBufferSize(fInputParams), 1);
	 		fOutputCardBuffer = calloc(interleavedBufferSize(fOutputParams), 1);
			
		} else {
			for (unsigned int i = 0; i < fCardInputs; i++) {
				fInputCardChannels[i] = calloc(noninterleavedBufferSize(fInputParams), 1);
			}
			for (unsigned int i = 0; i < fCardOutputs; i++) {
				fOutputCardChannels[i] = calloc(noninterleavedBufferSize(fOutputParams), 1);
			}
			
		}
		
		// allocation of floating point buffers needed by the dsp code
		
		fChanInputs = max(fSoftInputs, fCardInputs);		assert (fChanInputs < 256);
		fChanOutputs = max(fSoftOutputs, fCardOutputs);		assert (fChanOutputs < 256);

		for (unsigned int i = 0; i < fChanInputs; i++) {
			fInputSoftChannels[i] = (float*) calloc (fBuffering, sizeof(float));
			for (int j = 0; j < fBuffering; j++) {
				fInputSoftChannels[i][j] = 0.0;
			}
		}

		for (unsigned int i = 0; i < fChanOutputs; i++) {
			fOutputSoftChannels[i] = (float*) calloc (fBuffering, sizeof(float));
			for (int j = 0; j < fBuffering; j++) {
				fOutputSoftChannels[i][j] = 0.0;
			}
		}


	}
	
	
	void setAudioParams(snd_pcm_t* stream, snd_pcm_hw_params_t* params)
	{	
		int	err;

		// set params record with initial values
		err = snd_pcm_hw_params_any	( stream, params ); 	
		check_error_msg(err, "unable to init parameters")

		// set alsa access mode (and fSampleAccess field) either to non interleaved or interleaved
				
		err = snd_pcm_hw_params_set_access (stream, params, SND_PCM_ACCESS_RW_NONINTERLEAVED );
		if (err) {
			err = snd_pcm_hw_params_set_access (stream, params, SND_PCM_ACCESS_RW_INTERLEAVED );
			check_error_msg(err, "unable to set access mode neither to non-interleaved or to interleaved");
		}
		snd_pcm_hw_params_get_access(params, &fSampleAccess);
		

		// search for 32-bits or 16-bits format
		err = snd_pcm_hw_params_set_format (stream, params, SND_PCM_FORMAT_S32);
		if (err) {
			err = snd_pcm_hw_params_set_format (stream, params, SND_PCM_FORMAT_S16);
		 	check_error_msg(err, "unable to set format to either 32-bits or 16-bits");
		}
		snd_pcm_hw_params_get_format(params, &fSampleFormat);
		// set sample frequency
		snd_pcm_hw_params_set_rate_near (stream, params, &fFrequency, 0); 

		// set period and period size (buffering)
		err = snd_pcm_hw_params_set_period_size	(stream, params, fBuffering, 0); 	
		check_error_msg(err, "period size not available");
		
		err = snd_pcm_hw_params_set_periods (stream, params, fPeriods, 0); 			
		check_error_msg(err, "number of periods not available");

	}


	ssize_t interleavedBufferSize (snd_pcm_hw_params_t* params)
	{
		_snd_pcm_format 	format;  	snd_pcm_hw_params_get_format(params, &format);
		snd_pcm_uframes_t 	psize;		snd_pcm_hw_params_get_period_size(params, &psize, NULL);
		unsigned int 		channels; 	snd_pcm_hw_params_get_channels(params, &channels);
		ssize_t bsize = snd_pcm_format_size (format, psize * channels);
		return bsize;
	}


	ssize_t noninterleavedBufferSize (snd_pcm_hw_params_t* params)
	{
		_snd_pcm_format 	format;  	snd_pcm_hw_params_get_format(params, &format);
		snd_pcm_uframes_t 	psize;		snd_pcm_hw_params_get_period_size(params, &psize, NULL);
		ssize_t bsize = snd_pcm_format_size (format, psize);
		return bsize;
	}


	void close()
	{
	}



	/**
	 * Read audio samples from the audio card. Convert samples to floats and take 
	 * care of interleaved buffers
	 */
	void read()
	{
		
		if (fSampleAccess == SND_PCM_ACCESS_RW_INTERLEAVED) {
			
			int count = snd_pcm_readi(fInputDevice, fInputCardBuffer, fBuffering); 	
			if (count<0) { 
				display_error_msg(count, "reading samples");
				 int err = snd_pcm_prepare(fInputDevice);	
				 check_error_msg(err, "preparing input stream");
			}
			
			if (fSampleFormat == SND_PCM_FORMAT_S16) {

				short* 	buffer16b = (short*) fInputCardBuffer;
				for (int s = 0; s < fBuffering; s++) {
					for (unsigned int c = 0; c < fCardInputs; c++) {
						fInputSoftChannels[c][s] = float(buffer16b[c + s*fCardInputs])*(1.0/float(SHRT_MAX));
					}
				}

			} else if (fSampleFormat == SND_PCM_FORMAT_S32) {

				int32* 	buffer32b = (int32*) fInputCardBuffer;
				for (int s = 0; s < fBuffering; s++) {
					for (unsigned int c = 0; c < fCardInputs; c++) {
						fInputSoftChannels[c][s] = float(buffer32b[c + s*fCardInputs])*(1.0/float(INT_MAX));
					}
				}
			} else {

				printf("unrecognized input sample format : %u\n", fSampleFormat);
				exit(1);
			}
			
		} else if (fSampleAccess == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
			
			int count = snd_pcm_readn(fInputDevice, fInputCardChannels, fBuffering); 	
			if (count<0) { 
				display_error_msg(count, "reading samples");
				 int err = snd_pcm_prepare(fInputDevice);	
				 check_error_msg(err, "preparing input stream");
			}
			
			if (fSampleFormat == SND_PCM_FORMAT_S16) {

				for (unsigned int c = 0; c < fCardInputs; c++) {
					short* 	chan16b = (short*) fInputCardChannels[c];
					for (int s = 0; s < fBuffering; s++) {
						fInputSoftChannels[c][s] = float(chan16b[s])*(1.0/float(SHRT_MAX));
					}
				}

			} else if (fSampleFormat == SND_PCM_FORMAT_S32) { 

				for (unsigned int c = 0; c < fCardInputs; c++) {
					int32* 	chan32b = (int32*) fInputCardChannels[c];
					for (int s = 0; s < fBuffering; s++) {
						fInputSoftChannels[c][s] = float(chan32b[s])*(1.0/float(INT_MAX));
					}
				}
			} else {

				printf("unrecognized input sample format : %u\n", fSampleFormat);
				exit(1);
			}
			
		} else {
			check_error_msg(-10000, "unknow access mode");
		}


	}


	/**
	 * write the output soft channels to the audio card. Convert sample 
	 * format and interleaves buffers when needed
	 */
	void write()
	{
		recovery :
				
		if (fSampleAccess == SND_PCM_ACCESS_RW_INTERLEAVED) {
			
			if (fSampleFormat == SND_PCM_FORMAT_S16) {

				short* buffer16b = (short*) fOutputCardBuffer;
				for (int f = 0; f < fBuffering; f++) {
					for (unsigned int c = 0; c < fCardOutputs; c++) {
						float x = fOutputSoftChannels[c][f];
						buffer16b[c + f*fCardOutputs] = short( max(min(x,1.0),-1.0) * float(SHRT_MAX) ) ;
					}
				}

			} else if (fSampleFormat == SND_PCM_FORMAT_S32)  {

				int32* buffer32b = (int32*) fOutputCardBuffer;
				for (int f = 0; f < fBuffering; f++) {
					for (unsigned int c = 0; c < fCardOutputs; c++) {
						float x = fOutputSoftChannels[c][f];
						buffer32b[c + f*fCardOutputs] = int( max(min(x,1.0),-1.0) * float(INT_MAX) ) ;
					}
				}
			} else {

				printf("unrecognized output sample format : %u\n", fSampleFormat);
				exit(1);
			}

			int count = snd_pcm_writei(fOutputDevice, fOutputCardBuffer, fBuffering); 	
			if (count<0) { 
				display_error_msg(count, "w3"); 
				int err = snd_pcm_prepare(fOutputDevice);	
				check_error_msg(err, "preparing output stream");
				goto recovery;
			}
			
			
		} else if (fSampleAccess == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
			
			if (fSampleFormat == SND_PCM_FORMAT_S16) {

				for (unsigned int c = 0; c < fCardOutputs; c++) {
					short* chan16b = (short*) fOutputCardChannels[c];
					for (int f = 0; f < fBuffering; f++) {
						float x = fOutputSoftChannels[c][f];
						chan16b[f] = short( max(min(x,1.0),-1.0) * float(SHRT_MAX) ) ;
					}
				}

			} else if (fSampleFormat == SND_PCM_FORMAT_S32) { 

				for (unsigned int c = 0; c < fCardOutputs; c++) {
					int32* chan32b = (int32*) fOutputCardChannels[c];
					for (int f = 0; f < fBuffering; f++) {
						float x = fOutputSoftChannels[c][f];
						chan32b[f] = int( max(min(x,1.0),-1.0) * float(INT_MAX) ) ;
					}
				}

			} else {

				printf("unrecognized output sample format : %u\n", fSampleFormat);
				exit(1);
			}

			int count = snd_pcm_writen(fOutputDevice, fOutputCardChannels, fBuffering); 	
			if (count<0) { 
				display_error_msg(count, "w3"); 
				int err = snd_pcm_prepare(fOutputDevice);	
				check_error_msg(err, "preparing output stream");
				goto recovery;
			}
			
		} else {
			check_error_msg(-10000, "unknow access mode");
		}
	}


	
	/**
	 *  print short information on the audio device
	 */
	void shortinfo()
	{
		int						err;
		snd_ctl_card_info_t*	card_info;
    	snd_ctl_t*				ctl_handle;
		err = snd_ctl_open (&ctl_handle, fCardName, 0);		check_error(err);
		snd_ctl_card_info_alloca (&card_info);
		err = snd_ctl_card_info(ctl_handle, card_info);		check_error(err);
		printf("%s|%d|%d|%d|%d|%s\n", 
				snd_ctl_card_info_get_driver(card_info),
				fCardInputs, fCardOutputs,
				fFrequency, fBuffering,
				snd_pcm_format_name((_snd_pcm_format)fSampleFormat));
	}
					
	/**
	 *  print more detailled information on the audio device
	 */
	void longinfo()
	{
		int						err;
		snd_ctl_card_info_t*	card_info;
    	snd_ctl_t*				ctl_handle;

		printf("Audio Interface Description :\n");
		printf("Sampling Frequency : %d, Sample Format : %s, buffering : %d\n", 
				fFrequency, snd_pcm_format_name((_snd_pcm_format)fSampleFormat), fBuffering);
		printf("Software inputs : %2d, Software outputs : %2d\n", fSoftInputs, fSoftOutputs);
		printf("Hardware inputs : %2d, Hardware outputs : %2d\n", fCardInputs, fCardOutputs);
		printf("Channel inputs  : %2d, Channel outputs  : %2d\n", fChanInputs, fChanOutputs);
		
		// affichage des infos de la carte
		err = snd_ctl_open (&ctl_handle, fCardName, 0);		check_error(err);
		snd_ctl_card_info_alloca (&card_info);
		err = snd_ctl_card_info(ctl_handle, card_info);		check_error(err);
		printCardInfo(card_info);

		// affichage des infos liees aux streams d'entree-sortie
		if (fSoftInputs > 0)	printHWParams(fInputParams);
		if (fSoftOutputs > 0)	printHWParams(fOutputParams);
	}
	
	void printCardInfo(snd_ctl_card_info_t*	ci)
	{
		printf("Card info (address : %p)\n", ci);
		printf("\tID         = %s\n", snd_ctl_card_info_get_id(ci));
		printf("\tDriver     = %s\n", snd_ctl_card_info_get_driver(ci));
		printf("\tName       = %s\n", snd_ctl_card_info_get_name(ci));
		printf("\tLongName   = %s\n", snd_ctl_card_info_get_longname(ci));
		printf("\tMixerName  = %s\n", snd_ctl_card_info_get_mixername(ci));
		printf("\tComponents = %s\n", snd_ctl_card_info_get_components(ci));
		printf("--------------\n");
	}

	void printHWParams( snd_pcm_hw_params_t* params )
	{
		printf("HW Params info (address : %p)\n", params);
#if 0
		printf("\tChannels    = %d\n", snd_pcm_hw_params_get_channels(params));
		printf("\tFormat      = %s\n", snd_pcm_format_name((_snd_pcm_format)snd_pcm_hw_params_get_format(params)));
		printf("\tAccess      = %s\n", snd_pcm_access_name((_snd_pcm_access)snd_pcm_hw_params_get_access(params)));
		printf("\tRate        = %d\n", snd_pcm_hw_params_get_rate(params, NULL));
		printf("\tPeriods     = %d\n", snd_pcm_hw_params_get_periods(params, NULL));
		printf("\tPeriod size = %d\n", (int)snd_pcm_hw_params_get_period_size(params, NULL));
		printf("\tPeriod time = %d\n", snd_pcm_hw_params_get_period_time(params, NULL));
		printf("\tBuffer size = %d\n", (int)snd_pcm_hw_params_get_buffer_size(params));
		printf("\tBuffer time = %d\n", snd_pcm_hw_params_get_buffer_time(params, NULL));
#endif
		printf("--------------\n");
	}

	
};





/******************************************************************************
*******************************************************************************

								GRAPHIC USER INTERFACE (v2)
								  abstract interfaces

*******************************************************************************
*******************************************************************************/

#include <map>
#include <list>

using namespace std;

struct Meta : map<const char*, const char*>
{
    void declare (const char* key, const char* value) { (*this)[key]=value; }
};


struct uiItem;
typedef void (*uiCallback)(float val, void* data);

/**
* Format tooltip string by replacing some white spaces by 
* return characters so that line width doesn't exceed n.
* Limitation : long words exceeding n are not cut 
*/
static string formatTooltip(int n, const string& tt)
{
	string  ss = tt;	// ss string we are going to format
	int		lws = 0;	// last white space encountered
	int 	lri = 0;	// last return inserted
	for (int i=0; i<tt.size(); i++) {
		if (tt[i] == ' ') lws = i;
		if (((i-lri) >= n) && (lws > lri)) {
			// insert return here
			ss[lws] = '\n';
			lri = lws;
		}
	}
	cout << ss;
	return ss;
}

/**
 * Graphic User Interface : abstract definition
 */

class UI 
{
	typedef list<uiItem*> clist;
	typedef map<float*, clist*> zmap;
	
 private:
 	static list<UI*>	fGuiList;
	zmap				fZoneMap;
	bool				fStopped;
	
 public:
		
	UI() : fStopped(false) {	
		fGuiList.push_back(this);
	}
	
	virtual ~UI() {
		// suppression de this dans fGuiList
	}

	// -- zone management
	
	void registerZone(float* z, uiItem* c)
	{
		if (fZoneMap.find(z) == fZoneMap.end()) fZoneMap[z] = new clist();
		fZoneMap[z]->push_back(c);
	} 	

	// -- saveState(filename) : save the value of every zone to a file
	
	void saveState(const char* filename)	
	{
		ofstream f(filename);
		
		for (zmap::iterator i=fZoneMap.begin(); i!=fZoneMap.end(); i++) { 
			f << *(i->first) << ' ';
		} 
		
		f << endl;
		f.close();
	}

	// -- recallState(filename) : load the value of every zone from a file
	
	void recallState(const char* filename)	
	{
		ifstream f(filename);
		if (f.good()) {
			for (zmap::iterator i=fZoneMap.begin(); i!=fZoneMap.end(); i++) { 
				f >> *(i->first);
			} 
		}
		f.close();
	}
	
	void updateAllZones();
	
	void updateZone(float* z);
	
	static void updateAllGuis()
	{
		list<UI*>::iterator g;
		for (g = fGuiList.begin(); g != fGuiList.end(); g++) {
			(*g)->updateAllZones();
		}
	}
	
	// -- active widgets
	
	virtual void addButton(const char* label, float* zone) = 0;
	virtual void addToggleButton(const char* label, float* zone) = 0;
	virtual void addCheckButton(const char* label, float* zone) = 0;
	virtual void addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step) = 0;
	virtual void addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step) = 0;
	virtual void addNumEntry(const char* label, float* zone, float init, float min, float max, float step) = 0;
	
	// -- passive widgets
	
	virtual void addNumDisplay(const char* label, float* zone, int precision) = 0;
	virtual void addTextDisplay(const char* label, float* zone, const char* names[], float min, float max) = 0;
	virtual void addHorizontalBargraph(const char* label, float* zone, float min, float max) = 0;
	virtual void addVerticalBargraph(const char* label, float* zone, float min, float max) = 0;
	
	void addCallback(float* zone, uiCallback foo, void* data);
	
	// -- widget's layouts
	
	virtual void openFrameBox(const char* label) = 0;
	virtual void openTabBox(const char* label) = 0;
	virtual void openHorizontalBox(const char* label) = 0;
	virtual void openVerticalBox(const char* label) = 0;
	virtual void closeBox() = 0;
	
	virtual void show() = 0;
	virtual void run() = 0;
	
	void stop()		{ fStopped = true; }
	bool stopped() 	{ return fStopped; }

    virtual void declare(float* zone, const char* key, const char* value) {}
};


/**
 * User Interface Item: abstract definition
 */

class uiItem
{
  protected :
		  
	UI*		fGUI;
	float*		fZone;
	float		fCache;
	
	uiItem (UI* ui, float* zone) : fGUI(ui), fZone(zone), fCache(-123456.654321) 
	{ 
		ui->registerZone(zone, this); 
	}
	
	
  public :
	
	virtual ~uiItem() {}

	void modifyZone(float v) 	
	{ 
		fCache = v;
		if (*fZone != v) {
			*fZone = v;
			fGUI->updateZone(fZone);
		}
	}
		  	
	float			cache()			{ return fCache; }
	virtual void 	reflectZone() 	= 0;	
};


/**
 * Callback Item
 */

struct uiCallbackItem : public uiItem
{
	uiCallback	fCallback;
	void*		fData;
	
	uiCallbackItem(UI* ui, float* zone, uiCallback foo, void* data) 
			: uiItem(ui, zone), fCallback(foo), fData(data) {}
	
	virtual void 	reflectZone() {		
		float 	v = *fZone;
		fCache = v; 
		fCallback(v, fData);	
	}
};

/**
 * Update all user items reflecting zone z
 */

inline void UI::updateZone(float* z)
{
	float 	v = *z;
	clist* 	l = fZoneMap[z];
	for (clist::iterator c = l->begin(); c != l->end(); c++) {
		if ((*c)->cache() != v) (*c)->reflectZone();
	}
}


/**
 * Update all user items not up to date
 */

inline void UI::updateAllZones()
{
	for (zmap::iterator m = fZoneMap.begin(); m != fZoneMap.end(); m++) {
		float* 	z = m->first;
		clist*	l = m->second;
		float	v = *z;
		for (clist::iterator c = l->begin(); c != l->end(); c++) {
			if ((*c)->cache() != v) (*c)->reflectZone();
		}
	}
}

inline void UI::addCallback(float* zone, uiCallback foo, void* data) 
{ 
	new uiCallbackItem(this, zone, foo, data); 
};


/******************************************************************************
*******************************************************************************

                                GRAPHIC USER INTERFACE
                                  gtk interface

*******************************************************************************
*******************************************************************************/

#include <gtk/gtk.h>

#define stackSize 256

// Insertion modes

#define kSingleMode 0
#define kBoxMode 1
#define kTabMode 2



/**
 * rmWhiteSpaces(): Remove the leading and trailing white spaces of a string
 * (but not those in the middle of the string)
 */
static string rmWhiteSpaces(const string& s)
{
    size_t i = s.find_first_not_of(" \t");
    size_t j = s.find_last_not_of(" \t");

    if (i != string::npos & j != string::npos) {
        return s.substr(i, 1+j-i);
    } else {
        return "";
    }
}

/**
 * Extracts metdata from a label : 'vol [unit: dB]' -> 'vol' + metadata
 */
static void extractMetadata(const string& fulllabel, string& label, map<string, string>& metadata)
{
    enum {kLabel, kEscape1, kEscape2, kEscape3, kKey, kValue};
    int state = kLabel; int deep = 0;
    string key, value;

    for (unsigned int i=0; i < fulllabel.size(); i++) {
        char c = fulllabel[i];
        switch (state) {
            case kLabel :
                assert (deep == 0);
                switch (c) {
                    case '\\' : state = kEscape1; break;
                    case '[' : state = kKey; deep++; break;
                    default : label += c;
                }
                break;

            case kEscape1 :
                label += c;
                state = kLabel;
                break;

            case kEscape2 :
                key += c;
                state = kKey;
                break;

            case kEscape3 :
                value += c;
                state = kValue;
                break;

            case kKey :
                assert (deep > 0);
                switch (c) {
                    case '\\' :  state = kEscape2;
                                break;

                    case '[' :  deep++;
                                key += c;
                                break;

                    case ':' :  if (deep == 1) {
                                    state = kValue;
                                } else {
                                    key += c;
                                }
                                break;
                    case ']' :  deep--;
                                if (deep < 1) {
                                    metadata[rmWhiteSpaces(key)] = "";
                                    state = kLabel;
                                    key="";
                                    value="";
                                } else {
                                    key += c;
                                }
                                break;
                    default :   key += c;
                }
                break;

            case kValue :
                assert (deep > 0);
                switch (c) {
                    case '\\' : state = kEscape3;
                                break;

                    case '[' :  deep++;
                                value += c;
                                break;

                    case ']' :  deep--;
                                if (deep < 1) {
                                    metadata[rmWhiteSpaces(key)]=rmWhiteSpaces(value);
                                    state = kLabel;
                                    key="";
                                    value="";
                                } else {
                                    value += c;
                                }
                                break;
                    default :   value += c;
                }
                break;

            default :
                cerr << "ERROR unrecognized state " << state << endl;
        }
    }
    label = rmWhiteSpaces(label);
}


class GTKUI : public UI
{
 private :
    static bool                         fInitialized;
    static list<UI*>                    fGuiList;
    static map<float*, float>           fGuiSize;       // map widget zone with widget size coef
    static map<float*, string>          fTooltip;       // map widget zone with tooltip strings
    
 protected :
    GtkWidget*  fWindow;
    int         fTop;
    GtkWidget*  fBox[stackSize];
    int         fMode[stackSize];
    bool        fStopped;

    GtkWidget* addWidget(const char* label, GtkWidget* w);
    virtual void pushBox(int mode, GtkWidget* w);

        
 public :
    
    static const gboolean expand = TRUE;
    static const gboolean fill = TRUE;
    static const gboolean homogene = FALSE;
         
    GTKUI(char * name, int* pargc, char*** pargv);

    // -- Labels and metadata

    virtual void declare (float* zone, const char* key, const char* value);
    virtual int  checkLabelOptions (GtkWidget* widget, const string& fullLabel, string& simplifiedLabel);
    virtual void checkForTooltip (float* zone, GtkWidget* widget);
    
    // -- layout groups
    
    virtual void openFrameBox(const char* label);   
    virtual void openTabBox(const char* label = "");
    virtual void openHorizontalBox(const char* label = "");
    virtual void openVerticalBox(const char* label = "");

    // -- extra widget's layouts

    virtual void openDialogBox(const char* label, float* zone);
    virtual void openEventBox(const char* label = "");
    virtual void openHandleBox(const char* label = "");
    virtual void openExpanderBox(const char* label, float* zone);
    
    virtual void closeBox();
    virtual void adjustStack(int n);

    // -- active widgets
    
    virtual void addButton(const char* label, float* zone);
    virtual void addToggleButton(const char* label, float* zone);
    virtual void addCheckButton(const char* label, float* zone);
    virtual void addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step);   
    virtual void addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step); 
    virtual void addNumEntry(const char* label, float* zone, float init, float min, float max, float step);
    
    // -- passive display widgets
    
    virtual void addNumDisplay(const char* label, float* zone, int precision);
    virtual void addTextDisplay(const char* label, float* zone, const char* names[], float min, float max);
    virtual void addHorizontalBargraph(const char* label, float* zone, float min, float max);
    virtual void addVerticalBargraph(const char* label, float* zone, float min, float max);
    
    virtual void show();
    virtual void run();
    
};



/******************************************************************************
*******************************************************************************

                                GRAPHIC USER INTERFACE (v2)
                                  gtk implementation

*******************************************************************************
*******************************************************************************/

// global static fields

list<UI*>                   UI::fGuiList;

bool                        GTKUI::fInitialized = false;
map<float*, float>          GTKUI::fGuiSize;
map<float*, string>         GTKUI::fTooltip;



static gint delete_event( GtkWidget *widget, GdkEvent *event, gpointer data )
{
    return FALSE; 
}

static void destroy_event( GtkWidget *widget, gpointer data )
{
    gtk_main_quit ();
}

         
GTKUI::GTKUI(char * name, int* pargc, char*** pargv) 
{
    if (!fInitialized) {
        gtk_init(pargc, pargv);
        fInitialized = true;
    }
    
    fWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    //gtk_container_set_border_width (GTK_CONTAINER (fWindow), 10);
    gtk_window_set_title (GTK_WINDOW (fWindow), name);
    gtk_signal_connect (GTK_OBJECT (fWindow), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
    gtk_signal_connect (GTK_OBJECT (fWindow), "destroy", GTK_SIGNAL_FUNC (destroy_event), NULL);

    fTop = 0;
    fBox[fTop] = gtk_vbox_new (homogene, 4);
    fMode[fTop] = kBoxMode;
    gtk_container_add (GTK_CONTAINER (fWindow), fBox[fTop]);
    fStopped = false;
}

// empilement des boites

void GTKUI::pushBox(int mode, GtkWidget* w)
{
    ++fTop;
    assert(fTop < stackSize);
    fMode[fTop]     = mode;
    fBox[fTop]      = w;
}


/**
 * Remove n levels from the stack S before the top level
 * adjustStack(n): S -> S' with S' = S(0),S(n+1),S(n+2),...
 */
void GTKUI::adjustStack(int n)
{
    if (n > 0) {
        assert(fTop >= n);

        fTop -= n; 
        fMode[fTop] = fMode[fTop+n];
        fBox[fTop]  = fBox[fTop+n];
    }
}

void GTKUI::closeBox()
{
    --fTop;
    assert(fTop >= 0);
}


/**
 * Analyses the widget zone metadata declarations and takes
 * appropriate actions 
 */
void GTKUI::declare(float* zone, const char* key, const char* value)
{
    if (strcmp(key,"size")==0) {
        fGuiSize[zone]=atof(value);
    }
    else if (strcmp(key,"tooltip")==0) {
        fTooltip[zone] = formatTooltip(30,value) ;
    }
}
        
        

/**
 * Analyses a full label and activates the relevant options. returns a simplified
 * label (without options) and an amount of stack adjustement (in case additional
 * containers were pushed on the stack). 
 */

int GTKUI::checkLabelOptions(GtkWidget* widget, const string& fullLabel, string& simplifiedLabel)
{   
    map<string, string> metadata;
    extractMetadata(fullLabel, simplifiedLabel, metadata);

    if (metadata.count("tooltip")) {
        gtk_tooltips_set_tip (gtk_tooltips_new (), widget, metadata["tooltip"].c_str(), NULL);
    }
    if (metadata["option"] == "detachable") {
        openHandleBox(simplifiedLabel.c_str());
        return 1;
    }

    // no adjustement of the stack needed
    return 0;
}

/**
 * Check if a tooltip is associated to a zone and add it to the corresponding widget
 */
void GTKUI::checkForTooltip(float* zone, GtkWidget* widget)
{
    if (fTooltip.count(zone)) {
        gtk_tooltips_set_tip (gtk_tooltips_new (), widget, fTooltip[zone].c_str(), NULL);
    }
}


// les differentes boites

void GTKUI::openFrameBox(const char* label)
{
    GtkWidget * box = gtk_frame_new (label);
    //gtk_container_set_border_width (GTK_CONTAINER (box), 10);
            
    pushBox(kSingleMode, addWidget(label, box));
}


void GTKUI::openTabBox(const char* fullLabel)
{
    string  label;
    GtkWidget* widget = gtk_notebook_new();

    int     adjust = checkLabelOptions(widget, fullLabel, label);

    pushBox(kTabMode, addWidget(label.c_str(), widget));

    // adjust stack because otherwise Handlebox will remain open
    adjustStack(adjust);
}


void GTKUI::openHorizontalBox(const char* fullLabel)
{   
    string   label;
    GtkWidget* box = gtk_hbox_new (homogene, 4);
    int     adjust = checkLabelOptions(box, fullLabel, label);

    gtk_container_set_border_width (GTK_CONTAINER (box), 10);
            
    if (fMode[fTop] != kTabMode && label[0] != 0) {
        GtkWidget * frame = addWidget(label.c_str(), gtk_frame_new (label.c_str()));
        gtk_container_add (GTK_CONTAINER(frame), box);
        gtk_widget_show(box);
        pushBox(kBoxMode, box);
    } else {
        pushBox(kBoxMode, addWidget(label.c_str(), box));
    }

    // adjust stack because otherwise Handlebox will remain open
    adjustStack(adjust);
}


void GTKUI::openVerticalBox(const char* fullLabel)
{
    string  label;
    GtkWidget * box = gtk_vbox_new (homogene, 4);
    int      adjust = checkLabelOptions(box, fullLabel, label);

    gtk_container_set_border_width (GTK_CONTAINER (box), 10);
            
    if (fMode[fTop] != kTabMode && label[0] != 0) {
        GtkWidget * frame = addWidget(label.c_str(), gtk_frame_new (label.c_str()));
        gtk_container_add (GTK_CONTAINER(frame), box);
        gtk_widget_show(box);
        pushBox(kBoxMode, box);
    } else {
        pushBox(kBoxMode, addWidget(label.c_str(), box));
    }

    // adjust stack because otherwise Handlebox will remain open
    adjustStack(adjust);
}


void GTKUI::openHandleBox(const char* label)
{
    GtkWidget * box = gtk_hbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    if (fMode[fTop] != kTabMode && label[0] != 0)
    {
        GtkWidget * frame = addWidget(label, gtk_handle_box_new ());
        gtk_container_add (GTK_CONTAINER(frame), box);
        gtk_widget_show(box);
        pushBox(kBoxMode, box);
    }
    else
    {
        pushBox(kBoxMode, addWidget(label, box));
    }
}


void GTKUI::openEventBox(const char* label)
{
    GtkWidget * box = gtk_hbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    if (fMode[fTop] != kTabMode && label[0] != 0)
    {
        GtkWidget * frame = addWidget(label, gtk_event_box_new ());
        gtk_container_add (GTK_CONTAINER(frame), box);
        gtk_widget_show(box);
        pushBox(kBoxMode, box);
    }
    else
    {
        pushBox(kBoxMode, addWidget(label, box));
    }
}


struct uiExpanderBox : public uiItem
{
    GtkExpander* fButton;
    uiExpanderBox(UI* ui, float* zone, GtkExpander* b) : uiItem(ui, zone), fButton(b) {}
    static void expanded (GtkWidget *widget, gpointer data)
    {
        float   v = gtk_expander_get_expanded  (GTK_EXPANDER(widget));
        if (v == 1.000000)
        {
            v = 0;
        }
        else v = 1;
        ((uiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()
    {
        float   v = *fZone;
        fCache = v;
        gtk_expander_set_expanded(GTK_EXPANDER(fButton), v);
    }
};

void GTKUI::openExpanderBox(const char* label, float* zone)
{
    *zone = 0.0;
    GtkWidget * box = gtk_hbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    if (fMode[fTop] != kTabMode && label[0] != 0)
    {
        GtkWidget * frame = addWidget(label, gtk_expander_new (label));
        gtk_container_add (GTK_CONTAINER(frame), box);
        uiExpanderBox* c = new uiExpanderBox(this, zone, GTK_EXPANDER(frame));
        gtk_signal_connect (GTK_OBJECT (frame), "activate", GTK_SIGNAL_FUNC (uiExpanderBox::expanded), (gpointer)c);
        gtk_widget_show(box);
        pushBox(kBoxMode, box);
    }
    else
    {
        pushBox(kBoxMode, addWidget(label, box));
    }
}



GtkWidget* GTKUI::addWidget(const char* label, GtkWidget* w)
{ 
    switch (fMode[fTop]) {
        case kSingleMode    : gtk_container_add (GTK_CONTAINER(fBox[fTop]), w);                             break;
        case kBoxMode       : gtk_box_pack_start (GTK_BOX(fBox[fTop]), w, expand, fill, 0);                 break;
        case kTabMode       : gtk_notebook_append_page (GTK_NOTEBOOK(fBox[fTop]), w, gtk_label_new(label)); break;
    }
    gtk_widget_show (w);
    return w;
}

// --------------------------- Press button ---------------------------

struct uiButton : public uiItem
{
    GtkButton*  fButton;
    
    uiButton (UI* ui, float* zone, GtkButton* b) : uiItem(ui, zone), fButton(b) {}
    
    static void pressed( GtkWidget *widget, gpointer   data )
    {
        uiItem* c = (uiItem*) data;
        c->modifyZone(1.0);
    }

    static void released( GtkWidget *widget, gpointer   data )
    {
        uiItem* c = (uiItem*) data;
        c->modifyZone(0.0);
    }

    virtual void reflectZone()  
    { 
        float   v = *fZone;
        fCache = v; 
        if (v > 0.0) gtk_button_pressed(fButton); else gtk_button_released(fButton);
    }
};

void GTKUI::addButton(const char* label, float* zone)
{
    *zone = 0.0;
    GtkWidget*  button = gtk_button_new_with_label (label);
    addWidget(label, button);
    
    uiButton* c = new uiButton(this, zone, GTK_BUTTON(button));
    
    gtk_signal_connect (GTK_OBJECT (button), "pressed", GTK_SIGNAL_FUNC (uiButton::pressed), (gpointer) c);
    gtk_signal_connect (GTK_OBJECT (button), "released", GTK_SIGNAL_FUNC (uiButton::released), (gpointer) c);

    checkForTooltip(zone, button);
}

// ---------------------------  Toggle Buttons ---------------------------

struct uiToggleButton : public uiItem
{
    GtkToggleButton* fButton;
    
    uiToggleButton(UI* ui, float* zone, GtkToggleButton* b) : uiItem(ui, zone), fButton(b) {}
    
    static void toggled (GtkWidget *widget, gpointer data)
    {
        float   v = (GTK_TOGGLE_BUTTON (widget)->active) ? 1.0 : 0.0; 
        ((uiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()  
    { 
        float   v = *fZone;
        fCache = v; 
        gtk_toggle_button_set_active(fButton, v > 0.0); 
    }
};

void GTKUI::addToggleButton(const char* label, float* zone)
{
    *zone = 0.0;
    GtkWidget*  button = gtk_toggle_button_new_with_label (label);
    addWidget(label, button);
    
    uiToggleButton* c = new uiToggleButton(this, zone, GTK_TOGGLE_BUTTON(button));
    gtk_signal_connect (GTK_OBJECT (button), "toggled", GTK_SIGNAL_FUNC (uiToggleButton::toggled), (gpointer) c);

    checkForTooltip(zone, button);
}



void show_dialog(GtkWidget *widget, gpointer data)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)) == TRUE)
    {
        gtk_widget_show(GTK_WIDGET(data));
        gint root_x, root_y;
        gtk_window_get_position (GTK_WINDOW(data), &root_x, &root_y);
        root_y -= 120;
        gtk_window_move(GTK_WINDOW(data), root_x, root_y);
    }
    else gtk_widget_hide(GTK_WIDGET(data));
}

static gboolean deleteevent( GtkWidget *widget, gpointer   data )
{
return TRUE;
} 

void GTKUI::openDialogBox(const char* label, float* zone)
{
    // create toplevel window and set properties
    GtkWidget * dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_deletable(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_gravity(GTK_WINDOW(dialog), GDK_GRAVITY_SOUTH);
    gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(fWindow));
    gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_keep_below (GTK_WINDOW(dialog), FALSE);
    gtk_window_set_title (GTK_WINDOW (dialog), label);
    g_signal_connect (G_OBJECT (dialog), "delete_event", G_CALLBACK (deleteevent), NULL); 
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    GtkWidget * box = gtk_hbox_new (homogene, 4);
 
    *zone = 0.0;
    GtkWidget*  button = gtk_toggle_button_new ();
    gtk_signal_connect (GTK_OBJECT (button), "toggled", GTK_SIGNAL_FUNC (show_dialog), (gpointer) dialog);
 
    gtk_container_add (GTK_CONTAINER(fBox[fTop]), button);
    gtk_container_add (GTK_CONTAINER(dialog), box);
    gtk_widget_show (button);
    gtk_widget_show(box);
    pushBox(kBoxMode, box);
}




// ---------------------------  Check Button ---------------------------

struct uiCheckButton : public uiItem
{
    GtkToggleButton* fButton;
    
    uiCheckButton(UI* ui, float* zone, GtkToggleButton* b) : uiItem(ui, zone), fButton(b) {}
    
    static void toggled (GtkWidget *widget, gpointer data)
    {
        float   v = (GTK_TOGGLE_BUTTON (widget)->active) ? 1.0 : 0.0; 
        ((uiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()  
    { 
        float   v = *fZone;
        fCache = v; 
        gtk_toggle_button_set_active(fButton, v > 0.0); 
    }
};

void GTKUI::addCheckButton(const char* label, float* zone)
{
    *zone = 0.0;
    GtkWidget*  button = gtk_check_button_new_with_label (label);
    addWidget(label, button);
    
    uiCheckButton* c = new uiCheckButton(this, zone, GTK_TOGGLE_BUTTON(button));
    gtk_signal_connect (GTK_OBJECT (button), "toggled", GTK_SIGNAL_FUNC(uiCheckButton::toggled), (gpointer) c);

    checkForTooltip(zone, button);
}


// ---------------------------  Adjustmenty based widgets ---------------------------

struct uiAdjustment : public uiItem
{
    GtkAdjustment* fAdj;
    
    uiAdjustment(UI* ui, float* zone, GtkAdjustment* adj) : uiItem(ui, zone), fAdj(adj) {}
    
    static void changed (GtkWidget *widget, gpointer data)
    {
        float   v = GTK_ADJUSTMENT (widget)->value; 
        ((uiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()  
    { 
        float   v = *fZone;
        fCache = v; 
        gtk_adjustment_set_value(fAdj, v);  
    }
};

static int precision(double n)
{
    if (n < 0.009999) return 3;
    else if (n < 0.099999) return 2;
    else if (n < 0.999999) return 1;
    else return 0;
}

// -------------------------- Vertical Slider -----------------------------------

void GTKUI::addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));

    gtk_signal_connect (GTK_OBJECT (adj), "value-changed", GTK_SIGNAL_FUNC (uiAdjustment::changed), (gpointer) c);
    
    GtkWidget* slider = gtk_vscale_new (GTK_ADJUSTMENT(adj));
    gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
    gtk_scale_set_digits(GTK_SCALE(slider), precision(step));
    float size = 160 * pow(2, fGuiSize[zone]);
    gtk_widget_set_usize(slider, -1, size);

    if (label && label[0]!=0) {
        openFrameBox(label);
        addWidget(label, slider);
        closeBox();
    } else {
        addWidget(label, slider);
    }

    checkForTooltip(zone, slider);
}

// -------------------------- Horizontal Slider -----------------------------------

void GTKUI::addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));

    gtk_signal_connect (GTK_OBJECT (adj), "value-changed", GTK_SIGNAL_FUNC (uiAdjustment::changed), (gpointer) c);
    
    GtkWidget* slider = gtk_hscale_new (GTK_ADJUSTMENT(adj));
    gtk_scale_set_digits(GTK_SCALE(slider), precision(step));
    float size = 160 * pow(2, fGuiSize[zone]);
    gtk_widget_set_usize(slider, size, -1);
    
    if (label && label[0]!=0) {
        openFrameBox(label);
        addWidget(label, slider);
        closeBox();
    } else {
        addWidget(label, slider);
    }             

    checkForTooltip(zone, slider);
}


// ------------------------------ Num Entry -----------------------------------

void GTKUI::addNumEntry(const char* label, float* zone, float init, float min, float max, float step)
{
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, step);
    
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));

    gtk_signal_connect (GTK_OBJECT (adj), "value-changed", GTK_SIGNAL_FUNC (uiAdjustment::changed), (gpointer) c);
    
    GtkWidget* spinner = gtk_spin_button_new (GTK_ADJUSTMENT(adj), 0.005, precision(step));

    //gtk_widget_set_usize(slider, 160, -1);
    openFrameBox(label);
    addWidget(label, spinner);
    closeBox();

    checkForTooltip(zone, spinner);
}


// ==========================   passive widgets ===============================


// ------------------------------ Progress Bar -----------------------------------

struct uiBargraph : public uiItem
{
    GtkProgressBar*     fProgressBar;
    float               fMin;
    float               fMax;
    
    uiBargraph(UI* ui, float* zone, GtkProgressBar* pbar, float lo, float hi) 
            : uiItem(ui, zone), fProgressBar(pbar), fMin(lo), fMax(hi) {}

    float scale(float v)        { return (v-fMin)/(fMax-fMin); }
    
    virtual void reflectZone()  
    { 
        float   v = *fZone;
        fCache = v; 
        gtk_progress_bar_set_fraction(fProgressBar, scale(v));  
    }
};

    

void GTKUI::addVerticalBargraph(const char* label, float* zone, float lo, float hi)
{
    GtkWidget* pb = gtk_progress_bar_new();
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pb), GTK_PROGRESS_BOTTOM_TO_TOP);
    gtk_widget_set_size_request(pb, 8, -1);
    new uiBargraph(this, zone, GTK_PROGRESS_BAR(pb), lo, hi);
    openFrameBox(label);
    addWidget(label, pb);
    closeBox();

    checkForTooltip(zone, pb);
}
    

void GTKUI::addHorizontalBargraph(const char* label, float* zone, float lo, float hi)
{
    GtkWidget* pb = gtk_progress_bar_new();
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pb), GTK_PROGRESS_LEFT_TO_RIGHT);
    gtk_widget_set_size_request(pb, -1, 8);
    new uiBargraph(this, zone, GTK_PROGRESS_BAR(pb), lo, hi);
    openFrameBox(label);
    addWidget(label, pb);
    closeBox();

    checkForTooltip(zone, pb);
}


// ------------------------------ Num Display -----------------------------------

struct uiNumDisplay : public uiItem
{
    GtkLabel* fLabel;
    int fPrecision;
    
    uiNumDisplay(UI* ui, float* zone, GtkLabel* label, int precision) 
            : uiItem(ui, zone), fLabel(label), fPrecision(precision) {}

    virtual void reflectZone()  
    { 
        float   v = *fZone;
        fCache = v;
        char s[64]; 
        if (fPrecision <= 0) { 
            snprintf(s, 63, "%d", int(v)); 
        } else if (fPrecision>3) {
            snprintf(s, 63, "%f", v);
        } else {
            const char* format[] = {"%.1f", "%.2f", "%.3f"};
            snprintf(s, 63, format[fPrecision-1], v);
        }
        gtk_label_set_text(fLabel, s);
    }
};
    

void GTKUI::addNumDisplay(const char* label, float* zone, int precision )
{
    GtkWidget* lw = gtk_label_new("");
    new uiNumDisplay(this, zone, GTK_LABEL(lw), precision);
    openFrameBox(label);
    addWidget(label, lw);
    closeBox();

    checkForTooltip(zone, lw);
}


// ------------------------------ Text Display -----------------------------------

struct uiTextDisplay : public uiItem
{
        GtkLabel*           fLabel;
        const char**    fNames;
        float               fMin;
        float               fMax;
        int                         fNum;
        
        
        uiTextDisplay (UI* ui, float* zone, GtkLabel* label, const char* names[], float lo, float hi)
                        : uiItem(ui, zone), fLabel(label), fNames(names), fMin(lo), fMax(hi)
        {
                fNum = 0;
                while (fNames[fNum] != 0) fNum++;
    }

    virtual void reflectZone()  
    { 
        float   v = *fZone;
        fCache = v;
        
        int idx = int(fNum*(v-fMin)/(fMax-fMin));
        
        if      (idx < 0)       idx = 0; 
        else if (idx >= fNum)   idx = fNum-1;
                
        gtk_label_set_text(fLabel, fNames[idx]); 
    }
};
    

void GTKUI::addTextDisplay(const char* label, float* zone, const char* names[], float lo, float hi )
{
    GtkWidget* lw = gtk_label_new("");
    new uiTextDisplay (this, zone, GTK_LABEL(lw), names, lo, hi);
    openFrameBox(label);
    addWidget(label, lw);
    closeBox();

    checkForTooltip(zone, lw);
}



void GTKUI::show() 
{
    assert(fTop == 0);
    gtk_widget_show  (fBox[0]);
    gtk_widget_show  (fWindow);
}


/**
 * Update all user items reflecting zone z
 */
    
static gboolean callUpdateAllGuis(gpointer)
{ 
    UI::updateAllGuis(); 
    return TRUE;
}


void GTKUI::run() 
{
    assert(fTop == 0);
    gtk_widget_show  (fBox[0]);
    gtk_widget_show  (fWindow);
    gtk_timeout_add(40, callUpdateAllGuis, 0);
    gtk_main ();
    stop();
}



/******************************************************************************
*******************************************************************************

								DSP

*******************************************************************************
*******************************************************************************/


//----------------------------------------------------------------
//  Definition of a Faust Digital Signal Processor
//----------------------------------------------------------------
			
class dsp {
 protected:
	int fSamplingFreq;
        int fThreadNum;
 public:
	dsp() {}
	virtual ~dsp() {}
	
	virtual int getNumInputs() 										= 0;
	virtual int getNumOutputs() 									= 0;
    virtual void buildUserInterface(UI* interface) 					= 0;
    virtual void init(int samplingRate) 							= 0;
 	virtual void compute(int len, float** inputs, float** outputs) 	= 0;
 	virtual void conclude() 										{}
};
		
/********************END ARCHITECTURE SECTION (part 1/2)****************/

/**************************BEGIN USER SECTION **************************/
		
<<includeclass>>

/***************************END USER SECTION ***************************/

/*******************BEGIN ARCHITECTURE SECTION (part 2/2)***************/
					
mydsp	DSP;




/******************************************************************************
*******************************************************************************

								MAIN PLAY THREAD

*******************************************************************************
*******************************************************************************/
	
// lopt : Scan Command Line long int Arguments

long lopt (int argc, char *argv[], const char* longname, const char* shortname, long def) 
{
	for (int i=2; i<argc; i++) 
		if ( strcmp(argv[i-1], shortname) == 0 || strcmp(argv[i-1], longname) == 0 ) 
			return atoi(argv[i]);
	return def;
}
	
// sopt : Scan Command Line string Arguments

const char* sopt (int argc, char *argv[], const char* longname, const char* shortname, const char* def) 
{
	for (int i=2; i<argc; i++) 
		if ( strcmp(argv[i-1], shortname) == 0 || strcmp(argv[i-1], longname) == 0 ) 
			return argv[i];
	return def;
}
	
// fopt : Scan Command Line flag option (without argument), return true if the flag

bool fopt (int argc, char *argv[], const char* longname, const char* shortname) 
{
	for (int i=1; i<argc; i++) 
		if ( strcmp(argv[i], shortname) == 0 || strcmp(argv[i], longname) == 0 ) 
			return true;
	return false;
}
	

//-------------------------------------------------------------------------
// 									MAIN
//-------------------------------------------------------------------------

pthread_t	guithread;
	
void* run_ui(void* ptr)
{
	UI* interface = (UI*) ptr;
	interface->run();
	pthread_exit(0);
	return 0;
}



/**
 * Return the value of an environment variable or defval if undefined.
 */
static int getDefaultEnv(const char* name, int defval)
{
    const char* str = getenv(name);
    if (str) {
        return atoi(str);
    } else {
        return defval;
    }
}

/**
 * Return the value of an environment variable or defval if undefined.
 */
static const char* getDefaultEnv(const char* name, const char* defval)
{
    const char* str = getenv(name);
    if (str) {
        return str;
    } else {
        return defval;
    }
}

int main(int argc, char *argv[] )
{
	CHECKINTSIZE;

	UI* 	interface = new GTKUI(argv[0], &argc, &argv);
	
	// compute rcfilename to (re)store application state
	char	rcfilename[256];
	char* 	home = getenv("HOME");
	snprintf(rcfilename, 255, "%s/.%src", home, basename(argv[0]));
	
	AudioInterface	audio (
		AudioParam().cardName( sopt(argc, argv, "--device", "-d",     getDefaultEnv("FAUST2ALSA_DEVICE", "hw:0")  ) )
					.frequency( lopt(argc, argv, "--frequency", "-f", getDefaultEnv("FAUST2ALSA_FREQUENCY",44100) ) ) 
					.buffering( lopt(argc, argv, "--buffer", "-b",    getDefaultEnv("FAUST2ALSA_BUFFER",1024)     ) )
					.periods( lopt(argc, argv, "--periods", "-p",     getDefaultEnv("FAUST2ALSA_PERIODS",2)       ) )
					.inputs(DSP.getNumInputs())
					.outputs(DSP.getNumOutputs())
	);

	AVOIDDENORMALS;
	audio.open();
	
    DSP.init(audio.frequency());
    DSP.buildUserInterface(interface);
	
	interface->recallState(rcfilename);

	pthread_create(&guithread, NULL, run_ui, interface);
	
	bool rt = setRealtimePriority();
	printf(rt?"RT : ":"NRT: "); audio.shortinfo();
	if (fopt(argc, argv, "--verbose", "-v")) audio.longinfo();
	bool running = true;
	audio.write();
	audio.write();
	while(running) {
		audio.read();
    STARTMESURE
		DSP.compute(audio.buffering(), audio.inputSoftChannels(), audio.outputSoftChannels());
    STOPMESURE  
		audio.write();
		running = !interface->stopped();
	}
	interface->saveState(rcfilename);

#ifdef BENCHMARKMODE
    printstats();
#endif       

  	return 0;
}
/********************END ARCHITECTURE SECTION (part 2/2)****************/

