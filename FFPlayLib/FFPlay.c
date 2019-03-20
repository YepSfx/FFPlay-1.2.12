#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <signal.h>

#include "FFPlayLib.h"

void ExitEvent(void *sender, int exitCode)
{
   printf("\n\n Exit Event!!!\n\n");
   exit(exitCode);
}  

void sigterm_handler(int sig)
{
   ExitEvent(NULL, 123);
}

static char FileName[256] = {0,};
static unsigned char RGBBuff[1024*768*4] = {0,};

void SaveFramebufferAsPPM(void* Buff, int w, int h, int Bpp) 
{
	static  time_t t=0, t1;
	FILE*   f;
	int     i, j;
	int     row_stride = w*Bpp;
	int     redShift = 0;
	int     blueShift = 16;
	int     greenShift = 24;
  static  int idx;
	/* save one picture only if the last is older than 2 seconds */
	t1=time(NULL);
	if( t1-t > 2)
		t = t1;
	else
		return;

  sprintf(FileName,"framebuffer_%d.ppm", idx++);
	f=fopen(FileName,"wb");
	if(!f) 
	{
		ExitEvent(NULL, 4);
	}

	for(j = 0 ; j < h*row_stride ; j += row_stride)
	{
		for( i = 0 ; i < w*Bpp ; i += Bpp) 
		{
			unsigned char* p = Buff + j + i;
			unsigned int v;

			if(Bpp==4)
				v=*(unsigned int*)p;

			fputc((v>>redShift)*256/(256),f);
			fputc((v>>greenShift)*256/(256),f);
			fputc((v>>blueShift)*256/(256),f);
		}
	}
	fclose(f);
}
/* Called from the main */

void PlayProgress(void *sender)
{
    FFP_VID_PARAMS  *vidParams = multimedia_get_videoformat();
    FFP_AUD_PARAMS  *audParams = multimedia_get_audioformat();
}

void CallbackVideo(void *sender, FFP_YUV420P_DATA *YuvData)
{
     //multimedia_yuv420p_to_rgb32( YuvData, RGBBuff );
     //SaveFramebufferAsPPM(RGBBuff, YuvData->w, YuvData->h, 4 ); 

} 

void CallbackAudio(void *sender, unsigned char *buffer, int BufLenInByte)
{
    FFP_AUD_PARAMS *params = multimedia_get_audioformat();
    int i, idx;
    short sampleL, sampleR, *sampleBuf;
    sampleBuf = (short*)buffer;
   
    for (i = 0 ; i < params->SamplesInBuffer ; i++)
    {
        sampleL = sampleBuf[(i<<1)];
        sampleR = sampleBuf[(i<<1)+1];
        
        sampleBuf[(i<<1)]   = sampleL;
        sampleBuf[(i<<1)+1] = sampleR;
    }
}

void CallbackResize(void *sender, int w , int h)
{
 
}

void MessageInfo(void *sender, int infoCode, char *Message)
{
    printf( Message );
}

int main(int argc, char **argv)
{
    int theID = 123;
    FFP_EVENTS FFP_events;
    
    printf("\n");
    
    memset( (void*)&FFP_events, 0, sizeof(FFP_EVENTS));
    
    FFP_events.sender        = &theID; 
    FFP_events.ui_type       = FFP_CLI;
    FFP_events.event_exit    = ExitEvent; 
    FFP_events.event_info    = MessageInfo;
    FFP_events.event_audio   = CallbackAudio;
    FFP_events.event_video   = CallbackVideo;
    FFP_events.event_video_resize = CallbackResize;
    FFP_events.playstatus    = FFP_STOP;
    
    signal(SIGINT , sigterm_handler); /* Interrupt (ANSI).    */
    signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */

    multimedia_parse_options(argc, argv);

    if (!multimedia_get_filename()) 
    {
        fprintf(stderr, "\n");
        fprintf(stderr, "An input file must be specified\n");
        fprintf(stderr, "Use -h to get full help or, even better, run 'man %s'\n", "FFPlay");
        ExitEvent(NULL, 1);
    }

    if ( multimedia_init_device( &FFP_events ) != 0)
    {
        fprintf(stderr, "Failed to initialize!\n");
    }

    if ( !multimedia_stream_open() )
    {
        fprintf(stderr, "Failed to initialize VideoState!\n");
        multimedia_exit();
    }
    
    multimedia_stream_start();

    /* never returns */
    return 0;
}
