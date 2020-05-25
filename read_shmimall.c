/**
 * @file    read_shmimall.c
 * @brief   read all shared memory stream
 */

#include <sys/stat.h>
#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/mman.h> // mmap

#include "CommandLineInterface/CLIcore.h"
#include "image_ID.h"
#include "list_image.h"
#include "read_shmim.h"




// ==========================================
// forward declaration
// ==========================================

errno_t read_sharedmem_image_all(
    const char *name
);


// ==========================================
// command line interface wrapper functions
// ==========================================


static errno_t read_sharedmem_image_all__cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_STR)
            == 0)
    {

        read_sharedmem_image_all(
            data.cmdargtoken[1].val.string);

        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}








// ==========================================
// Register CLI command(s)
// ==========================================

errno_t read_shmimall_addCLIcmd()
{

    RegisterCLIcommand(
        "readshmimall",
        __FILE__, 
        read_sharedmem_image_all__cli,
        "read all shared memory images",
        "<string filter>",
        "readshmimall aol_",
        "read_sharedmem_image_all(const char *name)");    

    return RETURN_SUCCESS;
}








errno_t read_sharedmem_image_all(
    const char *strfilter
)
{
    //printf("LOADING ALL STREAMS matching %s\n", strfilter);

    int NBstreamMAX = 10000;
    STREAMINFO *streaminfo;

    streaminfo = (STREAMINFO *) malloc(sizeof(STREAMINFO) * NBstreamMAX);

    int NBstream = find_streams(streaminfo, 1, strfilter);

    //printf("%d streams found :\n", NBstream);
    for(int sindex = 0; sindex < NBstream; sindex++)
    {
        //printf(" %3d   %s\n", sindex, streaminfo[sindex].sname);
        imageID ID = image_ID(streaminfo[sindex].sname);
        if(ID == -1)
        {
            read_sharedmem_image(streaminfo[sindex].sname);
        }
    }

    free(streaminfo);

    return RETURN_SUCCESS;
}


