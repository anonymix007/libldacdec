#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <errno.h>

#include "ldacdec.h"

#include "sndfile.h"

SNDFILE *openAudioFile( const char *fileName, int freq, int channels )
{
    SF_INFO sfinfo = { 
        .samplerate = freq, 
        .channels = channels,
        .format = SF_FORMAT_WAV | SF_FORMAT_PCM_24,
    };
    printf("opening \"%s\" for writing ...\n", fileName );
    printf("sampling frequency: %d\n", freq );
    printf("channel count:      %d\n", channels );
    SNDFILE *out = sf_open( fileName, SFM_WRITE, &sfinfo );
    if( out == NULL )
    {
        printf("can't open output: %s\n", sf_strerror( NULL ) );
        return NULL;
    }
    return out;
}


#define BUFFER_SIZE (680*2*2)
#define PCM_BUFFER_SIZE (256*2*2)

int main(int argc, char *args[] )
{
    if( argc < 2 )
    {
        printf("usage:\n\t%s <input> <output, optional>\n", args[0] );
        return EXIT_SUCCESS;
    }
    const char *inputFile = args[1];
    const char *audioFile = "output.wav";

    if( argc > 2 )
        audioFile = args[2];

    printf("opening \"%s\" ...\n", inputFile );

    ldacdec_t dec;
    ldacdecInit( &dec );

    FILE *in = fopen( inputFile, "rb" );
    if( in == NULL )
    {
        perror("can't open stream file");
        return EXIT_FAILURE;
    }

    SNDFILE *out = NULL;
    printf("auto detect format!\n");
    out = openAudioFile(audioFile, 96000, 2); 
    if(out == NULL) return EXIT_FAILURE;
    uint8_t buf[BUFFER_SIZE];
    uint8_t *ptr = NULL;
    int32_t pcm[PCM_BUFFER_SIZE] = { 0 };
    size_t filePosition = 0;
    //int blockId = 0;
    int bytesInBuffer = 0;
    unsigned long long i = 0;

    while(1)
    {
        LOG("%ld =>\n", filePosition );
        int ret = fseek( in, filePosition, SEEK_SET );
        if( ret < 0 )
            break;
        bytesInBuffer = fread( buf, 1, BUFFER_SIZE, in );
        
        ptr = buf;
        if( bytesInBuffer == 0 ){
            break;
        }
#if 1
        while(ptr[0] != 0xAA) 
        {
            ptr++;
            filePosition++;
        }
#endif        
        //printf("sync: %02x,\tpos %4lx,\t", ptr[0], filePosition);
        int bytesUsed = 0;
      
        memset( pcm, 0, sizeof(pcm) );
        LOG("count === %4d ===\n", blockId++ );
        ret = ldacDecode_type( &dec, ptr, pcm, &bytesUsed, 4);
        //printf("ret %d,\tused %d\n", ret, bytesUsed); 

        LOG_ARRAY( pcm, "%4d, " );

        sf_writef_int( out, pcm, dec.frame.frameSamples );
        filePosition += bytesUsed;
        i++;
        if( ret < 0 ){
            break;
        }        
    }

    printf("%llu iterations\nDone!\n", i);

    sf_close(out);
    fclose(in);

    return EXIT_SUCCESS;
}
