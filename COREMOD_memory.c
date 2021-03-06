/**
 * @file    COREMOD_memory.c
 * @brief   milk memory functions
 *
 * Functions to handle images and streams
 *
 */



#define _GNU_SOURCE



/* ================================================================== */
/* ================================================================== */
/*            MODULE INFO                                             */
/* ================================================================== */
/* ================================================================== */

// module default short name
// all CLI calls to this module functions will be <shortname>.<funcname>
// if set to "", then calls use <funcname>
#define MODULE_SHORTNAME_DEFAULT ""

// Module short description
#define MODULE_DESCRIPTION       "Memory management for images and variables"




/* =============================================================================================== */
/* =============================================================================================== */
/*                                        HEADER FILES                                             */
/* =============================================================================================== */
/* =============================================================================================== */


#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <signal.h>
#include <ncurses.h>

#include <errno.h>
#include <signal.h>

#include <semaphore.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#include <time.h>
#include <sys/time.h>




#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
static int clock_gettime(int clk_id, struct mach_timespec *t)
{
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer) / ((
                          double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer) / ((
                         double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return RETURN_SUCCESS;
}
#else
#include <time.h>
#endif


#include <fitsio.h>



#include "CommandLineInterface/CLIcore.h"

#include "COREMOD_tools/COREMOD_tools.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_iofits/COREMOD_iofits.h"






/* =============================================================================================== */
/* =============================================================================================== */
/*                                      DEFINES, MACROS                                            */
/* =============================================================================================== */
/* =============================================================================================== */




# ifdef _OPENMP
# include <omp.h>
#define OMP_NELEMENT_LIMIT 1000000
# endif

#define STYPESIZE 10
#define SBUFFERSIZE 1000







#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)





static pthread_t *thrarray_semwait;
static long NB_thrarray_semwait;


// MEMORY MONITOR
static FILE *listim_scr_fpo;
static FILE *listim_scr_fpi;
static SCREEN *listim_scr; // for memory monitoring
static int MEM_MONITOR = 0; // 1 if memory monitor is on
static int listim_scr_wrow;
static int listim_scr_wcol;


//extern struct DATA data;

//static int INITSTATUS_COREMOD_memory = 0;




/** data logging of shared memory image stream
 *
 */



//static STREAMSAVE_THREAD_MESSAGE savethreadmsg;

static long tret; // thread return value




/* ================================================================== */
/* ================================================================== */
/*            INITIALIZE LIBRARY                                      */
/* ================================================================== */
/* ================================================================== */

// Module initialization macro in CLIcore.h
// macro argument defines module name for bindings
//
INIT_MODULE_LIB(COREMOD_memory)




/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 1. MANAGE MEMORY AND IDENTIFIERS                                                                */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */


errno_t delete_image_ID_cli()
{
    long i = 1;
    printf("%ld : %d\n", i, data.cmdargtoken[i].type);
    while(data.cmdargtoken[i].type != 0)
    {
        if(data.cmdargtoken[i].type == 4)
        {
            delete_image_ID(data.cmdargtoken[i].val.string);
        }
        else
        {
            printf("Image %s does not exist\n", data.cmdargtoken[i].val.string);
        }
        i++;
    }

    return CLICMD_SUCCESS;
}





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 2. KEYWORDS                                                                                     */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */


errno_t image_write_keyword_L_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, CLIARG_STR_NOT_IMG)
            == 0)
    {
        image_write_keyword_L(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.string
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}




errno_t image_list_keywords_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            == 0)
    {
        image_list_keywords(data.cmdargtoken[1].val.string);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}






/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 3. READ SHARED MEM IMAGE AND SIZE                                                               */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



errno_t read_sharedmem_image_size_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_STR)
            + CLI_checkarg(2, CLIARG_STR_NOT_IMG)
            == 0)
    {

        read_sharedmem_image_size(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string);

        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}




errno_t read_sharedmem_image_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            == 0)
    {

        read_sharedmem_image(
            data.cmdargtoken[1].val.string);

        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 4. CREATE IMAGE                                                                                 */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



errno_t create_image_cli()
{
    uint32_t *imsize;
    long naxis = 0;
    long i;
    uint8_t datatype;



    if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            + CLI_checkarg_noerrmsg(2, CLIARG_LONG)
            == 0)
    {
        naxis = 0;
        imsize = (uint32_t *) malloc(sizeof(uint32_t) * 5);
        i = 2;
        while(data.cmdargtoken[i].type == 2)
        {
            imsize[naxis] = data.cmdargtoken[i].val.numl;
            naxis++;
            i++;
        }
        switch(data.precision)
        {
            case 0:
                create_image_ID(data.cmdargtoken[1].val.string, naxis, imsize, _DATATYPE_FLOAT,
                                data.SHARED_DFT, data.NBKEWORD_DFT);
                break;
            case 1:
                create_image_ID(data.cmdargtoken[1].val.string, naxis, imsize, _DATATYPE_DOUBLE,
                                data.SHARED_DFT, data.NBKEWORD_DFT);
                break;
        }
        free(imsize);
        return CLICMD_SUCCESS;
    }
    else if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(2, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(3, CLIARG_LONG)
            == 0)  // type option exists
    {
        datatype = 0;

        if(strcmp(data.cmdargtoken[2].val.string, "c") == 0)
        {
            printf("type = CHAR\n");
            datatype = _DATATYPE_UINT8;
        }

        if(strcmp(data.cmdargtoken[2].val.string, "i") == 0)
        {
            printf("type = INT\n");
            datatype = _DATATYPE_INT32;
        }

        if(strcmp(data.cmdargtoken[2].val.string, "f") == 0)
        {
            printf("type = FLOAT\n");
            datatype = _DATATYPE_FLOAT;
        }

        if(strcmp(data.cmdargtoken[2].val.string, "d") == 0)
        {
            printf("type = DOUBLE\n");
            datatype = _DATATYPE_DOUBLE;
        }

        if(strcmp(data.cmdargtoken[2].val.string, "cf") == 0)
        {
            printf("type = COMPLEX_FLOAT\n");
            datatype = _DATATYPE_COMPLEX_FLOAT;
        }

        if(strcmp(data.cmdargtoken[2].val.string, "cd") == 0)
        {
            printf("type = COMPLEX_DOUBLE\n");
            datatype = _DATATYPE_COMPLEX_DOUBLE;
        }

        if(strcmp(data.cmdargtoken[2].val.string, "u") == 0)
        {
            printf("type = USHORT\n");
            datatype = _DATATYPE_UINT16;
        }

        if(strcmp(data.cmdargtoken[2].val.string, "l") == 0)
        {
            printf("type = LONG\n");
            datatype = _DATATYPE_INT64;
        }

        if(datatype == 0)
        {
            printf("Data type \"%s\" not recognized\n", data.cmdargtoken[2].val.string);
            printf("must be : \n");
            printf("  c : CHAR\n");
            printf("  i : INT32\n");
            printf("  f : FLOAT\n");
            printf("  d : DOUBLE\n");
            printf("  cf: COMPLEX FLOAT\n");
            printf("  cd: COMPLEX DOUBLE\n");
            printf("  u : USHORT16\n");
            printf("  l : LONG64\n");
            return CLICMD_INVALID_ARG;
        }
        naxis = 0;
        imsize = (uint32_t *) malloc(sizeof(uint32_t) * 5);
        i = 3;
        while(data.cmdargtoken[i].type == 2)
        {
            imsize[naxis] = data.cmdargtoken[i].val.numl;
            naxis++;
            i++;
        }

        create_image_ID(data.cmdargtoken[1].val.string, naxis, imsize, datatype,
                        data.SHARED_DFT, data.NBKEWORD_DFT);

        free(imsize);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}





errno_t create_image_shared_cli() // default precision
{
    uint32_t *imsize;
    long naxis = 0;
    long i;


    if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        naxis = 0;
        imsize = (uint32_t *) malloc(sizeof(uint32_t) * 5);
        i = 2;
        while(data.cmdargtoken[i].type == 2)
        {
            imsize[naxis] = data.cmdargtoken[i].val.numl;
            naxis++;
            i++;
        }
        switch(data.precision)
        {
            case 0:
                create_image_ID(data.cmdargtoken[1].val.string, naxis, imsize, _DATATYPE_FLOAT,
                                1, data.NBKEWORD_DFT);
                break;
            case 1:
                create_image_ID(data.cmdargtoken[1].val.string, naxis, imsize, _DATATYPE_DOUBLE,
                                1, data.NBKEWORD_DFT);
                break;
        }
        free(imsize);
        printf("Creating 10 semaphores\n");
        COREMOD_MEMORY_image_set_createsem(data.cmdargtoken[1].val.string,
                                           IMAGE_NB_SEMAPHORE);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}




errno_t create_ushort_image_shared_cli() // default precision
{
    uint32_t *imsize;
    long naxis = 0;
    long i;


    if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        naxis = 0;
        imsize = (uint32_t *) malloc(sizeof(uint32_t) * 5);
        i = 2;
        while(data.cmdargtoken[i].type == 2)
        {
            imsize[naxis] = data.cmdargtoken[i].val.numl;
            naxis++;
            i++;
        }
        create_image_ID(data.cmdargtoken[1].val.string, naxis, imsize, _DATATYPE_UINT16,
                        1, data.NBKEWORD_DFT);

        free(imsize);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}




errno_t create_2Dimage_float()
{
    uint32_t *imsize;

    // CHECK ARGS
    //  printf("CREATING IMAGE\n");
    imsize = (uint32_t *) malloc(sizeof(uint32_t) * 2);

    imsize[0] = data.cmdargtoken[2].val.numl;
    imsize[1] = data.cmdargtoken[3].val.numl;

    create_image_ID(data.cmdargtoken[1].val.string, 2, imsize, _DATATYPE_FLOAT,
                    data.SHARED_DFT, data.NBKEWORD_DFT);

    free(imsize);

    return RETURN_SUCCESS;
}



errno_t create_3Dimage_float()
{
    uint32_t *imsize;

    // CHECK ARGS
    //  printf("CREATING 3D IMAGE\n");
    imsize = (uint32_t *) malloc(sizeof(uint32_t) * 3);

    imsize[0] = data.cmdargtoken[2].val.numl;
    imsize[1] = data.cmdargtoken[3].val.numl;
    imsize[2] = data.cmdargtoken[4].val.numl;

    create_image_ID(data.cmdargtoken[1].val.string, 3, imsize, _DATATYPE_FLOAT,
                    data.SHARED_DFT, data.NBKEWORD_DFT);

    free(imsize);

    return RETURN_SUCCESS;
}












/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 5. CREATE VARIABLE                                                                              */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 6. COPY IMAGE                                                                                   */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



errno_t copy_image_ID_cli()
{
    if(data.cmdargtoken[1].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[1].val.string);
        return CLICMD_INVALID_ARG;
    }

    copy_image_ID(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string,
                  0);

    return CLICMD_SUCCESS;
}




errno_t copy_image_ID_sharedmem_cli()
{
    if(data.cmdargtoken[1].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[1].val.string);
        return CLICMD_INVALID_ARG;
    }

    copy_image_ID(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string,
                  1);

    return CLICMD_SUCCESS;
}


errno_t chname_image_ID_cli()
{
    if(data.cmdargtoken[1].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[1].val.string);
        return CLICMD_INVALID_ARG;
    }

    chname_image_ID(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string);

    return CLICMD_SUCCESS;
}



errno_t COREMOD_MEMORY_cp2shm_cli()
{
    if(CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_STR_NOT_IMG)
            == 0)
    {
        COREMOD_MEMORY_cp2shm(data.cmdargtoken[1].val.string,
                              data.cmdargtoken[2].val.string);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}



/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 7. DISPLAY / LISTS                                                                              */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



errno_t memory_monitor_cli()
{
    memory_monitor(data.cmdargtoken[1].val.string);
    return CLICMD_SUCCESS;
}


errno_t list_variable_ID_file_cli()
{
    if(CLI_checkarg(1, CLIARG_STR_NOT_IMG) == 0)
    {
        list_variable_ID_file(data.cmdargtoken[1].val.string);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}



/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 8. TYPE CONVERSIONS TO AND FROM COMPLEX                                                         */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



errno_t mk_complex_from_reim_cli()
{
    if(data.cmdargtoken[1].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[1].val.string);
        return CLICMD_INVALID_ARG;
    }
    if(data.cmdargtoken[2].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[2].val.string);
        return CLICMD_INVALID_ARG;
    }

    mk_complex_from_reim(
        data.cmdargtoken[1].val.string,
        data.cmdargtoken[2].val.string,
        data.cmdargtoken[3].val.string,
        0);

    return CLICMD_SUCCESS;
}



errno_t mk_complex_from_amph_cli()
{
    if(data.cmdargtoken[1].type != 4)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[1].val.string);
        return CLICMD_INVALID_ARG;
    }
    if(data.cmdargtoken[2].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[2].val.string);
        return CLICMD_INVALID_ARG;
    }

    mk_complex_from_amph(
        data.cmdargtoken[1].val.string,
        data.cmdargtoken[2].val.string,
        data.cmdargtoken[3].val.string,
        0);

    return CLICMD_SUCCESS;
}



errno_t mk_reim_from_complex_cli()
{
    if(data.cmdargtoken[1].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[1].val.string);
        return CLICMD_INVALID_ARG;
    }

    mk_reim_from_complex(
        data.cmdargtoken[1].val.string,
        data.cmdargtoken[2].val.string,
        data.cmdargtoken[3].val.string,
        0);

    return CLICMD_SUCCESS;
}



errno_t mk_amph_from_complex_cli()
{
    if(data.cmdargtoken[1].type != CLIARG_IMG)
    {
        printf("Image %s does not exist\n", data.cmdargtoken[1].val.string);
        return CLICMD_INVALID_ARG;
    }

    mk_amph_from_complex(
        data.cmdargtoken[1].val.string,
        data.cmdargtoken[2].val.string,
        data.cmdargtoken[3].val.string,
        0);

    return CLICMD_SUCCESS;
}




/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 9. VERIFY SIZE                                                                                  */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 10. COORDINATE CHANGE                                                                           */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 11. SET IMAGE FLAGS / COUNTERS                                                                  */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */


errno_t COREMOD_MEMORY_image_set_status_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_status(
            data.cmdargtoken[1].val.string,
            (int) data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_set_cnt0_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_cnt0(
            data.cmdargtoken[1].val.string,
            (int) data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_set_cnt1_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_cnt1(
            data.cmdargtoken[1].val.string,
            (int) data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 12. MANAGE SEMAPHORES                                                                           */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




errno_t COREMOD_MEMORY_image_set_createsem_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_createsem(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_seminfo_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG) == 0)
    {
        COREMOD_MEMORY_image_seminfo(data.cmdargtoken[1].val.string);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_set_sempost_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_sempost(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_set_sempost_loop_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            + CLI_checkarg(3, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_sempost_loop(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}



errno_t COREMOD_MEMORY_image_set_semwait_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_semwait(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_set_semflush_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_set_semflush(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}




/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 13. SIMPLE OPERATIONS ON STREAMS                                                                */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




errno_t COREMOD_MEMORY_streamPoke_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_streamPoke(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}



errno_t COREMOD_MEMORY_streamDiff_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_IMG)
            + CLI_checkarg(3, 5)
            + CLI_checkarg(4, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(5, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_streamDiff(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.string,
            data.cmdargtoken[4].val.string,
            data.cmdargtoken[5].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_streamPaste_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_IMG)
            + CLI_checkarg(3, 5)
            + CLI_checkarg(4, CLIARG_LONG)
            + CLI_checkarg(5, CLIARG_LONG)
            + CLI_checkarg(6, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_streamPaste(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.string,
            data.cmdargtoken[4].val.numl,
            data.cmdargtoken[5].val.numl,
            data.cmdargtoken[6].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}



errno_t COREMOD_MEMORY_stream_halfimDiff_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_IMG)
            + CLI_checkarg(3, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_stream_halfimDiff(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}



errno_t COREMOD_MEMORY_streamAve_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, 5)
            == 0)
    {
        COREMOD_MEMORY_streamAve(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.string
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}




errno_t COREMOD_MEMORY_image_streamupdateloop_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, 5)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, CLIARG_LONG)
            + CLI_checkarg(5, CLIARG_LONG)
            + CLI_checkarg(6, CLIARG_LONG)
            + CLI_checkarg(7, 5)
            + CLI_checkarg(8, CLIARG_LONG)
            + CLI_checkarg(9, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_streamupdateloop(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.numl,
            data.cmdargtoken[5].val.numl,
            data.cmdargtoken[6].val.numl,
            data.cmdargtoken[7].val.string,
            data.cmdargtoken[8].val.numl,
            data.cmdargtoken[9].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_streamupdateloop_semtrig_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, 5)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, CLIARG_LONG)
            + CLI_checkarg(5, 5)
            + CLI_checkarg(6, CLIARG_LONG)
            + CLI_checkarg(7, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_streamupdateloop_semtrig(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.numl,
            data.cmdargtoken[5].val.string,
            data.cmdargtoken[6].val.numl,
            data.cmdargtoken[7].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


/*
int_fast8_t COREMOD_MEMORY_streamDelay_cli() {
    if(CLI_checkarg(1, 4) + CLI_checkarg(2, 5) + CLI_checkarg(3, 2) + CLI_checkarg(4, 2) == 0) {
        COREMOD_MEMORY_streamDelay(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string, data.cmdargtoken[3].val.numl, data.cmdargtoken[4].val.numl);
        return 0;
    } else {
        return 1;
    }
}
*/

errno_t COREMOD_MEMORY_streamDelay_cli()
{
    char fpsname[200];

    // First, we try to execute function through FPS interface
    if(0
            + CLI_checkarg(1, 5)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)   // check that first arg is string, second arg is int
    {
        unsigned int OptionalArg00 = data.cmdargtoken[2].val.numl;

        // Set FPS interface name
        // By convention, if there are optional arguments, they should be appended to the fps name
        //
        if(data.processnameflag ==
                0)   // name fps to something different than the process name
        {
            sprintf(fpsname, "streamDelay-%06u", OptionalArg00);
        }
        else     // Automatically set fps name to be process name up to first instance of character '.'
        {
            strcpy(fpsname, data.processname0);
        }

        if(strcmp(data.cmdargtoken[1].val.string,
                  "_FPSINIT_") == 0)    // Initialize FPS and conf process
        {
            printf("Function parameters configure\n");
            COREMOD_MEMORY_streamDelay_FPCONF(fpsname, FPSCMDCODE_FPSINIT);
            return RETURN_SUCCESS;
        }

        if(strcmp(data.cmdargtoken[1].val.string,
                  "_CONFSTART_") == 0)    // Start conf process
        {
            printf("Function parameters configure\n");
            COREMOD_MEMORY_streamDelay_FPCONF(fpsname, FPSCMDCODE_CONFSTART);
            return RETURN_SUCCESS;
        }

        if(strcmp(data.cmdargtoken[1].val.string,
                  "_CONFSTOP_") == 0)   // Stop conf process
        {
            printf("Function parameters configure\n");
            COREMOD_MEMORY_streamDelay_FPCONF(fpsname, FPSCMDCODE_CONFSTOP);
            return RETURN_SUCCESS;
        }

        if(strcmp(data.cmdargtoken[1].val.string, "_RUNSTART_") == 0)   // Run process
        {
            printf("Run function\n");
            COREMOD_MEMORY_streamDelay_RUN(fpsname);
            return RETURN_SUCCESS;
        }
        /*
                if(strcmp(data.cmdargtoken[1].val.string, "_RUNSTOP_") == 0) { // Cleanly stop process
                    printf("Run function\n");
                    COREMOD_MEMORY_streamDelay_STOP(OptionalArg00);
                    return RETURN_SUCCESS;
                }*/
    }

    // non FPS implementation - all parameters specified at function launch
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, 5)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_streamDelay(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }

}















errno_t COREMOD_MEMORY_SaveAll_snapshot_cli()
{
    if(0
            + CLI_checkarg(1, 5) == 0)
    {
        COREMOD_MEMORY_SaveAll_snapshot(data.cmdargtoken[1].val.string);
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_SaveAll_sequ_cli()
{
    if(0
            + CLI_checkarg(1, 5)
            + CLI_checkarg(2, CLIARG_IMG)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_SaveAll_sequ(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_testfunction_semaphore_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            + CLI_checkarg(3, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_testfunction_semaphore(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_NETWORKtransmit_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, CLIARG_LONG)
            + CLI_checkarg(5, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_NETWORKtransmit(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.numl,
            data.cmdargtoken[5].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_image_NETWORKreceive_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_LONG)
            + CLI_checkarg(2, CLIARG_LONG)
            + CLI_checkarg(3, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_image_NETWORKreceive(
            data.cmdargtoken[1].val.numl,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_PixMapDecode_U_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            + CLI_checkarg(3, CLIARG_LONG)
            + CLI_checkarg(4, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(5, CLIARG_IMG)
            + CLI_checkarg(6, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(7, CLIARG_STR_NOT_IMG)
            == 0)
    {
        COREMOD_MEMORY_PixMapDecode_U(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.string,
            data.cmdargtoken[5].val.string,
            data.cmdargtoken[6].val.string,
            data.cmdargtoken[7].val.string
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 14. DATA LOGGING                                                                                */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




errno_t COREMOD_MEMORY_logshim_printstatus_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            == 0)
    {
        COREMOD_MEMORY_logshim_printstatus(
            data.cmdargtoken[1].val.string
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_logshim_set_on_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        printf("logshim_set_on ----------------------\n");
        COREMOD_MEMORY_logshim_set_on(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_logshim_set_logexit_cli()
{
    if(0
            + CLI_checkarg(1, CLIARG_STR_NOT_IMG)
            + CLI_checkarg(2, CLIARG_LONG)
            == 0)
    {
        COREMOD_MEMORY_logshim_set_logexit(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}


errno_t COREMOD_MEMORY_sharedMem_2Dim_log_cli()
{

    if(CLI_checkarg_noerrmsg(4, CLIARG_STR_NOT_IMG) != 0)
    {
        sprintf(data.cmdargtoken[4].val.string, "null");
    }

    if(0
            + CLI_checkarg(1, 3)
            + CLI_checkarg(2, CLIARG_LONG)
            + CLI_checkarg(3, 3)
            == 0)
    {
        COREMOD_MEMORY_sharedMem_2Dim_log(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.string,
            data.cmdargtoken[4].val.string
        );
        return CLICMD_SUCCESS;
    }
    else
    {
        return CLICMD_INVALID_ARG;
    }
}













static errno_t init_module_CLI()
{

    RegisterCLIcommand(
        "cmemtestf",
        __FILE__,
        COREMOD_MEMORY_testfunc,
        "testfunc",
        "no arg",
        "cmemtestf",
        "COREMOD_MEMORY_testfunc()");

    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 1. MANAGE MEMORY AND IDENTIFIERS                                                                */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "mmon",
        __FILE__,
        memory_monitor_cli,
        "Monitor memory content",
        "terminal tty name",
        "mmon /dev/pts/4",
        "int memory_monitor(const char *ttyname)");

    RegisterCLIcommand(
        "rm",
        __FILE__,
        delete_image_ID_cli,
        "remove image(s)",
        "list of images",
        "rm im1 im4",
        "int delete_image_ID(char* imname)");

    RegisterCLIcommand(
        "rmall",
        __FILE__,
        clearall,
        "remove all images",
        "no argument",
        "rmall",
        "int clearall()");



    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 2. KEYWORDS                                                                                     */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "imwritekwL",
        __FILE__,
        image_write_keyword_L_cli,
        "write long type keyword",
        "<imname> <kname> <value [long]> <comment>",
        "imwritekwL im1 kw2 34 my_keyword_comment",
        "long image_write_keyword_L(const char *IDname, const char *kname, long value, const char *comment)");

    RegisterCLIcommand(
        "imlistkw",
        __FILE__,
        image_list_keywords_cli,
        "list image keywords",
        "<imname>",
        "imlistkw im1",
        "long image_list_keywords(const char *IDname)");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 3. READ SHARED MEM IMAGE AND SIZE                                                               */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "readshmimsize",
        __FILE__,
        read_sharedmem_image_size_cli,
        "read shared memory image size",
        "<name> <output file>",
        "readshmimsize im1 imsize.txt",
        "read_sharedmem_image_size(const char *name, const char *fname)");

    RegisterCLIcommand(
        "readshmim",
        __FILE__, read_sharedmem_image_cli,
        "read shared memory image",
        "<name>",
        "readshmim im1",
        "read_sharedmem_image(const char *name)");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 4. CREATE IMAGE                                                                                 */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */




    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 5. CREATE VARIABLE                                                                              */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "creaim",
        __FILE__,
        create_image_cli,
        "create image, default precision",
        "<name> <xsize> <ysize> <opt: zsize>",
        "creaim imname 512 512",
        "long create_image_ID(const char *name, long naxis, uint32_t *size, uint8_t datatype, 0, 10)");

    RegisterCLIcommand(
        "creaimshm",
        __FILE__, create_image_shared_cli,
        "create image in shared mem, default precision",
        "<name> <xsize> <ysize> <opt: zsize>",
        "creaimshm imname 512 512",
        "long create_image_ID(const char *name, long naxis, uint32_t *size, uint8_t datatype, 0, 10)");

    RegisterCLIcommand(
        "creaushortimshm",
        __FILE__,
        create_ushort_image_shared_cli,
        "create unsigned short image in shared mem",
        "<name> <xsize> <ysize> <opt: zsize>",
        "creaushortimshm imname 512 512",
        "long create_image_ID(const char *name, long naxis, long *size, _DATATYPE_UINT16, 0, 10)");

    RegisterCLIcommand(
        "crea3dim",
        __FILE__,
        create_3Dimage_float,
        "creates 3D image, single precision",
        "<name> <xsize> <ysize> <zsize>",
        "crea3dim imname 512 512 100",
        "long create_image_ID(const char *name, long naxis, long *size, _DATATYPE_FLOAT, 0, 10)");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 6. COPY IMAGE                                                                                   */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "cp",
        __FILE__, copy_image_ID_cli,
        "copy image",
        "source, dest",
        "cp im1 im4",
        "long copy_image_ID(const char *name, const char *newname, 0)");

    RegisterCLIcommand(
        "cpsh",
        __FILE__, copy_image_ID_sharedmem_cli,
        "copy image - create in shared mem if does not exist",
        "source, dest",
        "cp im1 im4",
        "long copy_image_ID(const char *name, const char *newname, 1)");

    RegisterCLIcommand(
        "mv",
        __FILE__, chname_image_ID_cli,
        "change image name",
        "source, dest",
        "mv im1 im4",
        "long chname_image_ID(const char *name, const char *newname)");

    RegisterCLIcommand(
        "imcp2shm",
        __FILE__,
        COREMOD_MEMORY_cp2shm_cli,
        "copy image ot shared memory",
        "<image> <shared mem image>",
        "imcp2shm im1 ims1",
        "long COREMOD_MEMORY_cp2shm(const char *IDname, const char *IDshmname)");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 7. DISPLAY / LISTS                                                                              */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "listim",
        __FILE__,
        list_image_ID,
        "list images in memory",
        "no argument",
        "listim", "int_fast8_t list_image_ID()");

    RegisterCLIcommand(
        "listvar",
        __FILE__,
        list_variable_ID,
        "list variables in memory",
        "no argument",
        "listvar",
        "int list_variable_ID()");

    RegisterCLIcommand(
        "listvarf",
        __FILE__,
        list_variable_ID_file_cli,
        "list variables in memory, write to file",
        "<file name>",
        "listvarf var.txt",
        "int list_variable_ID_file()");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 8. TYPE CONVERSIONS TO AND FROM COMPLEX                                                         */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "ri2c",
        __FILE__,
        mk_complex_from_reim_cli,
        "real, imaginary -> complex",
        "real imaginary complex",
        "ri2c imr imi imc",
        "int mk_complex_from_reim(const char *re_name, const char *im_name, const char *out_name)");

    RegisterCLIcommand(
        "ap2c",
        __FILE__,
        mk_complex_from_amph_cli,
        "ampl, pha -> complex",
        "ampl pha complex",
        "ap2c ima imp imc",
        "int mk_complex_from_amph(const char *re_name, const char *im_name, const char *out_name, int sharedmem)");

    RegisterCLIcommand(
        "c2ri",
        __FILE__,
        mk_reim_from_complex_cli,
        "complex -> real, imaginary",
        "complex real imaginary",
        "c2ri imc imr imi",
        "int mk_reim_from_complex(const char *re_name, const char *im_name, const char *out_name)");

    RegisterCLIcommand(
        "c2ap",
        __FILE__,
        mk_amph_from_complex_cli,
        "complex -> ampl, pha",
        "complex ampl pha",
        "c2ap imc ima imp",
        "int mk_amph_from_complex(const char *re_name, const char *im_name, const char *out_name, int sharedmem)");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 9. VERIFY SIZE                                                                                  */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */



    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 10. COORDINATE CHANGE                                                                           */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */



    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 11. SET IMAGE FLAGS / COUNTERS                                                                  */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "imsetstatus",
        __FILE__,
        COREMOD_MEMORY_image_set_status_cli,
        "set image status variable", "<image> <value [long]>",
        "imsetstatus im1 2",
        "long COREMOD_MEMORY_image_set_status(const char *IDname, int status)");

    RegisterCLIcommand(
        "imsetcnt0",
        __FILE__,
        COREMOD_MEMORY_image_set_cnt0_cli,
        "set image cnt0 variable", "<image> <value [long]>",
        "imsetcnt0 im1 2",
        "long COREMOD_MEMORY_image_set_cnt0(const char *IDname, int status)");

    RegisterCLIcommand(
        "imsetcnt1",
        __FILE__,
        COREMOD_MEMORY_image_set_cnt1_cli,
        "set image cnt1 variable", "<image> <value [long]>",
        "imsetcnt1 im1 2",
        "long COREMOD_MEMORY_image_set_cnt1(const char *IDname, int status)");



    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 12. MANAGE SEMAPHORES                                                                           */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "imsetcreatesem",
        __FILE__,
        COREMOD_MEMORY_image_set_createsem_cli,
        "create image semaphore",
        "<image> <NBsem>",
        "imsetcreatesem im1 5",
        "long COREMOD_MEMORY_image_set_createsem(const char *IDname, long NBsem)");

    RegisterCLIcommand(
        "imseminfo",
        __FILE__,
        COREMOD_MEMORY_image_seminfo_cli,
        "display semaphore info",
        "<image>",
        "imseminfo im1",
        "long COREMOD_MEMORY_image_seminfo(const char *IDname)");

    RegisterCLIcommand(
        "imsetsempost",
        __FILE__,
        COREMOD_MEMORY_image_set_sempost_cli,
        "post image semaphore. If sem index = -1, post all semaphores",
        "<image> <sem index>",
        "imsetsempost im1 2",
        "long COREMOD_MEMORY_image_set_sempost(const char *IDname, long index)");

    RegisterCLIcommand(
        "imsetsempostl",
        __FILE__,
        COREMOD_MEMORY_image_set_sempost_loop_cli,
        "post image semaphore loop. If sem index = -1, post all semaphores",
        "<image> <sem index> <time interval [us]>",
        "imsetsempostl im1 -1 1000",
        "long COREMOD_MEMORY_image_set_sempost_loop(const char *IDname, long index, long dtus)");

    RegisterCLIcommand(
        "imsetsemwait",
        __FILE__,
        COREMOD_MEMORY_image_set_semwait_cli,
        "wait image semaphore",
        "<image>",
        "imsetsemwait im1",
        "long COREMOD_MEMORY_image_set_semwait(const char *IDname)");

    RegisterCLIcommand(
        "imsetsemflush",
        __FILE__,
        COREMOD_MEMORY_image_set_semflush_cli,
        "flush image semaphore",
        "<image> <sem index>",
        "imsetsemflush im1 0",
        "long COREMOD_MEMORY_image_set_semflush(const char *IDname, long index)");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 13. SIMPLE OPERATIONS ON STREAMS                                                                */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "creaimstream",
        __FILE__,
        COREMOD_MEMORY_image_streamupdateloop_cli,
        "create 2D image stream from 3D cube",
        "<image3d in> <image2d out> <interval [us]> <NBcubes> <period> <offsetus> <sync stream name> <semtrig> <timing mode>",
        "creaimstream imcube imstream 1000 3 3 154 ircam1 3 0",
        "long COREMOD_MEMORY_image_streamupdateloop(const char *IDinname, const char *IDoutname, long usperiod, long NBcubes, long period, long offsetus, const char *IDsync_name, int semtrig, int timingmode)");

    RegisterCLIcommand(
        "creaimstreamstrig",
        __FILE__,
        COREMOD_MEMORY_image_streamupdateloop_semtrig_cli,
        "create 2D image stream from 3D cube, use other stream to synchronize",
        "<image3d in> <image2d out> <period [int]> <delay [us]> <sync stream> <sync sem index> <timing mode>",
        "creaimstreamstrig imcube outstream 3 152 streamsync 3 0",
        "long COREMOD_MEMORY_image_streamupdateloop_semtrig(const char *IDinname, const char *IDoutname, long period, long offsetus, const char *IDsync_name, int semtrig, int timingmode)");

    RegisterCLIcommand(
        "streamdelay",
        __FILE__,
        COREMOD_MEMORY_streamDelay_cli,
        "delay 2D image stream",
        "<image2d in> <image2d out> <delay [us]> <resolution [us]>",
        "streamdelay instream outstream 1000 10",
        "long COREMOD_MEMORY_streamDelay(const char *IDin_name, const char *IDout_name, long delayus, long dtus)");

    RegisterCLIcommand(
        "imsaveallsnap",
        __FILE__,
        COREMOD_MEMORY_SaveAll_snapshot_cli, "save all images in directory",
        "<directory>", "imsaveallsnap dir1",
        "long COREMOD_MEMORY_SaveAll_snapshot(const char *dirname)");

    RegisterCLIcommand(
        "imsaveallseq",
        __FILE__,
        COREMOD_MEMORY_SaveAll_sequ_cli,
        "save all images in directory - sequence",
        "<directory> <trigger image name> <trigger semaphore> <NB frames>",
        "imsaveallsequ dir1 im1 3 20",
        "long COREMOD_MEMORY_SaveAll_sequ(const char *dirname, const char *IDtrig_name, long semtrig, long NBframes)");

    RegisterCLIcommand(
        "testfuncsem",
        __FILE__,
        COREMOD_MEMORY_testfunction_semaphore_cli,
        "test semaphore loop",
        "<image> <semindex> <testmode>",
        "testfuncsem im1 1 0",
        "int COREMOD_MEMORY_testfunction_semaphore(const char *IDname, int semtrig, int testmode)");

    RegisterCLIcommand(
        "imnetwtransmit",
        __FILE__,
        COREMOD_MEMORY_image_NETWORKtransmit_cli,
        "transmit image over network",
        "<image> <IP addr> <port [long]> <sync mode [int]>",
        "imnetwtransmit im1 127.0.0.1 0 8888 0",
        "long COREMOD_MEMORY_image_NETWORKtransmit(const char *IDname, const char *IPaddr, int port, int mode)");

    RegisterCLIcommand(
        "imnetwreceive",
        __FILE__,
        COREMOD_MEMORY_image_NETWORKreceive_cli,
        "receive image(s) over network. mode=1 uses counter instead of semaphore",
        "<port [long]> <mode [int]> <RT priority>",
        "imnetwreceive 8887 0 80",
        "long COREMOD_MEMORY_image_NETWORKreceive(int port, int mode, int RT_priority)");

    RegisterCLIcommand(
        "impixdecodeU",
        __FILE__,
        COREMOD_MEMORY_PixMapDecode_U_cli,
        "decode image stream",
        "<in stream> <xsize [long]> <ysize [long]> <nbpix per slice [ASCII file]> <decode map> <out stream> <out image slice index [FITS]>",
        "impixdecodeU streamin 120 120 pixsclienb.txt decmap outim outsliceindex.fits",
        "COREMOD_MEMORY_PixMapDecode_U(const char *inputstream_name, uint32_t xsizeim, uint32_t ysizeim, const char* NBpix_fname, const char* IDmap_name, const char *IDout_name, const char *IDout_pixslice_fname)");

    RegisterCLIcommand(
        "streampoke",
        __FILE__,
        COREMOD_MEMORY_streamPoke_cli,
        "Poke image stream at regular interval",
        "<in stream> <poke period [us]>",
        "streampoke stream 100",
        "long COREMOD_MEMORY_streamPoke(const char *IDstream_name, long usperiod)");

    RegisterCLIcommand(
        "streamdiff",
        __FILE__,
        COREMOD_MEMORY_streamDiff_cli,
        "compute difference between two image streams",
        "<in stream 0> <in stream 1> <out stream> <optional mask> <sem trigger index>",
        "streamdiff stream0 stream1 null outstream 3",
        "long COREMOD_MEMORY_streamDiff(const char *IDstream0_name, const char *IDstream1_name, const char *IDstreamout_name, long semtrig)");

    RegisterCLIcommand(
        "streampaste",
        __FILE__,
        COREMOD_MEMORY_streamPaste_cli,
        "paste two 2D image streams of same size",
        "<in stream 0> <in stream 1> <out stream> <sem trigger0> <sem trigger1> <master>",
        "streampaste stream0 stream1 outstream 3 3 0",
        "long COREMOD_MEMORY_streamPaste(const char *IDstream0_name, const char *IDstream1_name, const char *IDstreamout_name, long semtrig0, long semtrig1, int master)");

    RegisterCLIcommand(
        "streamhalfdiff",
        __FILE__,
        COREMOD_MEMORY_stream_halfimDiff_cli,
        "compute difference between two halves of an image stream",
        "<in stream> <out stream> <sem trigger index>",
        "streamhalfdiff stream outstream 3",
        "long COREMOD_MEMORY_stream_halfimDiff(const char *IDstream_name, const char *IDstreamout_name, long semtrig)");

    RegisterCLIcommand(
        "streamave",
        __FILE__,
        COREMOD_MEMORY_streamAve_cli,
        "averages stream",
        "<instream> <NBave> <mode, 1 for single local instance, 0 for loop> <outstream>",
        "streamave instream 100 0 outstream",
        "long COREMODE_MEMORY_streamAve(const char *IDstream_name, int NBave, int mode, const char *IDout_name)");


    /* =============================================================================================== */
    /* =============================================================================================== */
    /*                                                                                                 */
    /* 14. DATA LOGGING                                                                                */
    /*                                                                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */


    RegisterCLIcommand(
        "shmimstreamlog",
        __FILE__,
        COREMOD_MEMORY_sharedMem_2Dim_log_cli,
        "logs shared memory stream (run in current directory)",
        "<shm image> <cubesize [long]> <logdir>",
        "shmimstreamlog wfscamim 10000 /media/data",
        "long COREMOD_MEMORY_sharedMem_2Dim_log(const char *IDname, uint32_t zsize, const char *logdir, const char *IDlogdata_name)");

    RegisterCLIcommand(
        "shmimslogstat",
        __FILE__,
        COREMOD_MEMORY_logshim_printstatus_cli,
        "print log shared memory stream status",
        "<shm image>", "shmimslogstat wfscamim",
        "int COREMOD_MEMORY_logshim_printstatus(const char *IDname)");

    RegisterCLIcommand(
        "shmimslogonset", __FILE__,
        COREMOD_MEMORY_logshim_set_on_cli,
        "set on variable in log shared memory stream",
        "<shm image> <setv [long]>",
        "shmimslogonset imwfs 1",
        "int COREMOD_MEMORY_logshim_set_on(const char *IDname, int setv)");

    RegisterCLIcommand(
        "shmimslogexitset",
        __FILE__,
        COREMOD_MEMORY_logshim_set_logexit_cli,
        "set exit variable in log shared memory stream",
        "<shm image> <setv [long]>",
        "shmimslogexitset imwfs 1",
        "int COREMOD_MEMORY_logshim_set_logexit(const char *IDname, int setv)");



    // add atexit functions here

    return RETURN_SUCCESS;
}







/**
 *
 * Test function aimed at creating unsolved seg fault bug
 * Will crash under gcc-7 if -O3 or -Ofast gcc compilation flag
 *
 * UPDATE: has been resolved (2019) - kept it for reference
 */
errno_t COREMOD_MEMORY_testfunc()
{
//	imageID   ID;
//	imageID   IDimc;
    uint32_t  xsize;
    uint32_t  ysize;
    uint32_t  xysize;
    uint32_t  ii;
    uint32_t *imsize;
    IMAGE     testimage_in;
    IMAGE     testimage_out;



    printf("Entering test function\n");
    fflush(stdout);

    xsize = 50;
    ysize = 50;
    imsize = (uint32_t *) malloc(sizeof(uint32_t) * 2);
    imsize[0] = xsize;
    imsize[1] = ysize;
    xysize = xsize * ysize;

    // create image (shared memory)
    ImageStreamIO_createIm(&testimage_in, "testimshm", 2, imsize, _DATATYPE_UINT32,
                           1, 0);

    // create image (local memory only)
    ImageStreamIO_createIm(&testimage_out, "testimout", 2, imsize, _DATATYPE_UINT32,
                           0, 0);


    // crashes with seg fault with gcc-7.3.0 -O3
    // tested with float (.F) -> crashes
    // tested with uint32 (.UI32) -> crashes
    // no crash with gcc-6.3.0 -O3
    // crashes with gcc-7.2.0 -O3
    for(ii = 0; ii < xysize; ii++)
    {
        testimage_out.array.UI32[ii] = testimage_in.array.UI32[ii];
    }

    // no seg fault with gcc-7.3.0 -O3
    //memcpy(testimage_out.array.UI32, testimage_in.array.UI32, SIZEOF_DATATYPE_UINT32*xysize);


    free(imsize);

    printf("No bug... clean exit\n");
    fflush(stdout);

    return RETURN_SUCCESS;
}












/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 1. MANAGE MEMORY AND IDENTIFIERS                                                                */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */


errno_t memory_monitor(
    const char *termttyname
)
{
    if(data.Debug > 0)
    {
        printf("starting memory_monitor on \"%s\"\n", termttyname);
    }

    MEM_MONITOR = 1;
    init_list_image_ID_ncurses(termttyname);
    list_image_ID_ncurses();
    atexit(close_list_image_ID_ncurses);

    return RETURN_SUCCESS;
}




long compute_nb_image()
{
    long NBimage = 0;

    for(imageID i = 0; i < data.NB_MAX_IMAGE; i++)
    {
        if(data.image[i].used == 1)
        {
            NBimage += 1;
        }
    }

    return NBimage;
}


long compute_nb_variable()
{
    long NBvar = 0;

    for(variableID i = 0; i < data.NB_MAX_VARIABLE; i++)
    {
        if(data.variable[i].used == 1)
        {
            NBvar += 1;
        }
    }

    return NBvar;
}



long long compute_image_memory()
{
    long long totalmem = 0;

//	printf("Computing num images\n");
//	fflush(stdout);

    for(imageID i = 0; i < data.NB_MAX_IMAGE; i++)
    {
        //printf("%5ld / %5ld  %d\n", i, data.NB_MAX_IMAGE, data.image[i].used);
        //	fflush(stdout);

        if(data.image[i].used == 1)
        {
            totalmem += data.image[i].md[0].nelement *
                        TYPESIZE[data.image[i].md[0].datatype];
        }
    }

    return totalmem;
}



long compute_variable_memory()
{
    long totalvmem = 0;

    for(variableID i = 0; i < data.NB_MAX_VARIABLE; i++)
    {
        totalvmem += sizeof(VARIABLE);
        if(data.variable[i].used == 1)
        {
            totalvmem += 0;
        }
    }
    return totalvmem;
}



/* ID number corresponding to a name */
imageID image_ID(
    const char *name
)
{
    imageID    i;
    int        loopOK;
    imageID    tmpID = 0;

    i = 0;
    loopOK = 1;
    while(loopOK == 1)
    {
        if(data.image[i].used == 1)
        {
            if((strncmp(name, data.image[i].name, strlen(name)) == 0)
                    && (data.image[i].name[strlen(name)] == '\0'))
            {
                loopOK = 0;
                tmpID = i;
                clock_gettime(CLOCK_REALTIME, &data.image[i].md[0].lastaccesstime);
            }
        }
        i++;

        if(i == data.NB_MAX_IMAGE)
        {
            loopOK = 0;
            tmpID = -1;
        }
    }

    return tmpID;
}


/* ID number corresponding to a name */
imageID image_ID_noaccessupdate(
    const char *name
)
{
    imageID   i;
    imageID   tmpID = 0;
    int       loopOK;

    i = 0;
    loopOK = 1;
    while(loopOK == 1)
    {
        if(data.image[i].used == 1)
        {
            if((strncmp(name, data.image[i].name, strlen(name)) == 0)
                    && (data.image[i].name[strlen(name)] == '\0'))
            {
                loopOK = 0;
                tmpID = i;
            }
        }
        i++;

        if(i == data.NB_MAX_IMAGE)
        {
            loopOK = 0;
            tmpID = -1;
        }
    }

    return tmpID;
}


/* ID number corresponding to a name */
variableID variable_ID(
    const char *name
)
{
    variableID i;
    variableID tmpID;
    int        loopOK;

    i = 0;
    loopOK = 1;
    while(loopOK == 1)
    {
        if(data.variable[i].used == 1)
        {
            if((strncmp(name, data.variable[i].name, strlen(name)) == 0)
                    && (data.variable[i].name[strlen(name)] == '\0'))
            {
                loopOK = 0;
                tmpID = i;
            }
        }

        i++;
        if(i == data.NB_MAX_VARIABLE)
        {
            loopOK = 0;
            tmpID = -1;
        }
    }

    return tmpID;
}



/* next available ID number */
imageID next_avail_image_ID()
{
    imageID i;
    imageID ID = -1;

# ifdef _OPENMP
    #pragma omp critical
    {
#endif
        for(i = 0; i < data.NB_MAX_IMAGE; i++)
        {
            if(data.image[i].used == 0)
            {
                ID = i;
                data.image[ID].used = 1;
                break;
            }
        }
# ifdef _OPENMP
    }
# endif

    if(ID == -1)
    {
        printf("ERROR: ran out of image IDs - cannot allocate new ID\n");
        printf("NB_MAX_IMAGE should be increased above current value (%ld)\n",
               data.NB_MAX_IMAGE);
        exit(0);
    }

    return ID;
}


/* next available ID number */
variableID next_avail_variable_ID()
{
    variableID i;
    variableID ID = -1;
    int        found = 0;

    for(i = 0; i < data.NB_MAX_VARIABLE; i++)
    {
        if((data.variable[i].used == 0) && (found == 0))
        {
            ID = i;
            found = 1;
        }
    }

    if(ID == -1)
    {
        ID = data.NB_MAX_VARIABLE;
    }

    return ID;
}



/* deletes an ID */
errno_t delete_image_ID(
    const char *imname
)
{
    imageID ID;
    long    s;
    char    fname[200];

    ID = image_ID(imname);

    if(ID != -1)
    {
        data.image[ID].used = 0;

        if(data.image[ID].md[0].shared == 1)
        {
            for(s = 0; s < data.image[ID].md[0].sem; s++)
            {
                sem_close(data.image[ID].semptr[s]);
            }

            free(data.image[ID].semptr);
            data.image[ID].semptr = NULL;


            if(data.image[ID].semlog != NULL)
            {
                sem_close(data.image[ID].semlog);
                data.image[ID].semlog = NULL;
            }

            if(munmap(data.image[ID].md, data.image[ID].memsize) == -1)
            {
                printf("unmapping ID %ld : %p  %ld\n", ID, data.image[ID].md,
                       data.image[ID].memsize);
                perror("Error un-mmapping the file");
            }

            close(data.image[ID].shmfd);
            data.image[ID].shmfd = -1;

            data.image[ID].md = NULL;
            data.image[ID].kw = NULL;

            data.image[ID].memsize = 0;

            if(data.rmSHMfile == 1)    // remove files from disk
            {
                EXECUTE_SYSTEM_COMMAND("rm /dev/shm/sem.%s.%s_sem*",
                        data.shmsemdirname, imname);
                sprintf(fname, "/dev/shm/sem.%s.%s_semlog", data.shmsemdirname, imname);
                remove(fname);
								
                EXECUTE_SYSTEM_COMMAND("rm %s/%s.im.shm", data.shmdir, imname);
            }

        }
        else
        {
            if(data.image[ID].md[0].datatype == _DATATYPE_UINT8)
            {
                if(data.image[ID].array.UI8 == NULL)
                {
                    PRINT_ERROR("data array pointer is null\n");
                    exit(EXIT_FAILURE);
                }
                free(data.image[ID].array.UI8);
                data.image[ID].array.UI8 = NULL;
            }
            if(data.image[ID].md[0].datatype == _DATATYPE_INT32)
            {
                if(data.image[ID].array.SI32 == NULL)
                {
                    PRINT_ERROR("data array pointer is null\n");
                    exit(EXIT_FAILURE);
                }
                free(data.image[ID].array.SI32);
                data.image[ID].array.SI32 = NULL;
            }
            if(data.image[ID].md[0].datatype == _DATATYPE_FLOAT)
            {
                if(data.image[ID].array.F == NULL)
                {
                    PRINT_ERROR("data array pointer is null\n");
                    exit(EXIT_FAILURE);
                }
                free(data.image[ID].array.F);
                data.image[ID].array.F = NULL;
            }
            if(data.image[ID].md[0].datatype == _DATATYPE_DOUBLE)
            {
                if(data.image[ID].array.D == NULL)
                {
                    PRINT_ERROR("data array pointer is null\n");
                    exit(EXIT_FAILURE);
                }
                free(data.image[ID].array.D);
                data.image[ID].array.D = NULL;
            }
            if(data.image[ID].md[0].datatype == _DATATYPE_COMPLEX_FLOAT)
            {
                if(data.image[ID].array.CF == NULL)
                {
                    PRINT_ERROR("data array pointer is null\n");
                    exit(EXIT_FAILURE);
                }
                free(data.image[ID].array.CF);
                data.image[ID].array.CF = NULL;
            }
            if(data.image[ID].md[0].datatype == _DATATYPE_COMPLEX_DOUBLE)
            {
                if(data.image[ID].array.CD == NULL)
                {
                    PRINT_ERROR("data array pointer is null\n");
                    exit(EXIT_FAILURE);
                }
                free(data.image[ID].array.CD);
                data.image[ID].array.CD = NULL;
            }

            if(data.image[ID].md == NULL)
            {
                PRINT_ERROR("data array pointer is null\n");
                exit(0);
            }
            free(data.image[ID].md);
            data.image[ID].md = NULL;


            if(data.image[ID].kw != NULL)
            {
                free(data.image[ID].kw);
                data.image[ID].kw = NULL;
            }

        }
        //free(data.image[ID].logstatus);
        /*      free(data.image[ID].size);*/
        //      data.image[ID].md[0].last_access = 0;
    }
    else
    {
        fprintf(stderr,
                "%c[%d;%dm WARNING: image %s does not exist [ %s  %s  %d ] %c[%d;m\n",
                (char) 27, 1, 31, imname, __FILE__, __func__, __LINE__, (char) 27, 0);
    }

    if(MEM_MONITOR == 1)
    {
        list_image_ID_ncurses();
    }

    return RETURN_SUCCESS;
}



// delete all images with a prefix
errno_t delete_image_ID_prefix(
    const char *prefix
)
{
    imageID i;

    for(i = 0; i < data.NB_MAX_IMAGE; i++)
    {
        if(data.image[i].used == 1)
            if((strncmp(prefix, data.image[i].name, strlen(prefix))) == 0)
            {
                printf("deleting image %s\n", data.image[i].name);
                delete_image_ID(data.image[i].name);
            }
    }
    return RETURN_SUCCESS;
}


/* deletes a variable ID */
errno_t delete_variable_ID(
    const char *varname
)
{
    imageID ID;

    ID = variable_ID(varname);
    if(ID != -1)
    {
        data.variable[ID].used = 0;
        /*      free(data.variable[ID].name);*/
    }
    else
        fprintf(stderr,
                "%c[%d;%dm WARNING: variable %s does not exist [ %s  %s  %d ] %c[%d;m\n",
                (char) 27, 1, 31, varname, __FILE__, __func__, __LINE__, (char) 27, 0);

    return RETURN_SUCCESS;
}



errno_t clearall()
{
    imageID ID;

    for(ID = 0; ID < data.NB_MAX_IMAGE; ID++)
    {
        if(data.image[ID].used == 1)
        {
            delete_image_ID(data.image[ID].name);
        }
    }
    for(ID = 0; ID < data.NB_MAX_VARIABLE; ID++)
    {
        if(data.variable[ID].used == 1)
        {
            delete_variable_ID(data.variable[ID].name);
        }
    }

    return RETURN_SUCCESS;
}






/**
 * ## Purpose
 *
 * Save telemetry stream data
 *
 */
void *save_fits_function(
    void *ptr
)
{
    imageID  ID;


    //struct savethreadmsg *tmsg; // = malloc(sizeof(struct savethreadmsg));
    STREAMSAVE_THREAD_MESSAGE *tmsg;

    uint32_t     *imsizearray;
    uint32_t      xsize, ysize;
    uint8_t       datatype;


    imageID       IDc;
    long          framesize;  // in bytes
    char         *ptr0;       // source pointer
    char         *ptr1;       // destination pointer
    long          k;
    FILE         *fp;


    int RT_priority = 20;
    struct sched_param schedpar;


    schedpar.sched_priority = RT_priority;
#ifndef __MACH__
    if(seteuid(data.euid) != 0)     //This goes up to maximum privileges
    {
        PRINT_ERROR("seteuid error");
    }
    sched_setscheduler(0, SCHED_FIFO,
                       &schedpar); //other option is SCHED_RR, might be faster
    if(seteuid(data.ruid) != 0)     //Go back to normal privileges
    {
        PRINT_ERROR("seteuid error");
    }
#endif



    imsizearray = (uint32_t *) malloc(sizeof(uint32_t) * 3);

    //    tmsg = (struct savethreadmsg*) ptr;
    tmsg = (STREAMSAVE_THREAD_MESSAGE *) ptr;

    // printf("THREAD : SAVING  %s -> %s \n", tmsg->iname, tmsg->fname);
    //fflush(stdout);
    if(tmsg->partial == 0) // full image
    {
        save_fits(tmsg->iname, tmsg->fname);
    }
    else
    {
        //      printf("Saving partial image (name = %s   zsize = %ld)\n", tmsg->iname, tmsg->cubesize);

        //	list_image_ID();

        ID = image_ID(tmsg->iname);
        datatype = data.image[ID].md[0].datatype;
        xsize = data.image[ID].md[0].size[0];
        ysize = data.image[ID].md[0].size[1];

        //printf("step00\n");
        //fflush(stdout);

        imsizearray[0] = xsize;
        imsizearray[1] = ysize;
        imsizearray[2] = tmsg->cubesize;



        IDc = create_image_ID("tmpsavecube", 3, imsizearray, datatype, 0, 1);

        // list_image_ID();

        switch(datatype)
        {

            case _DATATYPE_UINT8:
                framesize = SIZEOF_DATATYPE_UINT8 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.UI8; // source
                ptr1 = (char *) data.image[IDc].array.UI8; // destination
                break;
            case _DATATYPE_INT8:
                framesize = SIZEOF_DATATYPE_INT8 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.SI8; // source
                ptr1 = (char *) data.image[IDc].array.SI8; // destination
                break;

            case _DATATYPE_UINT16:
                framesize = SIZEOF_DATATYPE_UINT16 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.UI16; // source
                ptr1 = (char *) data.image[IDc].array.UI16; // destination
                break;
            case _DATATYPE_INT16:
                framesize = SIZEOF_DATATYPE_INT16 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.SI16; // source
                ptr1 = (char *) data.image[IDc].array.SI16; // destination
                break;

            case _DATATYPE_UINT32:
                framesize = SIZEOF_DATATYPE_UINT32 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.UI32; // source
                ptr1 = (char *) data.image[IDc].array.UI32; // destination
                break;
            case _DATATYPE_INT32:
                framesize = SIZEOF_DATATYPE_INT32 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.SI32; // source
                ptr1 = (char *) data.image[IDc].array.SI32; // destination
                break;

            case _DATATYPE_UINT64:
                framesize = SIZEOF_DATATYPE_UINT64 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.UI64; // source
                ptr1 = (char *) data.image[IDc].array.UI64; // destination
                break;
            case _DATATYPE_INT64:
                framesize = SIZEOF_DATATYPE_INT64 * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.SI64; // source
                ptr1 = (char *) data.image[IDc].array.SI64; // destination
                break;

            case _DATATYPE_FLOAT:
                framesize = SIZEOF_DATATYPE_FLOAT * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.F; // source
                ptr1 = (char *) data.image[IDc].array.F; // destination
                break;
            case _DATATYPE_DOUBLE:
                framesize = SIZEOF_DATATYPE_DOUBLE * xsize * ysize;
                ptr0 = (char *) data.image[ID].array.D; // source
                ptr1 = (char *) data.image[IDc].array.D; // destination
                break;

            default:
                printf("ERROR: WRONG DATA TYPE\n");
                free(imsizearray);
                free(tmsg);
                exit(0);
                break;
        }


        memcpy((void *) ptr1, (void *) ptr0, framesize * tmsg->cubesize);
        save_fits("tmpsavecube", tmsg->fname);
        delete_image_ID("tmpsavecube");
    }

    if(tmsg->saveascii == 1)
    {
        if((fp = fopen(tmsg->fnameascii, "w")) == NULL)
        {
            printf("ERROR: cannot create file \"%s\"\n", tmsg->fnameascii);
            exit(0);
        }

        fprintf(fp, "# Telemetry stream timing data \n");
        fprintf(fp, "# File written by function %s in file %s\n", __FUNCTION__,
                __FILE__);
        fprintf(fp, "# \n");
        fprintf(fp, "# col1 : datacube frame index\n");
        fprintf(fp, "# col2 : Main index\n");
        fprintf(fp, "# col3 : Time since cube origin\n");
        fprintf(fp, "# col4 : Absolute time\n");
        fprintf(fp, "# col5 : stream cnt0 index\n");
        fprintf(fp, "# col6 : stream cnt1 index\n");
        fprintf(fp, "# \n");

        double t0; // time reference
        t0 = tmsg->arraytime[0];
        for(k = 0; k < tmsg->cubesize; k++)
        {
            //fprintf(fp, "%6ld   %10lu  %10lu   %15.9lf\n", k, tmsg->arraycnt0[k], tmsg->arraycnt1[k], tmsg->arraytime[k]);

            // entries are:
            // - index within cube
            // - loop index (if applicable)
            // - time since cube start
            // - time (absolute)
            // - cnt0
            // - cnt1

            fprintf(fp, "%10ld  %10lu  %15.9lf   %20.9lf  %10ld   %10ld\n", k,
                    tmsg->arrayindex[k], tmsg->arraytime[k] - t0, tmsg->arraytime[k],
                    tmsg->arraycnt0[k], tmsg->arraycnt1[k]);
        }
        fclose(fp);
    }

    //    printf(" DONE\n");
    //fflush(stdout);

    ID = image_ID(tmsg->iname);
    tret = ID;
    free(imsizearray);
    pthread_exit(&tret);

    //  free(tmsg);
}










/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 2. KEYWORDS                                                                                     */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




long image_write_keyword_L(
    const char *IDname,
    const char *kname,
    long        value,
    const char *comment
)
{
    imageID  ID;
    long     kw, NBkw, kw0;

    ID = image_ID(IDname);
    NBkw = data.image[ID].md[0].NBkw;

    kw = 0;
    while((data.image[ID].kw[kw].type != 'N') && (kw < NBkw))
    {
        kw++;
    }
    kw0 = kw;

    if(kw0 == NBkw)
    {
        printf("ERROR: no available keyword entry\n");
        exit(0);
    }
    else
    {
        strcpy(data.image[ID].kw[kw].name, kname);
        data.image[ID].kw[kw].type = 'L';
        data.image[ID].kw[kw].value.numl = value;
        strcpy(data.image[ID].kw[kw].comment, comment);
    }

    return kw0;
}



long image_write_keyword_D(
    const char *IDname,
    const char *kname,
    double      value,
    const char *comment
)
{
    imageID  ID;
    long     kw;
    long     NBkw;
    long     kw0;

    ID = image_ID(IDname);
    NBkw = data.image[ID].md[0].NBkw;

    kw = 0;
    while((data.image[ID].kw[kw].type != 'N') && (kw < NBkw))
    {
        kw++;
    }
    kw0 = kw;

    if(kw0 == NBkw)
    {
        printf("ERROR: no available keyword entry\n");
        exit(0);
    }
    else
    {
        strcpy(data.image[ID].kw[kw].name, kname);
        data.image[ID].kw[kw].type = 'D';
        data.image[ID].kw[kw].value.numf = value;
        strcpy(data.image[ID].kw[kw].comment, comment);
    }

    return kw0;
}



long image_write_keyword_S(
    const char *IDname,
    const char *kname,
    const char *value,
    const char *comment
)
{
    imageID ID;
    long    kw;
    long    NBkw;
    long    kw0;

    ID = image_ID(IDname);
    NBkw = data.image[ID].md[0].NBkw;

    kw = 0;
    while((data.image[ID].kw[kw].type != 'N') && (kw < NBkw))
    {
        kw++;
    }
    kw0 = kw;

    if(kw0 == NBkw)
    {
        printf("ERROR: no available keyword entry\n");
        exit(0);
    }
    else
    {
        strcpy(data.image[ID].kw[kw].name, kname);
        data.image[ID].kw[kw].type = 'D';
        strcpy(data.image[ID].kw[kw].value.valstr, value);
        strcpy(data.image[ID].kw[kw].comment, comment);
    }

    return kw0;
}




imageID image_list_keywords(
    const char *IDname
)
{
    imageID ID;
    long    kw;

    ID = image_ID(IDname);

    for(kw = 0; kw < data.image[ID].md[0].NBkw; kw++)
    {
        if(data.image[ID].kw[kw].type == 'L')
        {
            printf("%18s  %20ld %s\n", data.image[ID].kw[kw].name,
                   data.image[ID].kw[kw].value.numl, data.image[ID].kw[kw].comment);
        }
        if(data.image[ID].kw[kw].type == 'D')
        {
            printf("%18s  %20lf %s\n", data.image[ID].kw[kw].name,
                   data.image[ID].kw[kw].value.numf, data.image[ID].kw[kw].comment);
        }
        if(data.image[ID].kw[kw].type == 'S')
        {
            printf("%18s  %20s %s\n", data.image[ID].kw[kw].name,
                   data.image[ID].kw[kw].value.valstr, data.image[ID].kw[kw].comment);
        }
        //	if(data.image[ID].kw[kw].type=='N')
        //	printf("-------------\n");
    }

    return ID;
}




long image_read_keyword_D(
    const char *IDname,
    const char *kname,
    double     *val
)
{
    variableID  ID;
    long        kw;
    long        kw0;

    ID = image_ID(IDname);
    kw0 = -1;
    for(kw = 0; kw < data.image[ID].md[0].NBkw; kw++)
    {
        if((data.image[ID].kw[kw].type == 'D')
                && (strncmp(kname, data.image[ID].kw[kw].name, strlen(kname)) == 0))
        {
            kw0 = kw;
            *val = data.image[ID].kw[kw].value.numf;
        }
    }

    return kw0;
}



long image_read_keyword_L(
    const char *IDname,
    const char *kname,
    long       *val
)
{
    variableID ID;
    long       kw;
    long       kw0;

    ID = image_ID(IDname);
    kw0 = -1;
    for(kw = 0; kw < data.image[ID].md[0].NBkw; kw++)
    {
        if((data.image[ID].kw[kw].type == 'L')
                && (strncmp(kname, data.image[ID].kw[kw].name, strlen(kname)) == 0))
        {
            kw0 = kw;
            *val = data.image[ID].kw[kw].value.numl;
        }
    }

    return kw0;
}





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 3. READ SHARED MEM IMAGE AND SIZE                                                               */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




/**
 *  ## Purpose
 *
 *  Read shared memory image size
 *
 *
 * ## Arguments
 *
 * @param[in]
 * name		char*
 * -		stream name
 *
 * @param[in]
 * fname	char*
 * 			file name to write image name
 *
 */
imageID read_sharedmem_image_size(
    const char *name,
    const char *fname
)
{
    int             SM_fd;
    struct          stat file_stat;
    char            SM_fname[200];
    IMAGE_METADATA *map;
    int             i;
    FILE           *fp;
    imageID         ID = -1;


    if((ID = image_ID(name)) == -1)
    {
        sprintf(SM_fname, "%s/%s.im.shm", data.shmdir, name);

        SM_fd = open(SM_fname, O_RDWR);
        if(SM_fd == -1)
        {
            printf("Cannot import file - continuing\n");
        }
        else
        {
            fstat(SM_fd, &file_stat);
            //        printf("File %s size: %zd\n", SM_fname, file_stat.st_size);

            map = (IMAGE_METADATA *) mmap(0, sizeof(IMAGE_METADATA), PROT_READ | PROT_WRITE,
                                          MAP_SHARED, SM_fd, 0);
            if(map == MAP_FAILED)
            {
                close(SM_fd);
                perror("Error mmapping the file");
                exit(0);
            }

            fp = fopen(fname, "w");
            for(i = 0; i < map[0].naxis; i++)
            {
                fprintf(fp, "%ld ", (long) map[0].size[i]);
            }
            fprintf(fp, "\n");
            fclose(fp);


            if(munmap(map, sizeof(IMAGE_METADATA)) == -1)
            {
                printf("unmapping %s\n", SM_fname);
                perror("Error un-mmapping the file");
            }
            close(SM_fd);
        }
    }
    else
    {
        fp = fopen(fname, "w");
        for(i = 0; i < data.image[ID].md[0].naxis; i++)
        {
            fprintf(fp, "%ld ", (long) data.image[ID].md[0].size[i]);
        }
        fprintf(fp, "\n");
        fclose(fp);
    }

    return ID;
}











imageID read_sharedmem_image(
    const char *name
)
{
    imageID ID = -1;
    imageID IDmem = 0;
    IMAGE *image;

    IDmem = next_avail_image_ID();

    image = &data.image[IDmem];
    if(ImageStreamIO_read_sharedmem_image_toIMAGE(name, image) == EXIT_FAILURE)
    {
        printf("read shared mem image failed -> ID = -1\n");
        fflush(stdout);
        ID = -1;
    }
    else
    {
        ID = image_ID(name);
        printf("read shared mem image success -> ID = %ld\n", ID);
        fflush(stdout);
    }

    if(MEM_MONITOR == 1)
    {
        list_image_ID_ncurses();
    }

    return ID;
}









/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 4. CREATE IMAGE                                                                                 */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */






/* creates an image ID */
/* all images should be created by this function */
imageID create_image_ID(
    const char *name,
    long        naxis,
    uint32_t   *size,
    uint8_t     datatype,
    int         shared,
    int         NBkw
)
{
    imageID ID;
    long    i;


    ID = -1;
    if(image_ID(name) == -1)
    {
        ID = next_avail_image_ID();
        ImageStreamIO_createIm(&data.image[ID], name, naxis, size, datatype, shared,
                               NBkw);
    }
    else
    {
        // Cannot create image : name already in use
        ID = image_ID(name);

        if(data.image[ID].md[0].datatype != datatype)
        {
            fprintf(stderr, "%c[%d;%dm ERROR: [ %s %s %d ] %c[%d;m\n", (char) 27, 1, 31,
                    __FILE__, __func__, __LINE__, (char) 27, 0);
            fprintf(stderr, "%c[%d;%dm Pre-existing image \"%s\" has wrong type %c[%d;m\n",
                    (char) 27, 1, 31, name, (char) 27, 0);
            exit(0);
        }
        if(data.image[ID].md[0].naxis != naxis)
        {
            fprintf(stderr, "%c[%d;%dm ERROR: [ %s %s %d ] %c[%d;m\n", (char) 27, 1, 31,
                    __FILE__, __func__, __LINE__, (char) 27, 0);
            fprintf(stderr, "%c[%d;%dm Pre-existing image \"%s\" has wrong naxis %c[%d;m\n",
                    (char) 27, 1, 31, name, (char) 27, 0);
            exit(0);
        }

        for(i = 0; i < naxis; i++)
            if(data.image[ID].md[0].size[i] != size[i])
            {
                fprintf(stderr, "%c[%d;%dm ERROR: [ %s %s %d ] %c[%d;m\n", (char) 27, 1, 31,
                        __FILE__, __func__, __LINE__, (char) 27, 0);
                fprintf(stderr, "%c[%d;%dm Pre-existing image \"%s\" has wrong size %c[%d;m\n",
                        (char) 27, 1, 31, name, (char) 27, 0);
                fprintf(stderr, "Axis %ld :  %ld  %ld\n", i,
                        (long) data.image[ID].md[0].size[i], (long) size[i]);
                exit(0);
            }
    }

    if(MEM_MONITOR == 1)
    {
        list_image_ID_ncurses();
    }

    return ID;
}








imageID create_1Dimage_ID(
    const char *ID_name,
    uint32_t    xsize
)
{
    imageID ID = -1;
    long naxis = 1;
    uint32_t naxes[1];

    naxes[0] = xsize;

    if(data.precision == 0)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_FLOAT, data.SHARED_DFT,
                             data.NBKEWORD_DFT);    // single precision
    }
    if(data.precision == 1)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_DOUBLE, data.SHARED_DFT,
                             data.NBKEWORD_DFT);    // double precision
    }

    return ID;
}



imageID create_1DCimage_ID(
    const char *ID_name,
    uint32_t    xsize
)
{
    imageID ID = -1;
    long naxis = 1;
    uint32_t naxes[1];

    naxes[0] = xsize;

    if(data.precision == 0)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_COMPLEX_FLOAT,
                             data.SHARED_DFT, data.NBKEWORD_DFT);    // single precision
    }
    if(data.precision == 1)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_COMPLEX_DOUBLE,
                             data.SHARED_DFT, data.NBKEWORD_DFT);    // double precision
    }

    return ID;
}



imageID create_2Dimage_ID(
    const char *ID_name,
    uint32_t    xsize,
    uint32_t    ysize
)
{
    imageID ID = -1;
    long naxis = 2;
    uint32_t naxes[2];

    naxes[0] = xsize;
    naxes[1] = ysize;

    if(data.precision == 0)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_FLOAT, data.SHARED_DFT,
                             data.NBKEWORD_DFT);    // single precision
    }
    else if(data.precision == 1)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_DOUBLE, data.SHARED_DFT,
                             data.NBKEWORD_DFT);    // double precision
    }
    else
    {
        printf("Default precision (%d) invalid value: assuming single precision\n",
               data.precision);
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_FLOAT, data.SHARED_DFT,
                             data.NBKEWORD_DFT); // single precision
    }

    return ID;
}




imageID create_2Dimage_ID_double(
    const char *ID_name,
    uint32_t    xsize,
    uint32_t    ysize
)
{
    imageID ID = -1;
    long naxis = 2;
    uint32_t naxes[2];

    naxes[0] = xsize;
    naxes[1] = ysize;

    ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_DOUBLE, data.SHARED_DFT,
                         data.NBKEWORD_DFT);

    return ID;
}


/* 2D complex image */
imageID create_2DCimage_ID(
    const char *ID_name,
    uint32_t    xsize,
    uint32_t    ysize
)
{
    imageID ID = -1;
    long naxis = 2;
    uint32_t naxes[2];

    naxes[0] = xsize;
    naxes[1] = ysize;

    if(data.precision == 0)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_COMPLEX_FLOAT,
                             data.SHARED_DFT, data.NBKEWORD_DFT);    // single precision
    }
    if(data.precision == 1)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_COMPLEX_DOUBLE,
                             data.SHARED_DFT, data.NBKEWORD_DFT);    // double precision
    }

    return ID;
}



/* 2D complex image */
imageID create_2DCimage_ID_double(
    const char    *ID_name,
    uint32_t       xsize,
    uint32_t       ysize
)
{
    imageID ID = -1;
    long naxis = 2;
    uint32_t naxes[2];

    naxes[0] = xsize;
    naxes[1] = ysize;

    ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_COMPLEX_DOUBLE,
                         data.SHARED_DFT, data.NBKEWORD_DFT); // double precision

    return ID;
}



/* 3D image, single precision */
imageID create_3Dimage_ID_float(
    const char *ID_name,
    uint32_t xsize,
    uint32_t ysize,
    uint32_t zsize
)
{
    imageID ID = -1;
    long naxis = 3;
    uint32_t naxes[3];

    naxes[0] = xsize;
    naxes[1] = ysize;
    naxes[2] = zsize;

    //  printf("CREATING 3D IMAGE: %s %ld %ld %ld\n", ID_name, xsize, ysize, zsize);
    //  fflush(stdout);

    ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_FLOAT, data.SHARED_DFT,
                         data.NBKEWORD_DFT); // single precision

    //  printf("IMAGE CREATED WITH ID = %ld\n",ID);
    //  fflush(stdout);

    return ID;
}


/* 3D image, double precision */
imageID create_3Dimage_ID_double(
    const char *ID_name,
    uint32_t xsize,
    uint32_t ysize,
    uint32_t zsize
)
{
    imageID ID;
    long naxis = 3;
    uint32_t naxes[3];

    naxes[0] = xsize;
    naxes[1] = ysize;
    naxes[2] = zsize;

    ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_DOUBLE, data.SHARED_DFT,
                         data.NBKEWORD_DFT); // double precision

    return ID;
}



/* 3D image, default precision */
imageID create_3Dimage_ID(
    const char *ID_name,
    uint32_t xsize,
    uint32_t ysize,
    uint32_t zsize
)
{
    imageID ID = -1;
    long naxis = 3;
    uint32_t *naxes;


    naxes = (uint32_t *) malloc(sizeof(uint32_t) * 3);
    naxes[0] = xsize;
    naxes[1] = ysize;
    naxes[2] = zsize;

    if(data.precision == 0)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_FLOAT, data.SHARED_DFT,
                             data.NBKEWORD_DFT); // single precision
    }

    if(data.precision == 1)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_DOUBLE, data.SHARED_DFT,
                             data.NBKEWORD_DFT); // double precision
    }

    free(naxes);

    return ID;
}


/* 3D complex image */
imageID create_3DCimage_ID(
    const char *ID_name,
    uint32_t xsize,
    uint32_t ysize,
    uint32_t zsize
)
{
    imageID ID = -1;
    long naxis = 3;
    uint32_t *naxes;


    naxes = (uint32_t *) malloc(sizeof(uint32_t) * 3);
    naxes[0] = xsize;
    naxes[1] = ysize;
    naxes[2] = zsize;

    if(data.precision == 0)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_COMPLEX_FLOAT,
                             data.SHARED_DFT, data.NBKEWORD_DFT); // single precision
    }

    if(data.precision == 1)
    {
        ID = create_image_ID(ID_name, naxis, naxes, _DATATYPE_COMPLEX_DOUBLE,
                             data.SHARED_DFT, data.NBKEWORD_DFT); // double precision
    }

    free(naxes);

    return ID;
}

















/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 5. CREATE VARIABLE                                                                              */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */





/* creates floating point variable */
variableID create_variable_ID(
    const char *name,
    double value
)
{
    variableID ID;
    long i1, i2;

    //printf("TEST   %s  %ld   %ld %ld ================== \n", __FILE__, __LINE__, data.NB_MAX_IMAGE, data.NB_MAX_VARIABLE);

    ID = -1;
    //printf("TEST   %s  %ld   %ld %ld ================== \n", __FILE__, __LINE__, data.NB_MAX_IMAGE, data.NB_MAX_VARIABLE);

    i1 = image_ID(name);
    //printf("TEST   %s  %ld   %ld %ld ================== \n", __FILE__, __LINE__, data.NB_MAX_IMAGE, data.NB_MAX_VARIABLE);


    i2 = variable_ID(name);
    //    printf("TEST   %s  %ld   %ld %ld ================== \n", __FILE__, __LINE__, data.NB_MAX_IMAGE, data.NB_MAX_VARIABLE);

    if(i1 != -1)
    {
        printf("ERROR: cannot create variable \"%s\": name already used as an image\n",
               name);
    }
    else
    {
        if(i2 != -1)
        {
            //	  printf("Warning : variable name \"%s\" is already in use\n",name);
            ID = i2;
        }
        else
        {
            ID = next_avail_variable_ID();
        }

        data.variable[ID].used = 1;
        data.variable[ID].type = 0; /** floating point double */
        strcpy(data.variable[ID].name, name);
        data.variable[ID].value.f = value;

    }
    //    printf("TEST   %s  %ld   %ld %ld ================== \n", __FILE__, __LINE__, data.NB_MAX_IMAGE, data.NB_MAX_VARIABLE);
    return ID;
}



/* creates long variable */
variableID create_variable_long_ID(
    const char *name,
    long value
)
{
    variableID ID;
    long i1, i2;

    ID = -1;
    i1 = image_ID(name);
    i2 = variable_ID(name);

    if(i1 != -1)
    {
        printf("ERROR: cannot create variable \"%s\": name already used as an image\n",
               name);
    }
    else
    {
        if(i2 != -1)
        {
            //	  printf("Warning : variable name \"%s\" is already in use\n",name);
            ID = i2;
        }
        else
        {
            ID = next_avail_variable_ID();
        }

        data.variable[ID].used = 1;
        data.variable[ID].type = 1; /** long */
        strcpy(data.variable[ID].name, name);
        data.variable[ID].value.l = value;

    }

    return ID;
}



/* creates long variable */
variableID create_variable_string_ID(
    const char *name,
    const char *value
)
{
    variableID ID;
    long i1, i2;

    ID = -1;
    i1 = image_ID(name);
    i2 = variable_ID(name);

    if(i1 != -1)
    {
        printf("ERROR: cannot create variable \"%s\": name already used as an image\n",
               name);
    }
    else
    {
        if(i2 != -1)
        {
            //	  printf("Warning : variable name \"%s\" is already in use\n",name);
            ID = i2;
        }
        else
        {
            ID = next_avail_variable_ID();
        }

        data.variable[ID].used = 1;
        data.variable[ID].type = 2; /** string */
        strcpy(data.variable[ID].name, name);
        strcpy(data.variable[ID].value.s, value);
    }

    return ID;
}

















/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 6. COPY IMAGE                                                                                   */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




imageID copy_image_ID(
    const char *name,
    const char *newname,
    int         shared
)
{
    imageID    ID;
    imageID    IDout;
    long       naxis;
    uint32_t  *size = NULL;
    uint8_t    datatype;
    long       nelement;
    long       i;
    int        newim = 0;


    ID = image_ID(name);
    if(ID == -1)
    {
        PRINT_ERROR("image \"%s\" does not exist", name);
        exit(0);
    }
    naxis = data.image[ID].md[0].naxis;

    size = (uint32_t *) malloc(sizeof(uint32_t) * naxis);
    if(size == NULL)
    {
        PRINT_ERROR("malloc error");
        exit(0);
    }

    for(i = 0; i < naxis; i++)
    {
        size[i] = data.image[ID].md[0].size[i];
    }
    datatype  = data.image[ID].md[0].datatype;

    nelement = data.image[ID].md[0].nelement;

    IDout = image_ID(newname);

    if(IDout != -1)
    {
        // verify newname has the right size and type
        if(data.image[ID].md[0].nelement != data.image[IDout].md[0].nelement)
        {
            fprintf(stderr,
                    "ERROR [copy_image_ID]: images %s and %s do not have the same size -> deleting and re-creating image\n",
                    name, newname);
            newim = 1;
        }
        if(data.image[ID].md[0].datatype != data.image[IDout].md[0].datatype)
        {
            fprintf(stderr,
                    "ERROR [copy_image_ID]: images %s and %s do not have the same type -> deleting and re-creating image\n",
                    name, newname);
            newim = 1;
        }

        if(newim == 1)
        {
            delete_image_ID(newname);
            IDout = -1;
        }
    }




    if(IDout == -1)
    {
        create_image_ID(newname, naxis, size, datatype, shared, data.NBKEWORD_DFT);
        IDout = image_ID(newname);
    }
    else
    {
        // verify newname has the right size and type
        if(data.image[ID].md[0].nelement != data.image[IDout].md[0].nelement)
        {
            fprintf(stderr,
                    "ERROR [copy_image_ID]: images %s and %s do not have the same size\n", name,
                    newname);
            exit(0);
        }
        if(data.image[ID].md[0].datatype != data.image[IDout].md[0].datatype)
        {
            fprintf(stderr,
                    "ERROR [copy_image_ID]: images %s and %s do not have the same type\n", name,
                    newname);
            exit(0);
        }
    }
    data.image[IDout].md[0].write = 1;


    if(datatype == _DATATYPE_UINT8)
    {
        memcpy(data.image[IDout].array.UI8, data.image[ID].array.UI8,
               SIZEOF_DATATYPE_UINT8 * nelement);
    }

    if(datatype == _DATATYPE_INT8)
    {
        memcpy(data.image[IDout].array.SI8, data.image[ID].array.SI8,
               SIZEOF_DATATYPE_INT8 * nelement);
    }

    if(datatype == _DATATYPE_UINT16)
    {
        memcpy(data.image[IDout].array.UI16, data.image[ID].array.UI16,
               SIZEOF_DATATYPE_UINT16 * nelement);
    }

    if(datatype == _DATATYPE_INT16)
    {
        memcpy(data.image[IDout].array.SI16, data.image[ID].array.SI16,
               SIZEOF_DATATYPE_INT8 * nelement);
    }

    if(datatype == _DATATYPE_UINT32)
    {
        memcpy(data.image[IDout].array.UI32, data.image[ID].array.UI32,
               SIZEOF_DATATYPE_UINT32 * nelement);
    }

    if(datatype == _DATATYPE_INT32)
    {
        memcpy(data.image[IDout].array.SI32, data.image[ID].array.SI32,
               SIZEOF_DATATYPE_INT32 * nelement);
    }

    if(datatype == _DATATYPE_UINT64)
    {
        memcpy(data.image[IDout].array.UI64, data.image[ID].array.UI64,
               SIZEOF_DATATYPE_UINT64 * nelement);
    }

    if(datatype == _DATATYPE_INT64)
    {
        memcpy(data.image[IDout].array.SI64, data.image[ID].array.SI64,
               SIZEOF_DATATYPE_INT64 * nelement);
    }


    if(datatype == _DATATYPE_FLOAT)
    {
        memcpy(data.image[IDout].array.F, data.image[ID].array.F,
               SIZEOF_DATATYPE_FLOAT * nelement);
    }

    if(datatype == _DATATYPE_DOUBLE)
    {
        memcpy(data.image[IDout].array.D, data.image[ID].array.D,
               SIZEOF_DATATYPE_DOUBLE * nelement);
    }

    if(datatype == _DATATYPE_COMPLEX_FLOAT)
    {
        memcpy(data.image[IDout].array.CF, data.image[ID].array.CF,
               SIZEOF_DATATYPE_COMPLEX_FLOAT * nelement);
    }

    if(datatype == _DATATYPE_COMPLEX_DOUBLE)
    {
        memcpy(data.image[IDout].array.CD, data.image[ID].array.CD,
               SIZEOF_DATATYPE_COMPLEX_DOUBLE * nelement);
    }

    COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);

    data.image[IDout].md[0].write = 0;
    data.image[IDout].md[0].cnt0++;

    free(size);


    return IDout;
}




imageID chname_image_ID(
    const char *ID_name,
    const char *new_name
)
{
    imageID ID;

    ID = -1;
    if((image_ID(new_name) == -1) && (variable_ID(new_name) == -1))
    {
        ID = image_ID(ID_name);
        strcpy(data.image[ID].name, new_name);
        //      if ( Debug > 0 ) { printf("change image name %s -> %s\n",ID_name,new_name);}
    }
    else
    {
        printf("Cannot change name %s -> %s : new name already in use\n", ID_name,
               new_name);
    }

    if(MEM_MONITOR == 1)
    {
        list_image_ID_ncurses();
    }

    return ID;
}






/** copy an image to shared memory
 *
 *
 */
errno_t COREMOD_MEMORY_cp2shm(
    const char *IDname,
    const char *IDshmname
)
{
    imageID    ID;
    imageID    IDshm;
    uint8_t    datatype;
    long       naxis;
    uint32_t  *sizearray;
    char      *ptr1;
    char      *ptr2;
    long       k;
    int        axis;
    int        shmOK;


    ID = image_ID(IDname);
    naxis = data.image[ID].md[0].naxis;


    sizearray = (uint32_t *) malloc(sizeof(uint32_t) * naxis);
    datatype = data.image[ID].md[0].datatype;
    for(k = 0; k < naxis; k++)
    {
        sizearray[k] = data.image[ID].md[0].size[k];
    }

    shmOK = 1;
    IDshm = read_sharedmem_image(IDshmname);
    if(IDshm != -1)
    {
        // verify type and size
        if(data.image[ID].md[0].naxis != data.image[IDshm].md[0].naxis)
        {
            shmOK = 0;
        }
        if(shmOK == 1)
        {
            for(axis = 0; axis < data.image[IDshm].md[0].naxis; axis++)
                if(data.image[ID].md[0].size[axis] != data.image[IDshm].md[0].size[axis])
                {
                    shmOK = 0;
                }
        }
        if(data.image[ID].md[0].datatype != data.image[IDshm].md[0].datatype)
        {
            shmOK = 0;
        }

        if(shmOK == 0)
        {
            delete_image_ID(IDshmname);
            IDshm = -1;
        }
    }

    if(IDshm == -1)
    {
        IDshm = create_image_ID(IDshmname, naxis, sizearray, datatype, 1, 0);
    }
    free(sizearray);

    //data.image[IDshm].md[0].nelement = data.image[ID].md[0].nelement;
    //printf("======= %ld %ld ============\n", data.image[ID].md[0].nelement, data.image[IDshm].md[0].nelement);

    data.image[IDshm].md[0].write = 1;

    switch(datatype)
    {

        case _DATATYPE_FLOAT :
            ptr1 = (char *) data.image[ID].array.F;
            ptr2 = (char *) data.image[IDshm].array.F;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_FLOAT * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_DOUBLE :
            ptr1 = (char *) data.image[ID].array.D;
            ptr2 = (char *) data.image[IDshm].array.D;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_DOUBLE * data.image[ID].md[0].nelement);
            break;


        case _DATATYPE_INT8 :
            ptr1 = (char *) data.image[ID].array.SI8;
            ptr2 = (char *) data.image[IDshm].array.SI8;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_INT8 * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_UINT8 :
            ptr1 = (char *) data.image[ID].array.UI8;
            ptr2 = (char *) data.image[IDshm].array.UI8;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_UINT8 * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_INT16 :
            ptr1 = (char *) data.image[ID].array.SI16;
            ptr2 = (char *) data.image[IDshm].array.SI16;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_INT16 * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_UINT16 :
            ptr1 = (char *) data.image[ID].array.UI16;
            ptr2 = (char *) data.image[IDshm].array.UI16;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_UINT16 * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_INT32 :
            ptr1 = (char *) data.image[ID].array.SI32;
            ptr2 = (char *) data.image[IDshm].array.SI32;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_INT32 * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_UINT32 :
            ptr1 = (char *) data.image[ID].array.UI32;
            ptr2 = (char *) data.image[IDshm].array.UI32;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_UINT32 * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_INT64 :
            ptr1 = (char *) data.image[ID].array.SI64;
            ptr2 = (char *) data.image[IDshm].array.SI64;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_INT64 * data.image[ID].md[0].nelement);
            break;

        case _DATATYPE_UINT64 :
            ptr1 = (char *) data.image[ID].array.UI64;
            ptr2 = (char *) data.image[IDshm].array.UI64;
            memcpy((void *) ptr2, (void *) ptr1,
                   SIZEOF_DATATYPE_UINT64 * data.image[ID].md[0].nelement);
            break;


        default :
            printf("data type not supported\n");
            break;
    }
    COREMOD_MEMORY_image_set_sempost_byID(IDshm, -1);
    data.image[IDshm].md[0].cnt0++;
    data.image[IDshm].md[0].write = 0;

    return RETURN_SUCCESS;
}










/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 7. DISPLAY / LISTS                                                                              */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */







errno_t init_list_image_ID_ncurses(
    const char *termttyname
)
{
//    int wrow, wcol;

    listim_scr_fpi = fopen(termttyname, "r");
    listim_scr_fpo = fopen(termttyname, "w");
    listim_scr = newterm(NULL, listim_scr_fpo, listim_scr_fpi);

    getmaxyx(stdscr, listim_scr_wrow, listim_scr_wcol);
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_BLACK, COLOR_RED);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_BLACK, COLOR_GREEN);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(8, COLOR_BLACK, COLOR_MAGENTA);
    init_pair(9, COLOR_YELLOW, COLOR_BLACK);

    return RETURN_SUCCESS;
}



errno_t list_image_ID_ncurses()
{
    char      str[300];
    char      str1[500];
    char      str2[512];
    long      i, j;
    long long tmp_long;
    char      type[STYPESIZE];
    uint8_t   datatype;
    int       n;
    long long sizeb, sizeKb, sizeMb, sizeGb;

    struct    timespec timenow;
    double    timediff;

    clock_gettime(CLOCK_REALTIME, &timenow);

    set_term(listim_scr);

    clear();


    sizeb = compute_image_memory();


    printw("INDEX    NAME         SIZE                    TYPE        SIZE  [percent]    LAST ACCESS\n");
    printw("\n");

    for(i = 0; i < data.NB_MAX_IMAGE; i++)
    {
        if(data.image[i].used == 1)
        {
            datatype = data.image[i].md[0].datatype;
            tmp_long = ((long long)(data.image[i].md[0].nelement)) * TYPESIZE[datatype];

            if(data.image[i].md[0].shared == 1)
            {
                printw("%4ldS", i);
            }
            else
            {
                printw("%4ld ", i);
            }

            if(data.image[i].md[0].shared == 1)
            {
                attron(A_BOLD | COLOR_PAIR(9));
            }
            else
            {
                attron(A_BOLD | COLOR_PAIR(6));
            }
            sprintf(str, "%10s ", data.image[i].name);
            printw(str);

            if(data.image[i].md[0].shared == 1)
            {
                attroff(A_BOLD | COLOR_PAIR(9));
            }
            else
            {
                attroff(A_BOLD | COLOR_PAIR(6));
            }

            sprintf(str, "[ %6ld", (long) data.image[i].md[0].size[0]);

            for(j = 1; j < data.image[i].md[0].naxis; j++)
            {
                sprintf(str1, "%s x %6ld", str, (long) data.image[i].md[0].size[j]);
            }
            sprintf(str2, "%s]", str1);

            printw("%-28s", str2);

            attron(COLOR_PAIR(3));
            n = 0;

            if(datatype == _DATATYPE_UINT8)
            {
                n = snprintf(type, STYPESIZE, "UINT8  ");
            }
            if(datatype == _DATATYPE_INT8)
            {
                n = snprintf(type, STYPESIZE, "INT8   ");
            }
            if(datatype == _DATATYPE_UINT16)
            {
                n = snprintf(type, STYPESIZE, "UINT16 ");
            }
            if(datatype == _DATATYPE_INT16)
            {
                n = snprintf(type, STYPESIZE, "INT16  ");
            }
            if(datatype == _DATATYPE_UINT32)
            {
                n = snprintf(type, STYPESIZE, "UINT32 ");
            }
            if(datatype == _DATATYPE_INT32)
            {
                n = snprintf(type, STYPESIZE, "INT32  ");
            }
            if(datatype == _DATATYPE_UINT64)
            {
                n = snprintf(type, STYPESIZE, "UINT64 ");
            }
            if(datatype == _DATATYPE_INT64)
            {
                n = snprintf(type, STYPESIZE, "INT64  ");
            }
            if(datatype == _DATATYPE_FLOAT)
            {
                n = snprintf(type, STYPESIZE, "FLOAT  ");
            }
            if(datatype == _DATATYPE_DOUBLE)
            {
                n = snprintf(type, STYPESIZE, "DOUBLE ");
            }
            if(datatype == _DATATYPE_COMPLEX_FLOAT)
            {
                n = snprintf(type, STYPESIZE, "CFLOAT ");
            }
            if(datatype == _DATATYPE_COMPLEX_DOUBLE)
            {
                n = snprintf(type, STYPESIZE, "CDOUBLE");
            }

            printw("%7s ", type);

            attroff(COLOR_PAIR(3));

            if(n >= STYPESIZE) {
                PRINT_ERROR("Attempted to write string buffer with too many characters");
			}

            printw("%10ld Kb %6.2f   ", (long)(tmp_long / 1024),
                   (float)(100.0 * tmp_long / sizeb));

            timediff = (1.0 * timenow.tv_sec + 0.000000001 * timenow.tv_nsec) -
                       (1.0 * data.image[i].md[0].lastaccesstime.tv_sec + 0.000000001 *
                        data.image[i].md[0].lastaccesstime.tv_nsec);

            if(timediff < 0.01)
            {
                attron(COLOR_PAIR(4));
                printw("%15.9f\n", timediff);
                attroff(COLOR_PAIR(4));
            }
            else
            {
                printw("%15.9f\n", timediff);
            }
        }
        else
        {
            printw("\n");
        }
    }

    sizeGb = 0;
    sizeMb = 0;
    sizeKb = 0;
    sizeb = compute_image_memory();

    if(sizeb > 1024 - 1)
    {
        sizeKb = sizeb / 1024;
        sizeb = sizeb - 1024 * sizeKb;
    }
    if(sizeKb > 1024 - 1)
    {
        sizeMb = sizeKb / 1024;
        sizeKb = sizeKb - 1024 * sizeMb;
    }
    if(sizeMb > 1024 - 1)
    {
        sizeGb = sizeMb / 1024;
        sizeMb = sizeMb - 1024 * sizeGb;
    }

    //attron(A_BOLD);

    sprintf(str, "%ld image(s)      ", compute_nb_image());
    if(sizeGb > 0)
    {
        sprintf(str1, "%s %ld GB", str, (long)(sizeGb));
        strcpy(str, str1);
    }

    if(sizeMb > 0)
    {
        sprintf(str1, "%s %ld MB", str, (long)(sizeMb));
        strcpy(str, str1);
    }

    if(sizeKb > 0)
    {
        sprintf(str1, "%s %ld KB", str, (long)(sizeKb));
        strcpy(str, str1);
    }

    if(sizeb > 0)
    {
        sprintf(str1, "%s %ld B", str, (long)(sizeb));
        strcpy(str, str1);
    }

    mvprintw(listim_scr_wrow - 1, 0, "%s\n", str);
    //  attroff(A_BOLD);

    refresh();


    return RETURN_SUCCESS;
}




void close_list_image_ID_ncurses(void)
{
    printf("Closing monitor cleanly\n");
    set_term(listim_scr);
    endwin();
    fclose(listim_scr_fpo);
    fclose(listim_scr_fpi);
    MEM_MONITOR = 0;
}







errno_t list_image_ID_ofp(
    FILE *fo
)
{
    long        i;
    long        j;
    long long   tmp_long;
    char        type[STYPESIZE];
    uint8_t     datatype;
    int         n;
    unsigned long long sizeb, sizeKb, sizeMb, sizeGb;
    char        str[500];
    char        str1[512];
    struct      timespec timenow;
    double      timediff;
    //struct mallinfo minfo;

    sizeb = compute_image_memory();
    //minfo = mallinfo();

    clock_gettime(CLOCK_REALTIME, &timenow);
    //fprintf(fo, "time:  %ld.%09ld\n", timenow.tv_sec % 60, timenow.tv_nsec);



    fprintf(fo, "\n");
    fprintf(fo,
            "INDEX    NAME         SIZE                    TYPE        SIZE  [percent]    LAST ACCESS\n");
    fprintf(fo, "\n");

    for(i = 0; i < data.NB_MAX_IMAGE; i++)
        if(data.image[i].used == 1)
        {
            datatype = data.image[i].md[0].datatype;
            tmp_long = ((long long)(data.image[i].md[0].nelement)) * TYPESIZE[datatype];

            if(data.image[i].md[0].shared == 1)
            {
                fprintf(fo, "%4ld %c[%d;%dm%14s%c[%d;m ", i, (char) 27, 1, 34,
                        data.image[i].name, (char) 27, 0);
            }
            else
            {
                fprintf(fo, "%4ld %c[%d;%dm%14s%c[%d;m ", i, (char) 27, 1, 33,
                        data.image[i].name, (char) 27, 0);
            }
            //fprintf(fo, "%s", str);

            sprintf(str, "[ %6ld", (long) data.image[i].md[0].size[0]);

            for(j = 1; j < data.image[i].md[0].naxis; j++)
            {
                sprintf(str1, "%s x %6ld", str, (long) data.image[i].md[0].size[j]);
                strcpy(str, str1);
            }
            sprintf(str1, "%s]", str);
            strcpy(str, str1);

            fprintf(fo, "%-32s", str);


            n = 0;
            if(datatype == _DATATYPE_UINT8)
            {
                n = snprintf(type, STYPESIZE, "UINT8  ");
            }
            if(datatype == _DATATYPE_INT8)
            {
                n = snprintf(type, STYPESIZE, "INT8   ");
            }
            if(datatype == _DATATYPE_UINT16)
            {
                n = snprintf(type, STYPESIZE, "UINT16 ");
            }
            if(datatype == _DATATYPE_INT16)
            {
                n = snprintf(type, STYPESIZE, "INT16  ");
            }
            if(datatype == _DATATYPE_UINT32)
            {
                n = snprintf(type, STYPESIZE, "UINT32 ");
            }
            if(datatype == _DATATYPE_INT32)
            {
                n = snprintf(type, STYPESIZE, "INT32  ");
            }
            if(datatype == _DATATYPE_UINT64)
            {
                n = snprintf(type, STYPESIZE, "UINT64 ");
            }
            if(datatype == _DATATYPE_INT64)
            {
                n = snprintf(type, STYPESIZE, "INT64  ");
            }
            if(datatype == _DATATYPE_FLOAT)
            {
                n = snprintf(type, STYPESIZE, "FLOAT  ");
            }
            if(datatype == _DATATYPE_DOUBLE)
            {
                n = snprintf(type, STYPESIZE, "DOUBLE ");
            }
            if(datatype == _DATATYPE_COMPLEX_FLOAT)
            {
                n = snprintf(type, STYPESIZE, "CFLOAT ");
            }
            if(datatype == _DATATYPE_COMPLEX_DOUBLE)
            {
                n = snprintf(type, STYPESIZE, "CDOUBLE");
            }

            fprintf(fo, "%7s ", type);


            if(n >= STYPESIZE)
            {
                PRINT_ERROR("Attempted to write string buffer with too many characters");
            }

            fprintf(fo, "%10ld Kb %6.2f   ", (long)(tmp_long / 1024),
                    (float)(100.0 * tmp_long / sizeb));

            timediff = (1.0 * timenow.tv_sec + 0.000000001 * timenow.tv_nsec) -
                       (1.0 * data.image[i].md[0].lastaccesstime.tv_sec + 0.000000001 *
                        data.image[i].md[0].lastaccesstime.tv_nsec);

            fprintf(fo, "%15.9f\n", timediff);
        }
    fprintf(fo, "\n");


    sizeGb = 0;
    sizeMb = 0;
    sizeKb = 0;
    sizeb = compute_image_memory();

    if(sizeb > 1024 - 1)
    {
        sizeKb = sizeb / 1024;
        sizeb = sizeb - 1024 * sizeKb;
    }
    if(sizeKb > 1024 - 1)
    {
        sizeMb = sizeKb / 1024;
        sizeKb = sizeKb - 1024 * sizeMb;
    }
    if(sizeMb > 1024 - 1)
    {
        sizeGb = sizeMb / 1024;
        sizeMb = sizeMb - 1024 * sizeGb;
    }

    fprintf(fo, "%ld image(s)   ", compute_nb_image());
    if(sizeGb > 0)
    {
        fprintf(fo, " %ld Gb", (long)(sizeGb));
    }
    if(sizeMb > 0)
    {
        fprintf(fo, " %ld Mb", (long)(sizeMb));
    }
    if(sizeKb > 0)
    {
        fprintf(fo, " %ld Kb", (long)(sizeKb));
    }
    if(sizeb > 0)
    {
        fprintf(fo, " %ld", (long)(sizeb));
    }
    fprintf(fo, "\n");

    fflush(fo);


    return RETURN_SUCCESS;
}




errno_t list_image_ID_ofp_simple(
    FILE *fo
)
{
    long        i, j;
    //long long   tmp_long;
    uint8_t     datatype;

    for(i = 0; i < data.NB_MAX_IMAGE; i++)
        if(data.image[i].used == 1)
        {
            datatype = data.image[i].md[0].datatype;
            //tmp_long = ((long long) (data.image[i].md[0].nelement)) * TYPESIZE[datatype];

            fprintf(fo, "%20s %d %ld %d %4ld", data.image[i].name, datatype,
                    (long) data.image[i].md[0].naxis, data.image[i].md[0].shared,
                    (long) data.image[i].md[0].size[0]);

            for(j = 1; j < data.image[i].md[0].naxis; j++)
            {
                fprintf(fo, " %4ld", (long) data.image[i].md[0].size[j]);
            }
            fprintf(fo, "\n");
        }
    fprintf(fo, "\n");

    return RETURN_SUCCESS;
}




errno_t list_image_ID()
{
    list_image_ID_ofp(stdout);
    //malloc_stats();
    return RETURN_SUCCESS;
}



/* list all images in memory
   output is written in ASCII file
   only basic info is listed
   image name
   number of axis
   size
   type
 */

errno_t list_image_ID_file(
    const char *fname
)
{
    FILE *fp;
    long i, j;
    uint8_t datatype;
    char type[STYPESIZE];
    int n;

    fp = fopen(fname, "w");
    if(fp == NULL)
    {
        PRINT_ERROR("Cannot create file %s", fname);
        abort();
    }

    for(i = 0; i < data.NB_MAX_IMAGE; i++)
        if(data.image[i].used == 1)
        {
            datatype = data.image[i].md[0].datatype;
            fprintf(fp, "%ld %s", i, data.image[i].name);
            fprintf(fp, " %ld", (long) data.image[i].md[0].naxis);
            for(j = 0; j < data.image[i].md[0].naxis; j++)
            {
                fprintf(fp, " %ld", (long) data.image[i].md[0].size[j]);
            }

            n = 0;

            if(datatype == _DATATYPE_UINT8)
            {
                n = snprintf(type, STYPESIZE, "UINT8  ");
            }
            if(datatype == _DATATYPE_INT8)
            {
                n = snprintf(type, STYPESIZE, "INT8   ");
            }
            if(datatype == _DATATYPE_UINT16)
            {
                n = snprintf(type, STYPESIZE, "UINT16 ");
            }
            if(datatype == _DATATYPE_INT16)
            {
                n = snprintf(type, STYPESIZE, "INT16  ");
            }
            if(datatype == _DATATYPE_UINT32)
            {
                n = snprintf(type, STYPESIZE, "UINT32 ");
            }
            if(datatype == _DATATYPE_INT32)
            {
                n = snprintf(type, STYPESIZE, "INT32  ");
            }
            if(datatype == _DATATYPE_UINT64)
            {
                n = snprintf(type, STYPESIZE, "UINT64 ");
            }
            if(datatype == _DATATYPE_INT64)
            {
                n = snprintf(type, STYPESIZE, "INT64  ");
            }
            if(datatype == _DATATYPE_FLOAT)
            {
                n = snprintf(type, STYPESIZE, "FLOAT  ");
            }
            if(datatype == _DATATYPE_DOUBLE)
            {
                n = snprintf(type, STYPESIZE, "DOUBLE ");
            }
            if(datatype == _DATATYPE_COMPLEX_FLOAT)
            {
                n = snprintf(type, STYPESIZE, "CFLOAT ");
            }
            if(datatype == _DATATYPE_COMPLEX_DOUBLE)
            {
                n = snprintf(type, STYPESIZE, "CDOUBLE");
            }


            if(n >= STYPESIZE)
            {
                PRINT_ERROR("Attempted to write string buffer with too many characters");
            }

            fprintf(fp, " %s\n", type);
        }
    fclose(fp);

    return RETURN_SUCCESS;
}


errno_t list_variable_ID()
{
    variableID i;

    for(i = 0; i < data.NB_MAX_VARIABLE; i++)
        if(data.variable[i].used == 1)
        {
            printf("%4ld %16s %25.18g\n", i, data.variable[i].name,
                   data.variable[i].value.f);
        }

    return RETURN_SUCCESS;
}


errno_t list_variable_ID_file(const char *fname)
{
    imageID i;
    FILE *fp;

    fp = fopen(fname, "w");
    for(i = 0; i < data.NB_MAX_VARIABLE; i++)
        if(data.variable[i].used == 1)
        {
            fprintf(fp, "%s=%.18g\n", data.variable[i].name, data.variable[i].value.f);
        }

    fclose(fp);

    return RETURN_SUCCESS;
}








/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 8. TYPE CONVERSIONS TO AND FROM COMPLEX                                                         */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




errno_t mk_complex_from_reim(
    const char *re_name,
    const char *im_name,
    const char *out_name,
    int         sharedmem
)
{
    imageID     IDre;
    imageID     IDim;
    imageID     IDout;
    uint32_t   *naxes = NULL;
    long        naxis;
    long        nelement;
    long        ii;
    long        i;
    uint8_t     datatype_re;
    uint8_t     datatype_im;
    uint8_t     datatype_out;

    IDre = image_ID(re_name);
    IDim = image_ID(im_name);

    datatype_re = data.image[IDre].md[0].datatype;
    datatype_im = data.image[IDim].md[0].datatype;
    naxis = data.image[IDre].md[0].naxis;

    naxes = (uint32_t *) malloc(sizeof(uint32_t) * naxis);
    if(naxes == NULL)
    {
        PRINT_ERROR("malloc error");
        exit(0);
    }

    for(i = 0; i < naxis; i++)
    {
        naxes[i] = data.image[IDre].md[0].size[i];
    }
    nelement = data.image[IDre].md[0].nelement;


    if((datatype_re == _DATATYPE_FLOAT) && (datatype_im == _DATATYPE_FLOAT))
    {
        datatype_out = _DATATYPE_COMPLEX_FLOAT;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);
        for(ii = 0; ii < nelement; ii++)
        {
            data.image[IDout].array.CF[ii].re = data.image[IDre].array.F[ii];
            data.image[IDout].array.CF[ii].im = data.image[IDim].array.F[ii];
        }
    }
    else if((datatype_re == _DATATYPE_FLOAT) && (datatype_im == _DATATYPE_DOUBLE))
    {
        datatype_out = _DATATYPE_COMPLEX_DOUBLE;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);
        for(ii = 0; ii < nelement; ii++)
        {
            data.image[IDout].array.CD[ii].re = data.image[IDre].array.F[ii];
            data.image[IDout].array.CD[ii].im = data.image[IDim].array.D[ii];
        }
    }
    else if((datatype_re == _DATATYPE_DOUBLE) && (datatype_im == _DATATYPE_FLOAT))
    {
        datatype_out = _DATATYPE_COMPLEX_DOUBLE;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);
        for(ii = 0; ii < nelement; ii++)
        {
            data.image[IDout].array.CD[ii].re = data.image[IDre].array.D[ii];
            data.image[IDout].array.CD[ii].im = data.image[IDim].array.F[ii];
        }
    }
    else if((datatype_re == _DATATYPE_DOUBLE) && (datatype_im == _DATATYPE_DOUBLE))
    {
        datatype_out = _DATATYPE_COMPLEX_DOUBLE;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);
        for(ii = 0; ii < nelement; ii++)
        {
            data.image[IDout].array.CD[ii].re = data.image[IDre].array.D[ii];
            data.image[IDout].array.CD[ii].im = data.image[IDim].array.D[ii];
        }
    }
    else
    {
        PRINT_ERROR("Wrong image type(s)\n");
        exit(0);
    }
    // Note: openMP doesn't help here

    free(naxes);

    return RETURN_SUCCESS;
}





errno_t mk_complex_from_amph(
    const char *am_name,
    const char *ph_name,
    const char *out_name,
    int         sharedmem
)
{
    imageID    IDam;
    imageID    IDph;
    imageID    IDout;
    uint32_t   naxes[3];
    long       naxis;
    uint64_t   nelement;
    long       i;
    uint8_t    datatype_am;
    uint8_t    datatype_ph;
    uint8_t    datatype_out;

    IDam = image_ID(am_name);
    IDph = image_ID(ph_name);
    datatype_am = data.image[IDam].md[0].datatype;
    datatype_ph = data.image[IDph].md[0].datatype;

    naxis = data.image[IDam].md[0].naxis;
    for(i = 0; i < naxis; i++)
    {
        naxes[i] = data.image[IDam].md[0].size[i];
    }
    nelement = data.image[IDam].md[0].nelement;

    if((datatype_am == _DATATYPE_FLOAT) && (datatype_ph == _DATATYPE_FLOAT))
    {
        datatype_out = _DATATYPE_COMPLEX_FLOAT;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);

        data.image[IDout].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT)
        {
            #pragma omp for
# endif
            for(uint64_t ii = 0; ii < nelement; ii++)
            {
                data.image[IDout].array.CF[ii].re = data.image[IDam].array.F[ii] * ((float) cos(
                                                        data.image[IDph].array.F[ii]));
                data.image[IDout].array.CF[ii].im = data.image[IDam].array.F[ii] * ((float) sin(
                                                        data.image[IDph].array.F[ii]));
            }
# ifdef _OPENMP
        }
# endif
        data.image[IDout].md[0].cnt0++;
        data.image[IDout].md[0].write = 0;

    }
    else if((datatype_am == _DATATYPE_FLOAT) && (datatype_ph == _DATATYPE_DOUBLE))
    {
        datatype_out = _DATATYPE_COMPLEX_DOUBLE;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);
        data.image[IDout].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT)
        {
            #pragma omp for
# endif
            for(uint64_t ii = 0; ii < nelement; ii++)
            {
                data.image[IDout].array.CD[ii].re = data.image[IDam].array.F[ii] * cos(
                                                        data.image[IDph].array.D[ii]);
                data.image[IDout].array.CD[ii].im = data.image[IDam].array.F[ii] * sin(
                                                        data.image[IDph].array.D[ii]);
            }
# ifdef _OPENMP
        }
# endif
        data.image[IDout].md[0].cnt0++;
        data.image[IDout].md[0].write = 0;
    }
    else if((datatype_am == _DATATYPE_DOUBLE) && (datatype_ph == _DATATYPE_FLOAT))
    {
        datatype_out = _DATATYPE_COMPLEX_DOUBLE;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);
        data.image[IDout].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT)
        {
            #pragma omp for
# endif
            for(uint64_t ii = 0; ii < nelement; ii++)
            {
                data.image[IDout].array.CD[ii].re = data.image[IDam].array.D[ii] * cos(
                                                        data.image[IDph].array.F[ii]);
                data.image[IDout].array.CD[ii].im = data.image[IDam].array.D[ii] * sin(
                                                        data.image[IDph].array.F[ii]);
            }
# ifdef _OPENMP
        }
# endif
        data.image[IDout].md[0].cnt0++;
        data.image[IDout].md[0].write = 0;

    }
    else if((datatype_am == _DATATYPE_DOUBLE) && (datatype_ph == _DATATYPE_DOUBLE))
    {
        datatype_out = _DATATYPE_COMPLEX_DOUBLE;
        IDout = create_image_ID(out_name, naxis, naxes, datatype_out, sharedmem,
                                data.NBKEWORD_DFT);
        data.image[IDout].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT)
        {
            #pragma omp for
# endif
            for(uint64_t ii = 0; ii < nelement; ii++)
            {
                data.image[IDout].array.CD[ii].re = data.image[IDam].array.D[ii] * cos(
                                                        data.image[IDph].array.D[ii]);
                data.image[IDout].array.CD[ii].im = data.image[IDam].array.D[ii] * sin(
                                                        data.image[IDph].array.D[ii]);
            }
# ifdef _OPENMP
        }
# endif
        data.image[IDout].md[0].cnt0++;
        data.image[IDout].md[0].write = 0;
    }
    else
    {
        PRINT_ERROR("Wrong image type(s)\n");
        exit(0);
    }

    return RETURN_SUCCESS;
}




errno_t mk_reim_from_complex(
    const char *in_name,
    const char *re_name,
    const char *im_name,
    int         sharedmem
)
{
    imageID     IDre;
    imageID     IDim;
    imageID     IDin;
    uint32_t    naxes[3];
    long        naxis;
    uint64_t        nelement;
    long        i;
    uint8_t     datatype;

    IDin = image_ID(in_name);
    datatype = data.image[IDin].md[0].datatype;
    naxis = data.image[IDin].md[0].naxis;
    for(i = 0; i < naxis; i++)
    {
        naxes[i] = data.image[IDin].md[0].size[i];
    }
    nelement = data.image[IDin].md[0].nelement;

    if(datatype == _DATATYPE_COMPLEX_FLOAT) // single precision
    {
        IDre = create_image_ID(re_name, naxis, naxes, _DATATYPE_FLOAT, sharedmem,
                               data.NBKEWORD_DFT);
        IDim = create_image_ID(im_name, naxis, naxes, _DATATYPE_FLOAT, sharedmem,
                               data.NBKEWORD_DFT);

        data.image[IDre].md[0].write = 1;
        data.image[IDim].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT)
        {
            #pragma omp for
# endif
            for(uint64_t ii = 0; ii < nelement; ii++)
            {
                data.image[IDre].array.F[ii] = data.image[IDin].array.CF[ii].re;
                data.image[IDim].array.F[ii] = data.image[IDin].array.CF[ii].im;
            }
# ifdef _OPENMP
        }
# endif
        if(sharedmem == 1)
        {
            COREMOD_MEMORY_image_set_sempost_byID(IDre, -1);
            COREMOD_MEMORY_image_set_sempost_byID(IDim, -1);
        }
        data.image[IDre].md[0].cnt0++;
        data.image[IDim].md[0].cnt0++;
        data.image[IDre].md[0].write = 0;
        data.image[IDim].md[0].write = 0;
    }
    else if(datatype == _DATATYPE_COMPLEX_DOUBLE) // double precision
    {
        IDre = create_image_ID(re_name, naxis, naxes, _DATATYPE_DOUBLE, sharedmem,
                               data.NBKEWORD_DFT);
        IDim = create_image_ID(im_name, naxis, naxes, _DATATYPE_DOUBLE, sharedmem,
                               data.NBKEWORD_DFT);
        data.image[IDre].md[0].write = 1;
        data.image[IDim].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT)
        {
            #pragma omp for
# endif
            for(uint64_t ii = 0; ii < nelement; ii++)
            {
                data.image[IDre].array.D[ii] = data.image[IDin].array.CD[ii].re;
                data.image[IDim].array.D[ii] = data.image[IDin].array.CD[ii].im;
            }
# ifdef _OPENMP
        }
# endif
        if(sharedmem == 1)
        {
            COREMOD_MEMORY_image_set_sempost_byID(IDre, -1);
            COREMOD_MEMORY_image_set_sempost_byID(IDim, -1);
        }
        data.image[IDre].md[0].cnt0++;
        data.image[IDim].md[0].cnt0++;
        data.image[IDre].md[0].write = 0;
        data.image[IDim].md[0].write = 0;

    }
    else
    {
        PRINT_ERROR("Wrong image type(s)\n");
        exit(0);
    }


    return RETURN_SUCCESS;
}




errno_t mk_amph_from_complex(
    const char *in_name,
    const char *am_name,
    const char *ph_name,
    int         sharedmem
)
{
    imageID    IDam;
    imageID    IDph;
    imageID    IDin;
    uint32_t   naxes[3];
    long       naxis;
    uint64_t       nelement;
    uint64_t   ii;
    long       i;
    float      amp_f;
    float      pha_f;
    double     amp_d;
    double     pha_d;
    uint8_t    datatype;

    IDin = image_ID(in_name);
    datatype = data.image[IDin].md[0].datatype;
    naxis = data.image[IDin].md[0].naxis;

    for(i = 0; i < naxis; i++)
    {
        naxes[i] = data.image[IDin].md[0].size[i];
    }
    nelement = data.image[IDin].md[0].nelement;

    if(datatype == _DATATYPE_COMPLEX_FLOAT) // single precision
    {
        IDam = create_image_ID(am_name, naxis, naxes,  _DATATYPE_FLOAT, sharedmem,
                               data.NBKEWORD_DFT);
        IDph = create_image_ID(ph_name, naxis, naxes,  _DATATYPE_FLOAT, sharedmem,
                               data.NBKEWORD_DFT);
        data.image[IDam].md[0].write = 1;
        data.image[IDph].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT) private(ii,amp_f,pha_f)
        {
            #pragma omp for
# endif
            for(ii = 0; ii < nelement; ii++)
            {
                amp_f = (float) sqrt(data.image[IDin].array.CF[ii].re *
                                     data.image[IDin].array.CF[ii].re + data.image[IDin].array.CF[ii].im *
                                     data.image[IDin].array.CF[ii].im);
                pha_f = (float) atan2(data.image[IDin].array.CF[ii].im,
                                      data.image[IDin].array.CF[ii].re);
                data.image[IDam].array.F[ii] = amp_f;
                data.image[IDph].array.F[ii] = pha_f;
            }
# ifdef _OPENMP
        }
# endif
        if(sharedmem == 1)
        {
            COREMOD_MEMORY_image_set_sempost_byID(IDam, -1);
            COREMOD_MEMORY_image_set_sempost_byID(IDph, -1);
        }
        data.image[IDam].md[0].cnt0++;
        data.image[IDph].md[0].cnt0++;
        data.image[IDam].md[0].write = 0;
        data.image[IDph].md[0].write = 0;
    }
    else if(datatype == _DATATYPE_COMPLEX_DOUBLE) // double precision
    {
        IDam = create_image_ID(am_name, naxis, naxes, _DATATYPE_DOUBLE, sharedmem,
                               data.NBKEWORD_DFT);
        IDph = create_image_ID(ph_name, naxis, naxes, _DATATYPE_DOUBLE, sharedmem,
                               data.NBKEWORD_DFT);
        data.image[IDam].md[0].write = 1;
        data.image[IDph].md[0].write = 1;
# ifdef _OPENMP
        #pragma omp parallel if (nelement>OMP_NELEMENT_LIMIT) private(ii,amp_d,pha_d)
        {
            #pragma omp for
# endif
            for(ii = 0; ii < nelement; ii++)
            {
                amp_d = sqrt(data.image[IDin].array.CD[ii].re * data.image[IDin].array.CD[ii].re
                             + data.image[IDin].array.CD[ii].im * data.image[IDin].array.CD[ii].im);
                pha_d = atan2(data.image[IDin].array.CD[ii].im,
                              data.image[IDin].array.CD[ii].re);
                data.image[IDam].array.D[ii] = amp_d;
                data.image[IDph].array.D[ii] = pha_d;
            }
# ifdef _OPENMP
        }
# endif
        if(sharedmem == 1)
        {
            COREMOD_MEMORY_image_set_sempost_byID(IDam, -1);
            COREMOD_MEMORY_image_set_sempost_byID(IDph, -1);
        }
        data.image[IDam].md[0].cnt0++;
        data.image[IDph].md[0].cnt0++;
        data.image[IDam].md[0].write = 0;
        data.image[IDph].md[0].write = 0;
    }
    else
    {
        PRINT_ERROR("Wrong image type(s)\n");
        exit(0);
    }

    return RETURN_SUCCESS;
}




errno_t mk_reim_from_amph(
    const char *am_name,
    const char *ph_name,
    const char *re_out_name,
    const char *im_out_name,
    int         sharedmem
)
{
    mk_complex_from_amph(am_name, ph_name, "Ctmp", 0);
    mk_reim_from_complex("Ctmp", re_out_name, im_out_name, sharedmem);
    delete_image_ID("Ctmp");

    return RETURN_SUCCESS;
}



errno_t mk_amph_from_reim(
    const char *re_name,
    const char *im_name,
    const char *am_out_name,
    const char *ph_out_name,
    int         sharedmem
)
{
    mk_complex_from_reim(re_name, im_name, "Ctmp", 0);
    mk_amph_from_complex("Ctmp", am_out_name, ph_out_name, sharedmem);
    delete_image_ID("Ctmp");

    return RETURN_SUCCESS;
}
















/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 9. VERIFY SIZE                                                                                  */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */






//  check only is size > 0
int check_2Dsize(
    const char *ID_name,
    uint32_t    xsize,
    uint32_t    ysize
)
{
    int      retval;
    imageID  ID;

    retval = 1;
    ID = image_ID(ID_name);
    if(data.image[ID].md[0].naxis != 2)
    {
        retval = 0;
    }
    if(retval == 1)
    {
        if((xsize > 0) && (data.image[ID].md[0].size[0] != xsize))
        {
            retval = 0;
        }
        if((ysize > 0) && (data.image[ID].md[0].size[1] != ysize))
        {
            retval = 0;
        }
    }

    return retval;
}



int check_3Dsize(
    const char *ID_name,
    uint32_t    xsize,
    uint32_t    ysize,
    uint32_t    zsize
)
{
    int     retval;
    imageID ID;

    retval = 1;
    ID = image_ID(ID_name);
    if(data.image[ID].md[0].naxis != 3)
    {
        /*      printf("Wrong naxis : %ld - should be 3\n",data.image[ID].md[0].naxis);*/
        retval = 0;
    }
    if(retval == 1)
    {
        if((xsize > 0) && (data.image[ID].md[0].size[0] != xsize))
        {
            /*	  printf("Wrong xsize : %ld - should be %ld\n",data.image[ID].md[0].size[0],xsize);*/
            retval = 0;
        }
        if((ysize > 0) && (data.image[ID].md[0].size[1] != ysize))
        {
            /*	  printf("Wrong ysize : %ld - should be %ld\n",data.image[ID].md[0].size[1],ysize);*/
            retval = 0;
        }
        if((zsize > 0) && (data.image[ID].md[0].size[2] != zsize))
        {
            /*	  printf("Wrong zsize : %ld - should be %ld\n",data.image[ID].md[0].size[2],zsize);*/
            retval = 0;
        }
    }
    /*  printf("CHECK = %d\n",value);*/

    return retval;
}







int COREMOD_MEMORY_check_2Dsize(
    const char *IDname,
    uint32_t    xsize,
    uint32_t    ysize
)
{
    int     sizeOK = 1; // 1 if size matches
    imageID ID;


    ID = image_ID(IDname);
    if(data.image[ID].md[0].naxis != 2)
    {
        printf("WARNING : image %s naxis = %d does not match expected value 2\n",
               IDname, (int) data.image[ID].md[0].naxis);
        sizeOK = 0;
    }
    if((xsize > 0) && (data.image[ID].md[0].size[0] != xsize))
    {
        printf("WARNING : image %s xsize = %d does not match expected value %d\n",
               IDname, (int) data.image[ID].md[0].size[0], (int) xsize);
        sizeOK = 0;
    }
    if((ysize > 0) && (data.image[ID].md[0].size[1] != ysize))
    {
        printf("WARNING : image %s ysize = %d does not match expected value %d\n",
               IDname, (int) data.image[ID].md[0].size[1], (int) ysize);
        sizeOK = 0;
    }

    return sizeOK;
}



int COREMOD_MEMORY_check_3Dsize(
    const char *IDname,
    uint32_t    xsize,
    uint32_t    ysize,
    uint32_t    zsize
)
{
    int     sizeOK = 1; // 1 if size matches
    imageID ID;

    ID = image_ID(IDname);
    if(data.image[ID].md[0].naxis != 3)
    {
        printf("WARNING : image %s naxis = %d does not match expected value 3\n",
               IDname, (int) data.image[ID].md[0].naxis);
        sizeOK = 0;
    }
    if((xsize > 0) && (data.image[ID].md[0].size[0] != xsize))
    {
        printf("WARNING : image %s xsize = %d does not match expected value %d\n",
               IDname, (int) data.image[ID].md[0].size[0], (int) xsize);
        sizeOK = 0;
    }
    if((ysize > 0) && (data.image[ID].md[0].size[1] != ysize))
    {
        printf("WARNING : image %s ysize = %d does not match expected value %d\n",
               IDname, (int) data.image[ID].md[0].size[1], (int) ysize);
        sizeOK = 0;
    }
    if((zsize > 0) && (data.image[ID].md[0].size[2] != zsize))
    {
        printf("WARNING : image %s zsize = %d does not match expected value %d\n",
               IDname, (int) data.image[ID].md[0].size[2], (int) zsize);
        sizeOK = 0;
    }

    return sizeOK;
}





/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 10. COORDINATE CHANGE                                                                           */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */




errno_t rotate_cube(
    const char *ID_name,
    const char *ID_out_name,
    int         orientation
)
{
    /* 0 is from x axis */
    /* 1 is from y axis */
    imageID     ID;
    imageID     IDout;
    uint32_t    xsize, ysize, zsize;
    uint32_t    xsize1, ysize1, zsize1;
    uint32_t    ii, jj, kk;
    uint8_t     datatype;

    ID = image_ID(ID_name);
    datatype = data.image[ID].md[0].datatype;

    if(data.image[ID].md[0].naxis != 3)
    {
        PRINT_ERROR("Wrong naxis : %d - should be 3\n", (int) data.image[ID].md[0].naxis);
        exit(0);
    }
    xsize = data.image[ID].md[0].size[0];
    ysize = data.image[ID].md[0].size[1];
    zsize = data.image[ID].md[0].size[2];

    if(datatype == _DATATYPE_FLOAT) // single precision
    {
        if(orientation == 0)
        {
            xsize1 = zsize;
            ysize1 = ysize;
            zsize1 = xsize;
            IDout = create_3Dimage_ID_float(ID_out_name, xsize1, ysize1, zsize1);
            for(ii = 0; ii < xsize1; ii++)
                for(jj = 0; jj < ysize1; jj++)
                    for(kk = 0; kk < zsize1; kk++)
                    {
                        data.image[IDout].array.F[kk * ysize1 * xsize1 + jj * xsize1 + ii] =
                            data.image[ID].array.F[ii * xsize * ysize + jj * xsize + kk];
                    }
        }
        else
        {
            xsize1 = xsize;
            ysize1 = zsize;
            zsize1 = ysize;
            IDout = create_3Dimage_ID_float(ID_out_name, xsize1, ysize1, zsize1);
            for(ii = 0; ii < xsize1; ii++)
                for(jj = 0; jj < ysize1; jj++)
                    for(kk = 0; kk < zsize1; kk++)
                    {
                        data.image[IDout].array.F[kk * ysize1 * xsize1 + jj * xsize1 + ii] =
                            data.image[ID].array.F[jj * xsize * ysize + kk * xsize + ii];
                    }
        }
    }
    else if(datatype == _DATATYPE_DOUBLE)
    {
        if(orientation == 0)
        {
            xsize1 = zsize;
            ysize1 = ysize;
            zsize1 = xsize;
            IDout = create_3Dimage_ID_double(ID_out_name, xsize1, ysize1, zsize1);
            for(ii = 0; ii < xsize1; ii++)
                for(jj = 0; jj < ysize1; jj++)
                    for(kk = 0; kk < zsize1; kk++)
                    {
                        data.image[IDout].array.D[kk * ysize1 * xsize1 + jj * xsize1 + ii] =
                            data.image[ID].array.D[ii * xsize * ysize + jj * xsize + kk];
                    }
        }
        else
        {
            xsize1 = xsize;
            ysize1 = zsize;
            zsize1 = ysize;
            IDout = create_3Dimage_ID_double(ID_out_name, xsize1, ysize1, zsize1);
            for(ii = 0; ii < xsize1; ii++)
                for(jj = 0; jj < ysize1; jj++)
                    for(kk = 0; kk < zsize1; kk++)
                    {
                        data.image[IDout].array.D[kk * ysize1 * xsize1 + jj * xsize1 + ii] =
                            data.image[ID].array.D[jj * xsize * ysize + kk * xsize + ii];
                    }
        }
    }
    else
    {
        PRINT_ERROR("Wrong image type(s)\n");
        exit(0);
    }

    return RETURN_SUCCESS;
}










/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 11. SET IMAGE FLAGS / COUNTERS                                                                  */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */





errno_t COREMOD_MEMORY_image_set_status(
    const char *IDname,
    int         status
)
{
    imageID ID;

    ID = image_ID(IDname);
    data.image[ID].md[0].status = status;

    return RETURN_SUCCESS;
}


errno_t COREMOD_MEMORY_image_set_cnt0(
    const char *IDname,
    int         cnt0
)
{
    imageID ID;

    ID = image_ID(IDname);
    data.image[ID].md[0].cnt0 = cnt0;

    return RETURN_SUCCESS;
}


errno_t COREMOD_MEMORY_image_set_cnt1(
    const char *IDname,
    int         cnt1
)
{
    imageID ID;

    ID = image_ID(IDname);
    data.image[ID].md[0].cnt1 = cnt1;

    return RETURN_SUCCESS;
}







/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 12. MANAGE SEMAPHORES                                                                           */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



/**
 * @see ImageStreamIO_createsem
 */

imageID COREMOD_MEMORY_image_set_createsem(
    const char *IDname,
    long        NBsem
)
{
    imageID ID;

    ID = image_ID(IDname);

    if(ID != -1)
    {
        ImageStreamIO_createsem(&data.image[ID], NBsem);
    }

    return ID;
}




imageID COREMOD_MEMORY_image_seminfo(
    const char *IDname
)
{
    imageID ID;


    ID = image_ID(IDname);

    printf("  NB SEMAPHORES = %3d \n", data.image[ID].md[0].sem);
    printf(" semWritePID at %p\n", (void *) data.image[ID].semWritePID);
    printf(" semReadPID  at %p\n", (void *) data.image[ID].semReadPID);
    printf("----------------------------------\n");
    printf(" sem    value   writePID   readPID\n");
    printf("----------------------------------\n");
    int s;
    for(s = 0; s < data.image[ID].md[0].sem; s++)
    {
        int semval;

        sem_getvalue(data.image[ID].semptr[s], &semval);

        printf("  %2d   %6d   %8d  %8d\n", s, semval,
               (int) data.image[ID].semWritePID[s], (int) data.image[ID].semReadPID[s]);

    }
    printf("----------------------------------\n");
    int semval;
    sem_getvalue(data.image[ID].semlog, &semval);
    printf(" semlog = %3d\n", semval);
    printf("----------------------------------\n");

    return ID;
}




/**
 * @see ImageStreamIO_sempost
 */

imageID COREMOD_MEMORY_image_set_sempost(
    const char *IDname,
    long        index
)
{
    imageID ID;

    ID = image_ID(IDname);
    if(ID == -1)
    {
        ID = read_sharedmem_image(IDname);
    }

    ImageStreamIO_sempost(&data.image[ID], index);

    return ID;
}



/**
 * @see ImageStreamIO_sempost
 */
imageID COREMOD_MEMORY_image_set_sempost_byID(
    imageID ID,
    long    index
)
{
    ImageStreamIO_sempost(&data.image[ID], index);

    return ID;
}



/**
 * @see ImageStreamIO_sempost_excl
 */
imageID COREMOD_MEMORY_image_set_sempost_excl_byID(
    imageID ID,
    long index
)
{
    ImageStreamIO_sempost_excl(&data.image[ID], index);

    return ID;
}




/**
 * @see ImageStreamIO_sempost_loop
 */

imageID COREMOD_MEMORY_image_set_sempost_loop(
    const char *IDname,
    long        index,
    long        dtus
)
{
    imageID ID;

    ID = image_ID(IDname);
    if(ID == -1)
    {
        ID = read_sharedmem_image(IDname);
    }

    ImageStreamIO_sempost_loop(&data.image[ID], index, dtus);

    return ID;
}



/**
 * @see ImageStreamIO_semwait
 */
imageID COREMOD_MEMORY_image_set_semwait(
    const char *IDname,
    long        index
)
{
    imageID ID;

    ID = image_ID(IDname);
    if(ID == -1)
    {
        ID = read_sharedmem_image(IDname);
    }

    ImageStreamIO_semwait(&data.image[ID], index);

    return ID;
}





// only works for sem0
void *waitforsemID(
    void *ID
)
{
    pthread_t tid;
    int t;
    //    int semval;


    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    tid = pthread_self();

    //    sem_getvalue(data.image[(long) ID].semptr, &semval);
    //    printf("tid %u waiting for sem ID %ld   sem = %d   (%s)\n", (unsigned int) tid, (long) ID, semval, data.image[(long) ID].name);
    //    fflush(stdout);
    sem_wait(data.image[(imageID) ID].semptr[0]);
    //    printf("tid %u sem ID %ld done\n", (unsigned int) tid, (long) ID);
    //    fflush(stdout);

    for(t = 0; t < NB_thrarray_semwait; t++)
    {
        if(tid != thrarray_semwait[t])
        {
            //            printf("tid %u cancel thread %d tid %u\n", (unsigned int) tid, t, (unsigned int) (thrarray_semwait[t]));
            //           fflush(stdout);
            pthread_cancel(thrarray_semwait[t]);
        }
    }

    pthread_exit(NULL);
}




/// \brief Wait for multiple images semaphores [OR], only works for sem0
errno_t COREMOD_MEMORY_image_set_semwait_OR_IDarray(
    imageID *IDarray,
    long     NB_ID
)
{
    int t;
//    int semval;

    //   printf("======== ENTER COREMOD_MEMORY_image_set_semwait_OR_IDarray [%ld] =======\n", NB_ID);
    //   fflush(stdout);

    thrarray_semwait = (pthread_t *) malloc(sizeof(pthread_t) * NB_ID);
    NB_thrarray_semwait = NB_ID;

    for(t = 0; t < NB_ID; t++)
    {
        //      printf("thread %d create, ID = %ld\n", t, IDarray[t]);
        //      fflush(stdout);
        pthread_create(&thrarray_semwait[t], NULL, waitforsemID, (void *)IDarray[t]);
    }

    for(t = 0; t < NB_ID; t++)
    {
        //         printf("thread %d tid %u join waiting\n", t, (unsigned int) thrarray_semwait[t]);
        //fflush(stdout);
        pthread_join(thrarray_semwait[t], NULL);
        //    printf("thread %d tid %u joined\n", t, (unsigned int) thrarray_semwait[t]);
    }

    free(thrarray_semwait);
    // printf("======== EXIT COREMOD_MEMORY_image_set_semwait_OR_IDarray =======\n");
    //fflush(stdout);

    return RETURN_SUCCESS;
}



/// \brief flush multiple semaphores
errno_t COREMOD_MEMORY_image_set_semflush_IDarray(
    imageID *IDarray,
    long     NB_ID
)
{
    long i, cnt;
    int semval;
    int s;

    list_image_ID();
    for(i = 0; i < NB_ID; i++)
    {
        for(s = 0; s < data.image[IDarray[i]].md[0].sem; s++)
        {
            sem_getvalue(data.image[IDarray[i]].semptr[s], &semval);
            printf("sem %d/%d of %s [%ld] = %d\n", s, data.image[IDarray[i]].md[0].sem,
                   data.image[IDarray[i]].name, IDarray[i], semval);
            fflush(stdout);
            for(cnt = 0; cnt < semval; cnt++)
            {
                sem_trywait(data.image[IDarray[i]].semptr[s]);
            }
        }
    }

    return RETURN_SUCCESS;
}



/// set semaphore value to 0
// if index <0, flush all image semaphores
imageID COREMOD_MEMORY_image_set_semflush(
    const char *IDname,
    long        index
)
{
    imageID ID;

    ID = image_ID(IDname);
    if(ID == -1)
    {
        ID = read_sharedmem_image(IDname);
    }

    ImageStreamIO_semflush(&data.image[ID], index);


    return ID;
}

















/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 13. SIMPLE OPERATIONS ON STREAMS                                                                */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */


/**
 * ## Purpose
 *
 * Poke a stream at regular time interval\n
 * Does not change shared memory content\n
 *
 */
imageID COREMOD_MEMORY_streamPoke(
    const char *IDstream_name,
    long        usperiod
)
{
    imageID ID;
    long    twait1;
    struct  timespec t0;
    struct  timespec t1;
    double  tdiffv;
    struct  timespec tdiff;

    ID = image_ID(IDstream_name);



    PROCESSINFO *processinfo;
    if(data.processinfo == 1)
    {
        // CREATE PROCESSINFO ENTRY
        // see processtools.c in module CommandLineInterface for details
        //
        char pinfoname[200];
        sprintf(pinfoname, "streampoke-%s", IDstream_name);
        processinfo = processinfo_shm_create(pinfoname, 0);
        processinfo->loopstat = 0; // loop initialization

        strcpy(processinfo->source_FUNCTION, __FUNCTION__);
        strcpy(processinfo->source_FILE,     __FILE__);
        processinfo->source_LINE = __LINE__;

        char msgstring[200];
        sprintf(msgstring, "%s", IDstream_name);
        processinfo_WriteMessage(processinfo, msgstring);
    }

    if(data.processinfo == 1)
    {
        processinfo->loopstat = 1;    // loop running
    }
    int loopOK = 1;
    int loopCTRLexit = 0; // toggles to 1 when loop is set to exit cleanly
    long loopcnt = 0;


    while(loopOK == 1)
    {
        // processinfo control
        if(data.processinfo == 1)
        {
            while(processinfo->CTRLval == 1)   // pause
            {
                struct timespec treq, trem;
                treq.tv_sec = 0;
                treq.tv_nsec = 50000;
                nanosleep(&treq, &trem);
            }

            if(processinfo->CTRLval == 2) // single iteration
            {
                processinfo->CTRLval = 1;
            }

            if(processinfo->CTRLval == 3) // exit loop
            {
                loopCTRLexit = 1;
            }
        }


        clock_gettime(CLOCK_REALTIME, &t0);

        data.image[ID].md[0].write = 1;
        data.image[ID].md[0].cnt0++;
        data.image[ID].md[0].write = 0;
        COREMOD_MEMORY_image_set_sempost_byID(ID, -1);



        usleep(twait1);

        clock_gettime(CLOCK_REALTIME, &t1);
        tdiff = timespec_diff(t0, t1);
        tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;

        if(tdiffv < 1.0e-6 * usperiod)
        {
            twait1 ++;
        }
        else
        {
            twait1 --;
        }

        if(twait1 < 0)
        {
            twait1 = 0;
        }
        if(twait1 > usperiod)
        {
            twait1 = usperiod;
        }


        if(loopCTRLexit == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                struct timespec tstop;
                struct tm *tstoptm;
                char msgstring[200];

                clock_gettime(CLOCK_REALTIME, &tstop);
                tstoptm = gmtime(&tstop.tv_sec);

                sprintf(msgstring, "CTRLexit at %02d:%02d:%02d.%03d", tstoptm->tm_hour,
                        tstoptm->tm_min, tstoptm->tm_sec, (int)(0.000001 * (tstop.tv_nsec)));
                strncpy(processinfo->statusmsg, msgstring, 200);

                processinfo->loopstat = 3; // clean exit
            }
        }

        loopcnt++;
        if(data.processinfo == 1)
        {
            processinfo->loopcnt = loopcnt;
        }
    }


    return ID;
}







/**
 * ## Purpose
 *
 * Compute difference between two 2D streams\n
 * Triggers on stream0\n
 *
 */
imageID COREMOD_MEMORY_streamDiff(
    const char *IDstream0_name,
    const char *IDstream1_name,
    const char *IDstreammask_name,
    const char *IDstreamout_name,
    long        semtrig
)
{
    imageID    ID0;
    imageID    ID1;
    imageID    IDout;
    uint32_t   xsize;
    uint32_t   ysize;
    uint32_t   xysize;
    long       ii;
    uint32_t  *arraysize;
    unsigned long long  cnt;
    imageID    IDmask; // optional

    ID0 = image_ID(IDstream0_name);
    ID1 = image_ID(IDstream1_name);
    IDmask = image_ID(IDstreammask_name);

    xsize = data.image[ID0].md[0].size[0];
    ysize = data.image[ID0].md[0].size[1];
    xysize = xsize * ysize;

    arraysize = (uint32_t *) malloc(sizeof(uint32_t) * 2);
    arraysize[0] = xsize;
    arraysize[1] = ysize;

    IDout = image_ID(IDstreamout_name);
    if(IDout == -1)
    {
        IDout = create_image_ID(IDstreamout_name, 2, arraysize, _DATATYPE_FLOAT, 1, 0);
        COREMOD_MEMORY_image_set_createsem(IDstreamout_name, IMAGE_NB_SEMAPHORE);
    }

    free(arraysize);


    while(1)
    {
        // has new frame arrived ?
        if(data.image[ID0].md[0].sem == 0)
        {
            while(cnt == data.image[ID0].md[0].cnt0)   // test if new frame exists
            {
                usleep(5);
            }
            cnt = data.image[ID0].md[0].cnt0;
        }
        else
        {
            sem_wait(data.image[ID0].semptr[semtrig]);
        }




        data.image[IDout].md[0].write = 1;
        if(IDmask == -1)
        {
            for(ii = 0; ii < xysize; ii++)
            {
                data.image[IDout].array.F[ii] = data.image[ID0].array.F[ii] -
                                                data.image[ID1].array.F[ii];
            }
        }
        else
        {
            for(ii = 0; ii < xysize; ii++)
            {
                data.image[IDout].array.F[ii] = (data.image[ID0].array.F[ii] -
                                                 data.image[ID1].array.F[ii]) * data.image[IDmask].array.F[ii];
            }
        }
        COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);;
        data.image[IDout].md[0].cnt0++;
        data.image[IDout].md[0].write = 0;
    }


    return IDout;
}







//
// compute difference between two 2D streams
// triggers alternatively on stream0 and stream1
//
imageID COREMOD_MEMORY_streamPaste(
    const char *IDstream0_name,
    const char *IDstream1_name,
    const char *IDstreamout_name,
    long        semtrig0,
    long        semtrig1,
    int         master
)
{
    imageID     ID0;
    imageID     ID1;
    imageID     IDout;
    imageID     IDin;
    long        Xoffset;
    uint32_t    xsize;
    uint32_t    ysize;
//    uint32_t    xysize;
    long        ii;
    long        jj;
    uint32_t   *arraysize;
    unsigned long long   cnt;
    uint8_t     datatype;
    int         FrameIndex;

    ID0 = image_ID(IDstream0_name);
    ID1 = image_ID(IDstream1_name);

    xsize = data.image[ID0].md[0].size[0];
    ysize = data.image[ID0].md[0].size[1];
//    xysize = xsize*ysize;
    datatype = data.image[ID0].md[0].datatype;

    arraysize = (uint32_t *) malloc(sizeof(uint32_t) * 2);
    arraysize[0] = 2 * xsize;
    arraysize[1] = ysize;

    IDout = image_ID(IDstreamout_name);
    if(IDout == -1)
    {
        IDout = create_image_ID(IDstreamout_name, 2, arraysize, datatype, 1, 0);
        COREMOD_MEMORY_image_set_createsem(IDstreamout_name, IMAGE_NB_SEMAPHORE);
    }
    free(arraysize);


    FrameIndex = 0;

    while(1)
    {
        if(FrameIndex == 0)
        {
            // has new frame 0 arrived ?
            if(data.image[ID0].md[0].sem == 0)
            {
                while(cnt == data.image[ID0].md[0].cnt0) // test if new frame exists
                {
                    usleep(5);
                }
                cnt = data.image[ID0].md[0].cnt0;
            }
            else
            {
                sem_wait(data.image[ID0].semptr[semtrig0]);
            }
            Xoffset = 0;
            IDin = 0;
        }
        else
        {
            // has new frame 1 arrived ?
            if(data.image[ID1].md[0].sem == 0)
            {
                while(cnt == data.image[ID1].md[0].cnt0) // test if new frame exists
                {
                    usleep(5);
                }
                cnt = data.image[ID1].md[0].cnt0;
            }
            else
            {
                sem_wait(data.image[ID1].semptr[semtrig1]);
            }
            Xoffset = xsize;
            IDin = 1;
        }


        data.image[IDout].md[0].write = 1;

        switch(datatype)
        {
            case _DATATYPE_UINT8 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.UI8[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.UI8[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_UINT16 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.UI16[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.UI16[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_UINT32 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.UI32[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.UI32[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_UINT64 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.UI64[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.UI64[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_INT8 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.SI8[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.SI8[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_INT16 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.SI16[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.SI16[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_INT32 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.SI32[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.SI32[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_INT64 :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.SI64[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.SI64[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_FLOAT :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.F[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.F[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_DOUBLE :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.D[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.D[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_COMPLEX_FLOAT :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.CF[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.CF[jj * xsize + ii];
                    }
                break;

            case _DATATYPE_COMPLEX_DOUBLE :
                for(ii = 0; ii < xsize; ii++)
                    for(jj = 0; jj < ysize; jj++)
                    {
                        data.image[IDout].array.CD[jj * 2 * xsize + ii + Xoffset] =
                            data.image[IDin].array.CD[jj * xsize + ii];
                    }
                break;

            default :
                printf("Unknown data type\n");
                exit(0);
                break;
        }
        if(FrameIndex == master)
        {
            COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);;
            data.image[IDout].md[0].cnt0++;
        }
        data.image[IDout].md[0].cnt1 = FrameIndex;
        data.image[IDout].md[0].write = 0;

        if(FrameIndex == 0)
        {
            FrameIndex = 1;
        }
        else
        {
            FrameIndex = 0;
        }
    }

    return IDout;
}












//
// compute difference between two halves of an image stream
// triggers on instream
//
imageID COREMOD_MEMORY_stream_halfimDiff(
    const char *IDstream_name,
    const char *IDstreamout_name,
    long        semtrig
)
{
    imageID    ID0;
    imageID    IDout;
    uint32_t   xsizein;
    uint32_t   ysizein;
//    uint32_t   xysizein;
    uint32_t   xsize;
    uint32_t   ysize;
    uint32_t   xysize;
    long       ii;
    uint32_t  *arraysize;
    unsigned long long  cnt;
    uint8_t    datatype;
    uint8_t    datatypeout;


    ID0 = image_ID(IDstream_name);

    xsizein = data.image[ID0].md[0].size[0];
    ysizein = data.image[ID0].md[0].size[1];
//    xysizein = xsizein*ysizein;

    xsize = xsizein;
    ysize = ysizein / 2;
    xysize = xsize * ysize;


    arraysize = (uint32_t *) malloc(sizeof(uint32_t) * 2);
    arraysize[0] = xsize;
    arraysize[1] = ysize;

    datatype = data.image[ID0].md[0].datatype;
    datatypeout = _DATATYPE_FLOAT;
    switch(datatype)
    {

        case _DATATYPE_UINT8:
            datatypeout = _DATATYPE_INT16;
            break;

        case _DATATYPE_UINT16:
            datatypeout = _DATATYPE_INT32;
            break;

        case _DATATYPE_UINT32:
            datatypeout = _DATATYPE_INT64;
            break;

        case _DATATYPE_UINT64:
            datatypeout = _DATATYPE_INT64;
            break;


        case _DATATYPE_INT8:
            datatypeout = _DATATYPE_INT16;
            break;

        case _DATATYPE_INT16:
            datatypeout = _DATATYPE_INT32;
            break;

        case _DATATYPE_INT32:
            datatypeout = _DATATYPE_INT64;
            break;

        case _DATATYPE_INT64:
            datatypeout = _DATATYPE_INT64;
            break;

        case _DATATYPE_DOUBLE:
            datatypeout = _DATATYPE_DOUBLE;
            break;
    }

    IDout = image_ID(IDstreamout_name);
    if(IDout == -1)
    {
        IDout = create_image_ID(IDstreamout_name, 2, arraysize, datatypeout, 1, 0);
        COREMOD_MEMORY_image_set_createsem(IDstreamout_name, IMAGE_NB_SEMAPHORE);
    }

    free(arraysize);



    while(1)
    {
        // has new frame arrived ?
        if(data.image[ID0].md[0].sem == 0)
        {
            while(cnt == data.image[ID0].md[0].cnt0) // test if new frame exists
            {
                usleep(5);
            }
            cnt = data.image[ID0].md[0].cnt0;
        }
        else
        {
            sem_wait(data.image[ID0].semptr[semtrig]);
        }

        data.image[IDout].md[0].write = 1;

        switch(datatype)
        {

            case _DATATYPE_UINT8:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI16[ii] = data.image[ID0].array.UI8[ii] -
                                                       data.image[ID0].array.UI8[xysize + ii];
                }
                break;

            case _DATATYPE_UINT16:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI32[ii] = data.image[ID0].array.UI16[ii] -
                                                       data.image[ID0].array.UI16[xysize + ii];
                }
                break;

            case _DATATYPE_UINT32:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI64[ii] = data.image[ID0].array.UI32[ii] -
                                                       data.image[ID0].array.UI32[xysize + ii];
                }
                break;

            case _DATATYPE_UINT64:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI64[ii] = data.image[ID0].array.UI64[ii] -
                                                       data.image[ID0].array.UI64[xysize + ii];
                }
                break;



            case _DATATYPE_INT8:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI16[ii] = data.image[ID0].array.SI8[ii] -
                                                       data.image[ID0].array.SI8[xysize + ii];
                }
                break;

            case _DATATYPE_INT16:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI32[ii] = data.image[ID0].array.SI16[ii] -
                                                       data.image[ID0].array.SI16[xysize + ii];
                }
                break;

            case _DATATYPE_INT32:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI64[ii] = data.image[ID0].array.SI32[ii] -
                                                       data.image[ID0].array.SI32[xysize + ii];
                }
                break;

            case _DATATYPE_INT64:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.SI64[ii] = data.image[ID0].array.SI64[ii] -
                                                       data.image[ID0].array.SI64[xysize + ii];
                }
                break;



            case _DATATYPE_FLOAT:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.F[ii] = data.image[ID0].array.F[ii] -
                                                    data.image[ID0].array.F[xysize + ii];
                }
                break;

            case _DATATYPE_DOUBLE:
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.D[ii] = data.image[ID0].array.D[ii] -
                                                    data.image[ID0].array.D[xysize + ii];
                }
                break;

        }

        COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);
        data.image[IDout].md[0].cnt0++;
        data.image[IDout].md[0].write = 0;
    }


    return IDout;
}





imageID COREMOD_MEMORY_streamAve(
    const char *IDstream_name,
    int         NBave,
    int         mode,
    const char *IDout_name
)
{
    imageID      IDout;
    imageID      IDout0;
    imageID      IDin;
    uint8_t      datatype;
    uint32_t     xsize;
    uint32_t     ysize;
    uint32_t     xysize;
    uint32_t    *imsize;
    int_fast8_t  OKloop;
    int          cntin = 0;
    long         dtus = 20;
    long         ii;
    long         cnt0;
    long         cnt0old;

    IDin = image_ID(IDstream_name);
    datatype = data.image[IDin].md[0].datatype;
    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    xysize = xsize * ysize;


    IDout0 = create_2Dimage_ID("_streamAve_tmp", xsize, ysize);

    if(mode == 1) // local image
    {
        IDout = create_2Dimage_ID(IDout_name, xsize, ysize);
    }
    else // shared memory
    {
        IDout = image_ID(IDout_name);
        if(IDout == -1) // CREATE IT
        {
            imsize = (uint32_t *) malloc(sizeof(uint32_t) * 2);
            imsize[0] = xsize;
            imsize[1] = ysize;
            IDout = create_image_ID(IDout_name, 2, imsize, _DATATYPE_FLOAT, 1, 0);
            COREMOD_MEMORY_image_set_createsem(IDout_name, IMAGE_NB_SEMAPHORE);
            free(imsize);
        }
    }


    cntin = 0;
    cnt0old = data.image[IDin].md[0].cnt0;

    for(ii = 0; ii < xysize; ii++)
    {
        data.image[IDout].array.F[ii] = 0.0;
    }

    OKloop = 1;
    while(OKloop == 1)
    {
        // has new frame arrived ?
        cnt0 = data.image[IDin].md[0].cnt0;
        if(cnt0 != cnt0old)
        {
            switch(datatype)
            {
                case _DATATYPE_UINT8 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.UI8[ii];
                    }
                    break;

                case _DATATYPE_INT8 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.SI8[ii];
                    }
                    break;

                case _DATATYPE_UINT16 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.UI16[ii];
                    }
                    break;

                case _DATATYPE_INT16 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.SI16[ii];
                    }
                    break;

                case _DATATYPE_UINT32 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.UI32[ii];
                    }
                    break;

                case _DATATYPE_INT32 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.SI32[ii];
                    }
                    break;

                case _DATATYPE_UINT64 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.UI64[ii];
                    }
                    break;

                case _DATATYPE_INT64 :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.SI64[ii];
                    }
                    break;

                case _DATATYPE_FLOAT :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.F[ii];
                    }
                    break;

                case _DATATYPE_DOUBLE :
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout0].array.F[ii] += data.image[IDin].array.D[ii];
                    }
                    break;
            }

            cntin++;
            if(cntin == NBave)
            {
                cntin = 0;
                data.image[IDout].md[0].write = 1;
                for(ii = 0; ii < xysize; ii++)
                {
                    data.image[IDout].array.F[ii] = data.image[IDout0].array.F[ii] / NBave;
                }
                data.image[IDout].md[0].cnt0++;
                data.image[IDout].md[0].write = 0;
                COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);

                if(mode != 1)
                {
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout].array.F[ii] = 0.0;
                    }
                }
                else
                {
                    OKloop = 0;
                }
            }
            cnt0old = cnt0;
        }
        usleep(dtus);
    }

    delete_image_ID("_streamAve_tmp");

    return IDout;
}






/** @brief takes a 3Dimage(s) (circular buffer(s)) and writes slices to a 2D image with time interval specified in us
 *
 *
 * If NBcubes=1, then the circular buffer named IDinname is sent to IDoutname at a frequency of 1/usperiod MHz
 * If NBcubes>1, several circular buffers are used, named ("%S_%03ld", IDinname, cubeindex). Semaphore semtrig of image IDsync_name triggers switch between circular buffers, with a delay of offsetus. The number of consecutive sem posts required to advance to the next circular buffer is period
 *
 * @param IDinname      Name of DM circular buffer (appended by _000, _001 etc... if NBcubes>1)
 * @param IDoutname     Output DM channel stream
 * @param usperiod      Interval between consecutive frames [us]
 * @param NBcubes       Number of input DM circular buffers
 * @param period        If NBcubes>1: number of input triggers required to advance to next input buffer
 * @param offsetus      If NBcubes>1: time offset [us] between input trigger and input buffer switch
 * @param IDsync_name   If NBcubes>1: Stream used for synchronization
 * @param semtrig       If NBcubes>1: semaphore used for synchronization
 * @param timingmode    Not used
 *
 *
 */
imageID COREMOD_MEMORY_image_streamupdateloop(
    const char *IDinname,
    const char *IDoutname,
    long        usperiod,
    long        NBcubes,
    long        period,
    long        offsetus,
    const char *IDsync_name,
    int         semtrig,
    __attribute__((unused)) int         timingmode
)
{
    imageID   *IDin;
    long       cubeindex;
    char       imname[200];
    long       IDsync;
    unsigned long long  cntsync;
    long       pcnt = 0;
    long       offsetfr = 0;
    long       offsetfrcnt = 0;
    int        cntDelayMode = 0;

    imageID    IDout;
    long       kk;
    uint32_t  *arraysize;
    long       naxis;
    uint8_t    datatype;
    char      *ptr0s; // source start 3D array ptr
    char      *ptr0; // source
    char      *ptr1; // dest
    long       framesize;
//    int        semval;

    int        RT_priority = 80; //any number from 0-99
    struct     sched_param schedpar;

    long       twait1;
    struct     timespec t0;
    struct     timespec t1;
    double     tdiffv;
    struct     timespec tdiff;

    int        SyncSlice = 0;



    schedpar.sched_priority = RT_priority;
#ifndef __MACH__
    sched_setscheduler(0, SCHED_FIFO,
                       &schedpar); //other option is SCHED_RR, might be faster
#endif


    PROCESSINFO *processinfo;
    if(data.processinfo == 1)
    {
        // CREATE PROCESSINFO ENTRY
        // see processtools.c in module CommandLineInterface for details
        //
        char pinfoname[200];
        sprintf(pinfoname, "streamloop-%s", IDoutname);
        processinfo = processinfo_shm_create(pinfoname, 0);
        processinfo->loopstat = 0; // loop initialization

        strcpy(processinfo->source_FUNCTION, __FUNCTION__);
        strcpy(processinfo->source_FILE,     __FILE__);
        processinfo->source_LINE = __LINE__;

        char msgstring[200];
        sprintf(msgstring, "%s->%s", IDinname, IDoutname);
        processinfo_WriteMessage(processinfo, msgstring);
    }




    if(NBcubes < 1)
    {
        printf("ERROR: invalid number of input cubes, needs to be >0");
        return RETURN_FAILURE;
    }


    int sync_semwaitindex;
    IDin = (long *) malloc(sizeof(long) * NBcubes);
    SyncSlice = 0;
    if(NBcubes == 1)
    {
        IDin[0] = image_ID(IDinname);

        // in single cube mode, optional sync stream drives updates to next slice within cube
        IDsync = image_ID(IDsync_name);
        if(IDsync != -1)
        {
            SyncSlice = 1;
            sync_semwaitindex = ImageStreamIO_getsemwaitindex(&data.image[IDsync], semtrig);
        }
    }
    else
    {
        IDsync = image_ID(IDsync_name);
        sync_semwaitindex = ImageStreamIO_getsemwaitindex(&data.image[IDsync], semtrig);

        for(cubeindex = 0; cubeindex < NBcubes; cubeindex++)
        {
            sprintf(imname, "%s_%03ld", IDinname, cubeindex);
            IDin[cubeindex] = image_ID(imname);
        }
        offsetfr = (long)(0.5 + 1.0 * offsetus / usperiod);

        printf("FRAMES OFFSET = %ld\n", offsetfr);
    }



    printf("SyncSlice = %d\n", SyncSlice);

    printf("Creating / connecting to image stream ...\n");
    fflush(stdout);


    naxis = data.image[IDin[0]].md[0].naxis;
    arraysize = (uint32_t *) malloc(sizeof(uint32_t) * 3);
    if(naxis != 3)
    {
        printf("ERROR: input image %s should be 3D\n", IDinname);
        exit(0);
    }
    arraysize[0] = data.image[IDin[0]].md[0].size[0];
    arraysize[1] = data.image[IDin[0]].md[0].size[1];
    arraysize[2] = data.image[IDin[0]].md[0].size[2];



    datatype = data.image[IDin[0]].md[0].datatype;

    IDout = image_ID(IDoutname);
    if(IDout == -1)
    {
        IDout = create_image_ID(IDoutname, 2, arraysize, datatype, 1, 0);
        COREMOD_MEMORY_image_set_createsem(IDoutname, IMAGE_NB_SEMAPHORE);
    }

    cubeindex = 0;
    pcnt = 0;
    if(NBcubes > 1)
    {
        cntsync = data.image[IDsync].md[0].cnt0;
    }

    twait1 = usperiod;
    kk = 0;
    cntDelayMode = 0;



    if(data.processinfo == 1)
    {
        processinfo->loopstat = 1;    // loop running
    }
    int loopOK = 1;
    int loopCTRLexit = 0; // toggles to 1 when loop is set to exit cleanly
    long loopcnt = 0;

    while(loopOK == 1)
    {

        // processinfo control
        if(data.processinfo == 1)
        {
            while(processinfo->CTRLval == 1)  // pause
            {
                usleep(50);
            }

            if(processinfo->CTRLval == 2) // single iteration
            {
                processinfo->CTRLval = 1;
            }

            if(processinfo->CTRLval == 3) // exit loop
            {
                loopCTRLexit = 1;
            }
        }



        if(NBcubes > 1)
        {
            if(cntsync != data.image[IDsync].md[0].cnt0)
            {
                pcnt++;
                cntsync = data.image[IDsync].md[0].cnt0;
            }
            if(pcnt == period)
            {
                pcnt = 0;
                offsetfrcnt = 0;
                cntDelayMode = 1;
            }

            if(cntDelayMode == 1)
            {
                if(offsetfrcnt < offsetfr)
                {
                    offsetfrcnt++;
                }
                else
                {
                    cntDelayMode = 0;
                    cubeindex++;
                    kk = 0;
                }
            }
            if(cubeindex == NBcubes)
            {
                cubeindex = 0;
            }
        }


        switch(datatype)
        {

            case _DATATYPE_INT8:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.SI8;
                ptr1 = (char *) data.image[IDout].array.SI8;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_INT8;
                break;

            case _DATATYPE_UINT8:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.UI8;
                ptr1 = (char *) data.image[IDout].array.UI8;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_UINT8;
                break;

            case _DATATYPE_INT16:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.SI16;
                ptr1 = (char *) data.image[IDout].array.SI16;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_INT16;
                break;

            case _DATATYPE_UINT16:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.UI16;
                ptr1 = (char *) data.image[IDout].array.UI16;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_UINT16;
                break;

            case _DATATYPE_INT32:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.SI32;
                ptr1 = (char *) data.image[IDout].array.SI32;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_INT32;
                break;

            case _DATATYPE_UINT32:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.UI32;
                ptr1 = (char *) data.image[IDout].array.UI32;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_UINT32;
                break;

            case _DATATYPE_INT64:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.SI64;
                ptr1 = (char *) data.image[IDout].array.SI64;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_INT64;
                break;

            case _DATATYPE_UINT64:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.UI64;
                ptr1 = (char *) data.image[IDout].array.UI64;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * SIZEOF_DATATYPE_UINT64;
                break;


            case _DATATYPE_FLOAT:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.F;
                ptr1 = (char *) data.image[IDout].array.F;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * sizeof(float);
                break;

            case _DATATYPE_DOUBLE:
                ptr0s = (char *) data.image[IDin[cubeindex]].array.D;
                ptr1 = (char *) data.image[IDout].array.D;
                framesize = data.image[IDin[cubeindex]].md[0].size[0] *
                            data.image[IDin[cubeindex]].md[0].size[1] * sizeof(double);
                break;

        }




        clock_gettime(CLOCK_REALTIME, &t0);

        ptr0 = ptr0s + kk * framesize;
        data.image[IDout].md[0].write = 1;
        memcpy((void *) ptr1, (void *) ptr0, framesize);
        data.image[IDout].md[0].cnt1 = kk;
        data.image[IDout].md[0].cnt0++;
        data.image[IDout].md[0].write = 0;
        COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);

        kk++;
        if(kk == data.image[IDin[0]].md[0].size[2])
        {
            kk = 0;
        }



        if(SyncSlice == 0)
        {
            usleep(twait1);

            clock_gettime(CLOCK_REALTIME, &t1);
            tdiff = timespec_diff(t0, t1);
            tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;

            if(tdiffv < 1.0e-6 * usperiod)
            {
                twait1 ++;
            }
            else
            {
                twait1 --;
            }

            if(twait1 < 0)
            {
                twait1 = 0;
            }
            if(twait1 > usperiod)
            {
                twait1 = usperiod;
            }
        }
        else
        {
            sem_wait(data.image[IDsync].semptr[sync_semwaitindex]);
        }

        if(loopCTRLexit == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                struct timespec tstop;
                struct tm *tstoptm;
                char msgstring[200];

                clock_gettime(CLOCK_REALTIME, &tstop);
                tstoptm = gmtime(&tstop.tv_sec);

                sprintf(msgstring, "CTRLexit at %02d:%02d:%02d.%03d", tstoptm->tm_hour,
                        tstoptm->tm_min, tstoptm->tm_sec, (int)(0.000001 * (tstop.tv_nsec)));
                strncpy(processinfo->statusmsg, msgstring, 200);

                processinfo->loopstat = 3; // clean exit
            }
        }

        loopcnt++;
        if(data.processinfo == 1)
        {
            processinfo->loopcnt = loopcnt;
        }
    }

    free(IDin);

    return IDout;
}







// takes a 3Dimage (circular buffer) and writes slices to a 2D image synchronized with an image semaphore
imageID COREMOD_MEMORY_image_streamupdateloop_semtrig(
    const char *IDinname,
    const char *IDoutname,
    long        period,
    long        offsetus,
    const char *IDsync_name,
    int         semtrig,
    __attribute__((unused)) int         timingmode
)
{
    imageID    IDin;
    imageID    IDout;
    imageID    IDsync;

    long       kk;
    long       kk1;

    uint32_t  *arraysize;
    long       naxis;
    uint8_t    datatype;
    char      *ptr0s; // source start 3D array ptr
    char      *ptr0; // source
    char      *ptr1; // dest
    long       framesize;
//    int        semval;

    int        RT_priority = 80; //any number from 0-99
    struct     sched_param schedpar;


    schedpar.sched_priority = RT_priority;
#ifndef __MACH__
    sched_setscheduler(0, SCHED_FIFO,
                       &schedpar); //other option is SCHED_RR, might be faster
#endif


    printf("Creating / connecting to image stream ...\n");
    fflush(stdout);

    IDin = image_ID(IDinname);
    naxis = data.image[IDin].md[0].naxis;
    arraysize = (uint32_t *) malloc(sizeof(uint32_t) * 3);
    if(naxis != 3)
    {
        printf("ERROR: input image %s should be 3D\n", IDinname);
        exit(0);
    }
    arraysize[0] = data.image[IDin].md[0].size[0];
    arraysize[1] = data.image[IDin].md[0].size[1];
    arraysize[2] = data.image[IDin].md[0].size[2];





    datatype = data.image[IDin].md[0].datatype;

    IDout = image_ID(IDoutname);
    if(IDout == -1)
    {
        IDout = create_image_ID(IDoutname, 2, arraysize, datatype, 1, 0);
        COREMOD_MEMORY_image_set_createsem(IDoutname, IMAGE_NB_SEMAPHORE);
    }

    switch(datatype)
    {

        case _DATATYPE_INT8:
            ptr0s = (char *) data.image[IDin].array.SI8;
            ptr1 = (char *) data.image[IDout].array.SI8;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_INT8;
            break;

        case _DATATYPE_UINT8:
            ptr0s = (char *) data.image[IDin].array.UI8;
            ptr1 = (char *) data.image[IDout].array.UI8;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_UINT8;
            break;

        case _DATATYPE_INT16:
            ptr0s = (char *) data.image[IDin].array.SI16;
            ptr1 = (char *) data.image[IDout].array.SI16;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_INT16;
            break;

        case _DATATYPE_UINT16:
            ptr0s = (char *) data.image[IDin].array.UI16;
            ptr1 = (char *) data.image[IDout].array.UI16;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_UINT16;
            break;

        case _DATATYPE_INT32:
            ptr0s = (char *) data.image[IDin].array.SI32;
            ptr1 = (char *) data.image[IDout].array.SI32;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_INT32;
            break;

        case _DATATYPE_UINT32:
            ptr0s = (char *) data.image[IDin].array.UI32;
            ptr1 = (char *) data.image[IDout].array.UI32;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_UINT32;
            break;

        case _DATATYPE_INT64:
            ptr0s = (char *) data.image[IDin].array.SI64;
            ptr1 = (char *) data.image[IDout].array.SI64;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_INT64;
            break;

        case _DATATYPE_UINT64:
            ptr0s = (char *) data.image[IDin].array.UI64;
            ptr1 = (char *) data.image[IDout].array.UI64;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        SIZEOF_DATATYPE_UINT64;
            break;


        case _DATATYPE_FLOAT:
            ptr0s = (char *) data.image[IDin].array.F;
            ptr1 = (char *) data.image[IDout].array.F;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        sizeof(float);
            break;

        case _DATATYPE_DOUBLE:
            ptr0s = (char *) data.image[IDin].array.D;
            ptr1 = (char *) data.image[IDout].array.D;
            framesize = data.image[IDin].md[0].size[0] * data.image[IDin].md[0].size[1] *
                        sizeof(double);
            break;
    }




    IDsync = image_ID(IDsync_name);

    kk = 0;
    kk1 = 0;

    int sync_semwaitindex;
    sync_semwaitindex = ImageStreamIO_getsemwaitindex(&data.image[IDin], semtrig);

    while(1)
    {
        sem_wait(data.image[IDsync].semptr[sync_semwaitindex]);

        kk++;
        if(kk == period) // UPDATE
        {
            kk = 0;
            kk1++;
            if(kk1 == data.image[IDin].md[0].size[2])
            {
                kk1 = 0;
            }
            usleep(offsetus);
            ptr0 = ptr0s + kk1 * framesize;
            data.image[IDout].md[0].write = 1;
            memcpy((void *) ptr1, (void *) ptr0, framesize);
            COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);
            data.image[IDout].md[0].cnt0++;
            data.image[IDout].md[0].write = 0;
        }
    }

    // release semaphore
    data.image[IDsync].semReadPID[sync_semwaitindex] = 0;

    return IDout;
}











/**
 * @brief Manages configuration parameters for streamDelay
 *
 * ## Purpose
 *
 * Initializes configuration parameters structure\n
 *
 * ## Arguments
 *
 * @param[in]
 * char*		fpsname
 * 				name of function parameter structure
 *
 * @param[in]
 * uint32_t		CMDmode
 * 				Command mode
 *
 *
 */
errno_t COREMOD_MEMORY_streamDelay_FPCONF(
    char    *fpsname,
    uint32_t CMDmode
)
{

    FPS_SETUP_INIT(fpsname, CMDmode);

    void *pNull = NULL;
    uint64_t FPFLAG;

    FPFLAG = FPFLAG_DEFAULT_INPUT | FPFLAG_MINLIMIT;
    FPFLAG &= ~FPFLAG_WRITERUN;

    long delayus_default[4] = { 1000, 1, 10000, 1000 };
    long fp_delayus = function_parameter_add_entry(&fps, ".delayus", "Delay [us]",
                      FPTYPE_INT64, FPFLAG, &delayus_default);
    (void) fp_delayus; // suppresses unused parameter compiler warning

    long dtus_default[4] = { 50, 1, 200, 50 };
    long fp_dtus    = function_parameter_add_entry(&fps, ".dtus",
                      "Loop period [us]", FPTYPE_INT64, FPFLAG, &dtus_default);
    (void) fp_dtus; // suppresses unused parameter compiler warning


    FPS_ADDPARAM_STREAM_IN(stream_inname,        ".in_name",     "input stream");
    FPS_ADDPARAM_STREAM_OUT(stream_outname,       ".out_name",    "output stream");

    long timeavemode_default[4] = { 0, 0, 3, 0 };
    FPS_ADDPARAM_INT64_IN(
        option_timeavemode,
        ".option.timeavemode",
        "Enable time window averaging (>0)",
        &timeavemode_default);

    double avedt_default[4] = { 0.001, 0.0001, 1.0, 0.001};
    FPS_ADDPARAM_FLT64_IN(
        option_avedt,
        ".option.avedt",
        "Averaging time window width",
        &avedt_default);

    // status
    FPS_ADDPARAM_INT64_OUT(zsize,        ".status.zsize",     "cube size");
    FPS_ADDPARAM_INT64_OUT(framelog,     ".status.framelag",  "lag in frame unit");
    FPS_ADDPARAM_INT64_OUT(kkin,         ".status.kkin",
                           "input cube slice index");
    FPS_ADDPARAM_INT64_OUT(kkout,        ".status.kkout",
                           "output cube slice index");





    // ==============================================
    // start function parameter conf loop, defined in function_parameter.h
    FPS_CONFLOOP_START
    // ==============================================


    // here goes the logic
    if(fps.parray[fp_option_timeavemode].val.l[0] !=
            0)     // time averaging enabled
    {
        fps.parray[fp_option_avedt].fpflag |= FPFLAG_WRITERUN;
        fps.parray[fp_option_avedt].fpflag |= FPFLAG_USED;
        fps.parray[fp_option_avedt].fpflag |= FPFLAG_VISIBLE;
    }
    else
    {
        fps.parray[fp_option_avedt].fpflag &= ~FPFLAG_WRITERUN;
        fps.parray[fp_option_avedt].fpflag &= ~FPFLAG_USED;
        fps.parray[fp_option_avedt].fpflag &= ~FPFLAG_VISIBLE;
    }


    // ==============================================
    // stop function parameter conf loop, defined in function_parameter.h
    FPS_CONFLOOP_END
    // ==============================================


    return RETURN_SUCCESS;
}














/**
 * @brief Delay image stream by time offset
 *
 * IDout_name is a time-delayed copy of IDin_name
 *
 */

imageID COREMOD_MEMORY_streamDelay_RUN(
    char *fpsname
)
{
    imageID             IDimc;
    imageID             IDin, IDout;
    uint32_t            xsize, ysize, xysize;
//    long                cnt0old;
    long                ii;
    struct timespec    *t0array;
    struct timespec     tnow;
    double              tdiffv;
    struct timespec     tdiff;
    uint32_t           *arraytmp;
    long                cntskip = 0;
    long                kk;


    // ===========================
    /// ### CONNECT TO FPS
    // ===========================
    FPS_CONNECT(fpsname, FPSCONNECT_RUN);


    // ===============================
    /// ### GET FUNCTION PARAMETER VALUES
    // ===============================
    // parameters are addressed by their tag name
    // These parameters are read once, before running the loop
    //
    char IDin_name[200];
    strncpy(IDin_name,  functionparameter_GetParamPtr_STRING(&fps, ".in_name"),
            FUNCTION_PARAMETER_STRMAXLEN);

    char IDout_name[200];
    strncpy(IDout_name, functionparameter_GetParamPtr_STRING(&fps, ".out_name"),
            FUNCTION_PARAMETER_STRMAXLEN);

    long delayus = functionparameter_GetParamValue_INT64(&fps, ".delayus");

    long dtus    = functionparameter_GetParamValue_INT64(&fps, ".dtus");

    int timeavemode = functionparameter_GetParamValue_INT64(&fps,
                      ".option.timeavemode");
    double *avedt   = functionparameter_GetParamPtr_FLOAT64(&fps, ".option.avedt");

    long *zsize    = functionparameter_GetParamPtr_INT64(&fps, ".status.zsize");
    long *framelag = functionparameter_GetParamPtr_INT64(&fps, ".status.framelag");
    long *kkin     = functionparameter_GetParamPtr_INT64(&fps, ".status.kkin");
    long *kkout    = functionparameter_GetParamPtr_INT64(&fps, ".status.kkout");

    DEBUG_TRACEPOINT(" ");

    // ===========================
    /// ### processinfo support
    // ===========================
    PROCESSINFO *processinfo;

    char pinfodescr[200];
    sprintf(pinfodescr, "streamdelay %.10s %.10s", IDin_name, IDout_name);
    processinfo = processinfo_setup(
                      fpsname,                 // re-use fpsname as processinfo name
                      pinfodescr,    // description
                      "startup",  // message on startup
                      __FUNCTION__, __FILE__, __LINE__
                  );

    // OPTIONAL SETTINGS
    processinfo->MeasureTiming = 1; // Measure timing
    processinfo->RT_priority =
        20;  // RT_priority, 0-99. Larger number = higher priority. If <0, ignore




    // =============================================
    /// ### OPTIONAL: TESTING CONDITION FOR LOOP ENTRY
    // =============================================
    // Pre-loop testing, anything that would prevent loop from starting should issue message
    int loopOK = 1;


    IDin = image_ID(IDin_name);



    // ERROR HANDLING
    if(IDin == -1)
    {
        struct timespec errtime;
        struct tm *errtm;

        clock_gettime(CLOCK_REALTIME, &errtime);
        errtm = gmtime(&errtime.tv_sec);

        fprintf(stderr,
                "%02d:%02d:%02d.%09ld  ERROR [%s %s %d] Input stream %s does not exist, cannot proceed\n",
                errtm->tm_hour,
                errtm->tm_min,
                errtm->tm_sec,
                errtime.tv_nsec,
                __FILE__,
                __FUNCTION__,
                __LINE__,
                IDin_name);

        char msgstring[200];
        sprintf(msgstring, "Input stream %.20s does not exist", IDin_name);
        processinfo_error(processinfo, msgstring);
        loopOK = 0;
    }


    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    *zsize = (long)(2 * delayus / dtus);
    if(*zsize < 1)
    {
        *zsize = 1;
    }
    xysize = xsize * ysize;

    t0array = (struct timespec *) malloc(sizeof(struct timespec) * *zsize);

    IDimc = create_3Dimage_ID("_tmpc", xsize, ysize, *zsize);



    IDout = image_ID(IDout_name);
    if(IDout == -1)   // CREATE IT
    {
        arraytmp = (uint32_t *) malloc(sizeof(uint32_t) * 2);
        arraytmp[0] = xsize;
        arraytmp[1] = ysize;
        IDout = create_image_ID(IDout_name, 2, arraytmp, _DATATYPE_FLOAT, 1, 0);
        COREMOD_MEMORY_image_set_createsem(IDout_name, IMAGE_NB_SEMAPHORE);
        free(arraytmp);
    }


    *kkin = 0;
    *kkout = 0;
//    cnt0old = data.image[IDin].md[0].cnt0;

    float *arraytmpf;
    arraytmpf = (float *) malloc(sizeof(float) * xsize * ysize);

    clock_gettime(CLOCK_REALTIME, &tnow);
    for(kk = 0; kk < *zsize; kk++)
    {
        t0array[kk] = tnow;
    }


    DEBUG_TRACEPOINT(" ");

    // ===========================
    /// ### START LOOP
    // ===========================

    processinfo_loopstart(
        processinfo); // Notify processinfo that we are entering loop

    DEBUG_TRACEPOINT(" ");

    while(loopOK == 1)
    {
        int kkinscan;
        float normframes = 0.0;

        DEBUG_TRACEPOINT(" ");
        loopOK = processinfo_loopstep(processinfo);

        usleep(dtus); // main loop wait

        processinfo_exec_start(processinfo);

        if(processinfo_compute_status(processinfo) == 1)
        {
            DEBUG_TRACEPOINT(" ");

            // has new frame arrived ?
//            cnt0 = data.image[IDin].md[0].cnt0;

//            if(cnt0 != cnt0old) { // new frame
            clock_gettime(CLOCK_REALTIME, &t0array[*kkin]);  // record time of input frame

            DEBUG_TRACEPOINT(" ");
            for(ii = 0; ii < xysize; ii++)
            {
                data.image[IDimc].array.F[(*kkin) * xysize + ii] = data.image[IDin].array.F[ii];
            }
            (*kkin) ++;
            DEBUG_TRACEPOINT(" ");

            if((*kkin) == (*zsize))
            {
                (*kkin) = 0;
            }
            //              cnt0old = cnt0;
            //          }



            clock_gettime(CLOCK_REALTIME, &tnow);
            DEBUG_TRACEPOINT(" ");


            cntskip = 0;
            tdiff = timespec_diff(t0array[*kkout], tnow);
            tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;

            DEBUG_TRACEPOINT(" ");


            while((tdiffv > 1.0e-6 * delayus) && (cntskip < *zsize))
            {
                cntskip++;  // advance index until time condition is satisfied
                (*kkout) ++;
                if(*kkout == *zsize)
                {
                    *kkout = 0;
                }
                tdiff = timespec_diff(t0array[*kkout], tnow);
                tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;
            }

            DEBUG_TRACEPOINT(" ");

            *framelag = *kkin - *kkout;
            if(*framelag < 0)
            {
                *framelag += *zsize;
            }


            DEBUG_TRACEPOINT(" ");


            switch(timeavemode)
            {


                case 0: // no time averaging - pick more recent frame that matches requirement
                    DEBUG_TRACEPOINT(" ");
                    if(cntskip > 0)
                    {
                        char *ptr; // pointer address

                        data.image[IDout].md[0].write = 1;

                        ptr = (char *) data.image[IDimc].array.F;
                        ptr += SIZEOF_DATATYPE_FLOAT * xysize * *kkout;

                        memcpy(data.image[IDout].array.F, ptr,
                               SIZEOF_DATATYPE_FLOAT * xysize);  // copy time-delayed input to output

                        COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);
                        data.image[IDout].md[0].cnt0++;
                        data.image[IDout].md[0].write = 0;
                    }
                    break;

                default : // strict time window (note: other modes will be coded in the future)
                    normframes = 0.0;
                    DEBUG_TRACEPOINT(" ");

                    for(ii = 0; ii < xysize; ii++)
                    {
                        arraytmpf[ii] = 0.0;
                    }

                    for(kkinscan = 0; kkinscan < *zsize; kkinscan++)
                    {
                        tdiff = timespec_diff(t0array[kkinscan], tnow);
                        tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;

                        if((tdiffv > 0) && (fabs(tdiffv - 1.0e-6 * delayus) < *avedt))
                        {
                            float coeff = 1.0;
                            for(ii = 0; ii < xysize; ii++)
                            {
                                arraytmpf[ii] += coeff * data.image[IDimc].array.F[kkinscan * xysize + ii];
                            }
                            normframes += coeff;
                        }
                    }
                    if(normframes < 0.0001)
                    {
                        normframes = 0.0001;    // avoid division by zero
                    }

                    data.image[IDout].md[0].write = 1;
                    for(ii = 0; ii < xysize; ii++)
                    {
                        data.image[IDout].array.F[ii] = arraytmpf[ii] / normframes;
                    }
                    COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);
                    data.image[IDout].md[0].cnt0++;
                    data.image[IDout].md[0].write = 0;

                    break;
            }
            DEBUG_TRACEPOINT(" ");



        }
        // process signals, increment loop counter
        processinfo_exec_end(processinfo);
        DEBUG_TRACEPOINT(" ");
    }

    // ==================================
    /// ### ENDING LOOP
    // ==================================
    processinfo_cleanExit(processinfo);
    function_parameter_RUNexit(&fps);

    DEBUG_TRACEPOINT(" ");

    delete_image_ID("_tmpc");

    free(t0array);
    free(arraytmpf);

    return IDout;
}





























errno_t COREMOD_MEMORY_streamDelay(
    const char *IDin_name,
    const char *IDout_name,
    long        delayus,
    long        dtus
)
{
    char fpsname[200];
    unsigned int pindex = 0;
    FUNCTION_PARAMETER_STRUCT fps;

    // create FPS
    sprintf(fpsname, "%s-%06u", __FUNCTION__, pindex);
    COREMOD_MEMORY_streamDelay_FPCONF(fpsname, FPSCMDCODE_FPSINIT);

    function_parameter_struct_connect(fpsname, &fps, FPSCONNECT_RUN);

    functionparameter_SetParamValue_STRING(&fps, ".instreamname", IDin_name);
    functionparameter_SetParamValue_STRING(&fps, ".outstreamname", IDout_name);

    functionparameter_SetParamValue_INT64(&fps, ".delayus", delayus);
    functionparameter_SetParamValue_INT64(&fps, ".dtus", dtus);

    function_parameter_struct_disconnect(&fps);

    COREMOD_MEMORY_streamDelay_RUN(fpsname);

    return RETURN_SUCCESS;
}














//
// save all current images/stream onto file
//
errno_t COREMOD_MEMORY_SaveAll_snapshot(
    const char *dirname
)
{
    long *IDarray;
    long *IDarraycp;
    long i;
    long imcnt = 0;
    char imnamecp[200];
    char fnamecp[500];
    long ID;


    for(i = 0; i < data.NB_MAX_IMAGE; i++)
        if(data.image[i].used == 1)
        {
            imcnt++;
        }

    IDarray = (long *) malloc(sizeof(long) * imcnt);
    IDarraycp = (long *) malloc(sizeof(long) * imcnt);

    imcnt = 0;
    for(i = 0; i < data.NB_MAX_IMAGE; i++)
        if(data.image[i].used == 1)
        {
            IDarray[imcnt] = i;
            imcnt++;
        }

	EXECUTE_SYSTEM_COMMAND("mkdir -p %s", dirname);

    // create array for each image
    for(i = 0; i < imcnt; i++)
    {
        ID = IDarray[i];
        sprintf(imnamecp, "%s_cp", data.image[ID].name);
        //printf("image %s\n", data.image[ID].name);
        IDarraycp[i] = copy_image_ID(data.image[ID].name, imnamecp, 0);
    }

    list_image_ID();

    for(i = 0; i < imcnt; i++)
    {
        ID = IDarray[i];
        sprintf(imnamecp, "%s_cp", data.image[ID].name);
        sprintf(fnamecp, "!./%s/%s.fits", dirname, data.image[ID].name);
        save_fits(imnamecp, fnamecp);
    }

    free(IDarray);
    free(IDarraycp);


    return RETURN_SUCCESS;
}



//
// save all current images/stream onto file
// only saves 2D float streams into 3D cubes
//
errno_t COREMOD_MEMORY_SaveAll_sequ(
    const char *dirname,
    const char *IDtrig_name,
    long semtrig,
    long NBframes
)
{
    long *IDarray;
    long *IDarrayout;
    long i;
    long imcnt = 0;
    char imnameout[200];
    char fnameout[500];
    imageID ID;
    imageID IDtrig;

    long frame = 0;
    char *ptr0;
    char *ptr1;
    uint32_t *imsizearray;




    for(i = 0; i < data.NB_MAX_IMAGE; i++)
        if(data.image[i].used == 1)
        {
            imcnt++;
        }

    IDarray = (imageID *) malloc(sizeof(imageID) * imcnt);
    IDarrayout = (imageID *) malloc(sizeof(imageID) * imcnt);

    imcnt = 0;
    for(i = 0; i < data.NB_MAX_IMAGE; i++)
        if(data.image[i].used == 1)
        {
            IDarray[imcnt] = i;
            imcnt++;
        }
    imsizearray = (uint32_t *) malloc(sizeof(uint32_t) * imcnt);



    EXECUTE_SYSTEM_COMMAND("mkdir -p %s", dirname);

    IDtrig = image_ID(IDtrig_name);


    printf("Creating arrays\n");
    fflush(stdout);

    // create 3D arrays
    for(i = 0; i < imcnt; i++)
    {
        sprintf(imnameout, "%s_out", data.image[IDarray[i]].name);
        imsizearray[i] = sizeof(float) * data.image[IDarray[i]].md[0].size[0] *
                         data.image[IDarray[i]].md[0].size[1];
        printf("Creating image %s  size %d x %d x %ld\n", imnameout,
               data.image[IDarray[i]].md[0].size[0], data.image[IDarray[i]].md[0].size[1],
               NBframes);
        fflush(stdout);
        IDarrayout[i] = create_3Dimage_ID(imnameout,
                                          data.image[IDarray[i]].md[0].size[0], data.image[IDarray[i]].md[0].size[1],
                                          NBframes);
    }
    list_image_ID();

    printf("filling arrays\n");
    fflush(stdout);

    // drive semaphore to zero
    while(sem_trywait(data.image[IDtrig].semptr[semtrig]) == 0) {}

    frame = 0;
    while(frame < NBframes)
    {
        sem_wait(data.image[IDtrig].semptr[semtrig]);
        for(i = 0; i < imcnt; i++)
        {
            ID = IDarray[i];
            ptr0 = (char *) data.image[IDarrayout[i]].array.F;
            ptr1 = ptr0 + imsizearray[i] * frame;
            memcpy(ptr1, data.image[ID].array.F, imsizearray[i]);
        }
        frame++;
    }


    printf("Saving images\n");
    fflush(stdout);

    list_image_ID();


    for(i = 0; i < imcnt; i++)
    {
        ID = IDarray[i];
        sprintf(imnameout, "%s_out", data.image[ID].name);
        sprintf(fnameout, "!./%s/%s_out.fits", dirname, data.image[ID].name);
        save_fits(imnameout, fnameout);
    }

    free(IDarray);
    free(IDarrayout);
    free(imsizearray);

    return RETURN_SUCCESS;
}






errno_t COREMOD_MEMORY_testfunction_semaphore(
    const char *IDname,
    int         semtrig,
    int         testmode
)
{
    imageID ID;
    int     semval;
    int     rv;
    long    loopcnt = 0;

    ID = image_ID(IDname);

    char pinfomsg[200];


    // ===========================
    // Start loop
    // ===========================
    int loopOK = 1;
    while(loopOK == 1)
    {
        printf("\n");
        usleep(500);

        sem_getvalue(data.image[ID].semptr[semtrig], &semval);
        sprintf(pinfomsg, "%ld TEST 0 semtrig %d  ID %ld  %d", loopcnt, semtrig, ID,
                semval);
        printf("MSG: %s\n", pinfomsg);
        fflush(stdout);

        if(testmode == 0)
        {
            rv = sem_wait(data.image[ID].semptr[semtrig]);
        }

        if(testmode == 1)
        {
            rv = sem_trywait(data.image[ID].semptr[semtrig]);
        }

        if(testmode == 2)
        {
            sem_post(data.image[ID].semptr[semtrig]);
            rv = sem_wait(data.image[ID].semptr[semtrig]);
        }

        if(rv == -1)
        {
            switch(errno)
            {

                case EINTR:
                    printf("    sem_wait call was interrupted by a signal handler\n");
                    break;

                case EINVAL:
                    printf("    not a valid semaphore\n");
                    break;

                case EAGAIN:
                    printf("    The operation could not be performed without blocking (i.e., the semaphore currently has the value zero)\n");
                    break;

                default:
                    printf("    ERROR: unknown code %d\n", rv);
                    break;
            }
        }
        else
        {
            printf("    OK\n");
        }

        sem_getvalue(data.image[ID].semptr[semtrig], &semval);
        sprintf(pinfomsg, "%ld TEST 1 semtrig %d  ID %ld  %d", loopcnt, semtrig, ID,
                semval);
        printf("MSG: %s\n", pinfomsg);
        fflush(stdout);


        loopcnt ++;
    }


    return RETURN_SUCCESS;
}










/** continuously transmits 2D image through TCP link
 * mode = 1, force counter to be used for synchronization, ignore semaphores if they exist
 */


imageID COREMOD_MEMORY_image_NETWORKtransmit(
    const char *IDname,
    const char *IPaddr,
    int         port,
    int         mode,
    int         RT_priority
)
{
    imageID    ID;
    struct     sockaddr_in sock_server;
    int        fds_client;
    int        flag = 1;
    int        result;
    unsigned long long  cnt = 0;
    long long  iter = 0;
    long       framesize; // pixel data only
    uint32_t   xsize, ysize;
    char      *ptr0; // source
    char      *ptr1; // source - offset by slice
    int        rs;
//    int        sockOK;

    //struct     sched_param schedpar;
    struct     timespec ts;
    long       scnt;
    int        semval;
    int        semr;
    int        slice, oldslice;
    int        NBslices;

    TCP_BUFFER_METADATA *frame_md;
    long       framesize1; // pixel data + metadata
    char      *buff; // transmit buffer


    int        semtrig = 6; // TODO - scan for available sem
    // IMPORTANT: do not use semtrig 0
    int        UseSem = 1;

    char       errmsg[200];

    int        TMPDEBUG = 0; // set to 1 for debugging this function


    printf("Transmit stream %s over IP %s port %d\n", IDname, IPaddr, port);
    fflush(stdout);

    DEBUG_TRACEPOINT(" ");

    if(TMPDEBUG == 1)
    {
        COREMOD_MEMORY_testfunction_semaphore(IDname, 0, 0);
    }

    // ===========================
    // processinfo support
    // ===========================
    PROCESSINFO *processinfo;

    char pinfoname[200];
    sprintf(pinfoname, "ntw-tx-%s", IDname);

    char descr[200];
    sprintf(descr, "%s->%s/%d", IDname, IPaddr, port);

    char pinfomsg[200];
    sprintf(pinfomsg, "setup");


    printf("Setup processinfo ...");
    fflush(stdout);
    processinfo = processinfo_setup(
                      pinfoname,
                      descr,    // description
                      pinfomsg,  // message on startup
                      __FUNCTION__, __FILE__, __LINE__
                  );
    printf(" done\n");
    fflush(stdout);



    // OPTIONAL SETTINGS
    processinfo->MeasureTiming = 1; // Measure timing
    processinfo->RT_priority =
        RT_priority;  // RT_priority, 0-99. Larger number = higher priority. If <0, ignore

    int loopOK = 1;

    ID = image_ID(IDname);


    printf("TMPDEBUG = %d\n", TMPDEBUG);
    fflush(stdout);


    if(TMPDEBUG == 0)
    {

        if((fds_client = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        {
            printf("ERROR creating socket\n");
            exit(0);
        }

        result = setsockopt(fds_client,            /* socket affected */
                            IPPROTO_TCP,     /* set option at TCP level */
                            TCP_NODELAY,     /* name of option */
                            (char *) &flag,  /* the cast is historical cruft */
                            sizeof(int));    /* length of option value */


        if(result < 0)
        {
            processinfo_error(processinfo, "ERROR: setsockopt() failed\n");
            loopOK = 0;
        }

        if(loopOK == 1)
        {
            memset((char *) &sock_server, 0, sizeof(sock_server));
            sock_server.sin_family = AF_INET;
            sock_server.sin_port = htons(port);
            sock_server.sin_addr.s_addr = inet_addr(IPaddr);

            if(connect(fds_client, (struct sockaddr *) &sock_server,
                       sizeof(sock_server)) < 0)
            {
                perror("Error  connect() failed ");
                printf("port = %d\n", port);
                processinfo_error(processinfo, "ERROR: connect() failed\n");
                loopOK = 0;
            }
        }

        if(loopOK == 1)
        {
            if(send(fds_client, (void *) data.image[ID].md, sizeof(IMAGE_METADATA),
                    0) != sizeof(IMAGE_METADATA))
            {
                printf("send() sent a different number of bytes than expected %ld\n",
                       sizeof(IMAGE_METADATA));
                fflush(stdout);
                processinfo_error(processinfo,
                                  "send() sent a different number of bytes than expected");
                loopOK = 0;
            }
        }


        if(loopOK == 1)
        {
            xsize = data.image[ID].md[0].size[0];
            ysize = data.image[ID].md[0].size[1];
            NBslices = 1;
            if(data.image[ID].md[0].naxis > 2)
                if(data.image[ID].md[0].size[2] > 1)
                {
                    NBslices = data.image[ID].md[0].size[2];
                }
        }

        if(loopOK == 1)
        {
            switch(data.image[ID].md[0].datatype)
            {

                case _DATATYPE_INT8:
                    framesize = SIZEOF_DATATYPE_INT8 * xsize * ysize;
                    break;
                case _DATATYPE_UINT8:
                    framesize = SIZEOF_DATATYPE_UINT8 * xsize * ysize;
                    break;

                case _DATATYPE_INT16:
                    framesize = SIZEOF_DATATYPE_INT16 * xsize * ysize;
                    break;
                case _DATATYPE_UINT16:
                    framesize = SIZEOF_DATATYPE_UINT16 * xsize * ysize;
                    break;

                case _DATATYPE_INT32:
                    framesize = SIZEOF_DATATYPE_INT32 * xsize * ysize;
                    break;
                case _DATATYPE_UINT32:
                    framesize = SIZEOF_DATATYPE_UINT32 * xsize * ysize;
                    break;

                case _DATATYPE_INT64:
                    framesize = SIZEOF_DATATYPE_INT64 * xsize * ysize;
                    break;
                case _DATATYPE_UINT64:
                    framesize = SIZEOF_DATATYPE_UINT64 * xsize * ysize;
                    break;

                case _DATATYPE_FLOAT:
                    framesize = SIZEOF_DATATYPE_FLOAT * xsize * ysize;
                    break;
                case _DATATYPE_DOUBLE:
                    framesize = SIZEOF_DATATYPE_DOUBLE * xsize * ysize;
                    break;


                default:
                    printf("ERROR: WRONG DATA TYPE\n");
                    sprintf(errmsg, "WRONG DATA TYPE data type = %d\n",
                            data.image[ID].md[0].datatype);
                    printf("data type = %d\n", data.image[ID].md[0].datatype);
                    processinfo_error(processinfo, errmsg);
                    loopOK = 0;
                    break;
            }


            printf("IMAGE FRAME SIZE = %ld\n", framesize);
            fflush(stdout);
        }

        if(loopOK == 1)
        {
            switch(data.image[ID].md[0].datatype)
            {

                case _DATATYPE_INT8:
                    ptr0 = (char *) data.image[ID].array.SI8;
                    break;
                case _DATATYPE_UINT8:
                    ptr0 = (char *) data.image[ID].array.UI8;
                    break;

                case _DATATYPE_INT16:
                    ptr0 = (char *) data.image[ID].array.SI16;
                    break;
                case _DATATYPE_UINT16:
                    ptr0 = (char *) data.image[ID].array.UI16;
                    break;

                case _DATATYPE_INT32:
                    ptr0 = (char *) data.image[ID].array.SI32;
                    break;
                case _DATATYPE_UINT32:
                    ptr0 = (char *) data.image[ID].array.UI32;
                    break;

                case _DATATYPE_INT64:
                    ptr0 = (char *) data.image[ID].array.SI64;
                    break;
                case _DATATYPE_UINT64:
                    ptr0 = (char *) data.image[ID].array.UI64;
                    break;

                case _DATATYPE_FLOAT:
                    ptr0 = (char *) data.image[ID].array.F;
                    break;
                case _DATATYPE_DOUBLE:
                    ptr0 = (char *) data.image[ID].array.D;
                    break;

                default:
                    printf("ERROR: WRONG DATA TYPE\n");
                    exit(0);
                    break;
            }




            frame_md = (TCP_BUFFER_METADATA *) malloc(sizeof(TCP_BUFFER_METADATA));
            framesize1 = framesize + sizeof(TCP_BUFFER_METADATA);
            buff = (char *) malloc(sizeof(char) * framesize1);

            oldslice = 0;
            //sockOK = 1;
            printf("sem = %d\n", data.image[ID].md[0].sem);
            fflush(stdout);
        }

        if((data.image[ID].md[0].sem == 0) || (mode == 1))
        {
            processinfo_WriteMessage(processinfo, "sync using counter");
            UseSem = 0;
        }
        else
        {
            char msgstring[200];
            sprintf(msgstring, "sync using semaphore %d", semtrig);
            processinfo_WriteMessage(processinfo, msgstring);
        }

    }
    // ===========================
    // Start loop
    // ===========================
    processinfo_loopstart(
        processinfo); // Notify processinfo that we are entering loop



    while(loopOK == 1)
    {
        loopOK = processinfo_loopstep(processinfo);


        if(TMPDEBUG == 1)
        {
            sem_getvalue(data.image[ID].semptr[semtrig], &semval);
            sprintf(pinfomsg, "%ld TEST 0 semtrig %d  ID %ld  %d", processinfo->loopcnt,
                    semtrig, ID, semval);
            printf("MSG: %s\n", pinfomsg);
            fflush(stdout);
            processinfo_WriteMessage(processinfo, pinfomsg);

            //     if(semval < 3) {
            //        usleep(2000000);
            //     }

            sem_wait(data.image[ID].semptr[semtrig]);

            sem_getvalue(data.image[ID].semptr[semtrig], &semval);
            sprintf(pinfomsg, "%ld TEST 1 semtrig %d  ID %ld  %d", processinfo->loopcnt,
                    semtrig, ID, semval);
            printf("MSG: %s\n", pinfomsg);
            fflush(stdout);
            processinfo_WriteMessage(processinfo, pinfomsg);
        }
        else
        {
            if(UseSem == 0)   // use counter
            {
                while(data.image[ID].md[0].cnt0 == cnt)   // test if new frame exists
                {
                    usleep(5);
                }
                cnt = data.image[ID].md[0].cnt0;
                semr = 0;
            }
            else
            {
                if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
                {
                    perror("clock_gettime");
                    exit(EXIT_FAILURE);
                }
                ts.tv_sec += 2;

#ifndef __MACH__
                semr = sem_timedwait(data.image[ID].semptr[semtrig], &ts);
#else
                alarm(1);  // send SIGALRM to process in 1 sec - Will force sem_wait to proceed in 1 sec
                semr = sem_wait(data.image[ID].semptr[semtrig]);
#endif

                if(iter == 0)
                {
                    processinfo_WriteMessage(processinfo, "Driving sem to 0");
                    printf("Driving semaphore to zero ... ");
                    fflush(stdout);
                    sem_getvalue(data.image[ID].semptr[semtrig], &semval);
                    int semvalcnt = semval;
                    for(scnt = 0; scnt < semvalcnt; scnt++)
                    {
                        sem_getvalue(data.image[ID].semptr[semtrig], &semval);
                        printf("sem = %d\n", semval);
                        fflush(stdout);
                        sem_trywait(data.image[ID].semptr[semtrig]);
                    }
                    printf("done\n");
                    fflush(stdout);

                    sem_getvalue(data.image[ID].semptr[semtrig], &semval);
                    printf("-> sem = %d\n", semval);
                    fflush(stdout);

                    iter++;
                }
            }


        }





        processinfo_exec_start(processinfo);
        if(processinfo_compute_status(processinfo) == 1)
        {
            if(TMPDEBUG == 0)
            {
                if(semr == 0)
                {
                    frame_md[0].cnt0 = data.image[ID].md[0].cnt0;
                    frame_md[0].cnt1 = data.image[ID].md[0].cnt1;

                    slice = data.image[ID].md[0].cnt1;
                    if(slice > oldslice + 1)
                    {
                        slice = oldslice + 1;
                    }
                    if(NBslices > 1)
                        if(oldslice == NBslices - 1)
                        {
                            slice = 0;
                        }
                    if(slice > NBslices - 1)
                    {
                        slice = 0;
                    }

                    frame_md[0].cnt1 = slice;

                    ptr1 = ptr0 + framesize *
                           slice; //data.image[ID].md[0].cnt1; // frame that was just written
                    memcpy(buff, ptr1, framesize);

                    memcpy(buff + framesize, frame_md, sizeof(TCP_BUFFER_METADATA));

                    rs = send(fds_client, buff, framesize1, 0);

                    if(rs != framesize1)
                    {
                        perror("socket send error ");
                        sprintf(errmsg,
                                "ERROR: send() sent a different number of bytes (%d) than expected %ld  %ld  %ld",
                                rs, (long) framesize, (long) framesize1, (long) sizeof(TCP_BUFFER_METADATA));
                        printf("%s\n", errmsg);
                        fflush(stdout);
                        processinfo_WriteMessage(processinfo, errmsg);
                        loopOK = 0;
                    }
                    oldslice = slice;
                }
            }

        }
        // process signals, increment loop counter
        processinfo_exec_end(processinfo);

        if((data.signal_INT == 1) || \
                (data.signal_TERM == 1) || \
                (data.signal_ABRT == 1) || \
                (data.signal_BUS == 1) || \
                (data.signal_SEGV == 1) || \
                (data.signal_HUP == 1) || \
                (data.signal_PIPE == 1))
        {
            loopOK = 0;
        }
    }
    // ==================================
    // ENDING LOOP
    // ==================================
    processinfo_cleanExit(processinfo);

    if(TMPDEBUG == 0)
    {
        free(buff);

        close(fds_client);
        printf("port %d closed\n", port);
        fflush(stdout);

        free(frame_md);
    }

    return ID;
}






/** continuously receives 2D image through TCP link
 * mode = 1, force counter to be used for synchronization, ignore semaphores if they exist
 */


imageID COREMOD_MEMORY_image_NETWORKreceive(
    int port,
    __attribute__((unused)) int mode,
    int RT_priority
)
{
    struct sockaddr_in   sock_server;
    struct sockaddr_in   sock_client;
    int                  fds_server;
    int                  fds_client;
    socklen_t            slen_client;

    int             flag = 1;
    long            recvsize;
    int             result;
    long            totsize = 0;
    int             MAXPENDING = 5;


    IMAGE_METADATA *imgmd;
    imageID         ID;
    long            framesize;
    uint32_t        xsize;
    uint32_t        ysize;
    char           *ptr0; // source
    long            NBslices;
    int             socketOpen = 1; // 0 if socket is closed
    int             semval;
    int             semnb;
    int             OKim;
    int             axis;


    imgmd = (IMAGE_METADATA *) malloc(sizeof(IMAGE_METADATA));

    TCP_BUFFER_METADATA *frame_md;
    long framesize1; // pixel data + metadata
    char *buff; // buffer



    struct sched_param schedpar;




    PROCESSINFO *processinfo;
    if(data.processinfo == 1)
    {
        // CREATE PROCESSINFO ENTRY
        // see processtools.c in module CommandLineInterface for details
        //
        char pinfoname[200];
        sprintf(pinfoname, "ntw-receive-%d", port);
        processinfo = processinfo_shm_create(pinfoname, 0);
        processinfo->loopstat = 0; // loop initialization

        strcpy(processinfo->source_FUNCTION, __FUNCTION__);
        strcpy(processinfo->source_FILE,     __FILE__);
        processinfo->source_LINE = __LINE__;

        char msgstring[200];
        sprintf(msgstring, "Waiting for input stream");
        processinfo_WriteMessage(processinfo, msgstring);
    }

    // CATCH SIGNALS

    if(sigaction(SIGTERM, &data.sigact, NULL) == -1)
    {
        printf("\ncan't catch SIGTERM\n");
    }

    if(sigaction(SIGINT, &data.sigact, NULL) == -1)
    {
        printf("\ncan't catch SIGINT\n");
    }

    if(sigaction(SIGABRT, &data.sigact, NULL) == -1)
    {
        printf("\ncan't catch SIGABRT\n");
    }

    if(sigaction(SIGBUS, &data.sigact, NULL) == -1)
    {
        printf("\ncan't catch SIGBUS\n");
    }

    if(sigaction(SIGSEGV, &data.sigact, NULL) == -1)
    {
        printf("\ncan't catch SIGSEGV\n");
    }

    if(sigaction(SIGHUP, &data.sigact, NULL) == -1)
    {
        printf("\ncan't catch SIGHUP\n");
    }

    if(sigaction(SIGPIPE, &data.sigact, NULL) == -1)
    {
        printf("\ncan't catch SIGPIPE\n");
    }






    schedpar.sched_priority = RT_priority;
#ifndef __MACH__
    if(seteuid(data.euid) != 0)     //This goes up to maximum privileges
    {
        PRINT_ERROR("seteuid error");
    }
    sched_setscheduler(0, SCHED_FIFO,
                       &schedpar); //other option is SCHED_RR, might be faster
    if(seteuid(data.ruid) != 0)     //Go back to normal privileges
    {
        PRINT_ERROR("seteuid error");
    }
#endif

    // create TCP socket
    if((fds_server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        printf("ERROR creating socket\n");
        if(data.processinfo == 1)
        {
            processinfo->loopstat = 4;
            processinfo_WriteMessage(processinfo, "ERROR creating socket");
        }
        exit(0);
    }





    memset((char *) &sock_server, 0, sizeof(sock_server));

    result = setsockopt(fds_server,            /* socket affected */
                        IPPROTO_TCP,     /* set option at TCP level */
                        TCP_NODELAY,     /* name of option */
                        (char *) &flag,  /* the cast is historical cruft */
                        sizeof(int));    /* length of option value */
    if(result < 0)
    {
        printf("ERROR setsockopt\n");
        if(data.processinfo == 1)
        {
            processinfo->loopstat = 4;
            processinfo_WriteMessage(processinfo, "ERROR socketopt");
        }
        exit(0);
    }


    sock_server.sin_family = AF_INET;
    sock_server.sin_port = htons(port);
    sock_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    if(bind(fds_server, (struct sockaddr *)&sock_server,
            sizeof(sock_server)) == -1)
    {
        char msgstring[200];

        sprintf(msgstring, "ERROR binding socket, port %d", port);
        printf("%s\n", msgstring);

        if(data.processinfo == 1)
        {
            processinfo->loopstat = 4;
            processinfo_WriteMessage(processinfo, msgstring);
        }
        exit(0);
    }


    if(listen(fds_server, MAXPENDING) < 0)
    {
        char msgstring[200];

        sprintf(msgstring, "ERROR listen socket");
        printf("%s\n", msgstring);

        if(data.processinfo == 1)
        {
            processinfo->loopstat = 4;
            processinfo_WriteMessage(processinfo, msgstring);
        }

        exit(0);
    }

//    cnt = 0;

    /* Set the size of the in-out parameter */
    slen_client = sizeof(sock_client);

    /* Wait for a client to connect */
    if((fds_client = accept(fds_server, (struct sockaddr *) &sock_client,
                            &slen_client)) == -1)
    {
        char msgstring[200];

        sprintf(msgstring, "ERROR accept socket");
        printf("%s\n", msgstring);

        if(data.processinfo == 1)
        {
            processinfo->loopstat = 4;
            processinfo_WriteMessage(processinfo, msgstring);
        }

        exit(0);
    }

    printf("Client connected\n");
    fflush(stdout);

    // listen for image metadata
    if((recvsize = recv(fds_client, imgmd, sizeof(IMAGE_METADATA),
                        MSG_WAITALL)) < 0)
    {
        char msgstring[200];

        sprintf(msgstring, "ERROR receiving image metadata");
        printf("%s\n", msgstring);

        if(data.processinfo == 1)
        {
            processinfo->loopstat = 4;
            processinfo_WriteMessage(processinfo, msgstring);
        }

        exit(0);
    }



    if(data.processinfo == 1)
    {
        char msgstring[200];
        sprintf(msgstring, "Receiving stream %s", imgmd[0].name);
        processinfo_WriteMessage(processinfo, msgstring);
    }



    // is image already in memory ?
    OKim = 0;

    ID = image_ID(imgmd[0].name);
    if(ID == -1)
    {
        // is it in shared memory ?
        ID = read_sharedmem_image(imgmd[0].name);
    }

    list_image_ID();

    if(ID == -1)
    {
        OKim = 0;
    }
    else
    {
        OKim = 1;
        if(imgmd[0].naxis != data.image[ID].md[0].naxis)
        {
            OKim = 0;
        }
        if(OKim == 1)
        {
            for(axis = 0; axis < imgmd[0].naxis; axis++)
                if(imgmd[0].size[axis] != data.image[ID].md[0].size[axis])
                {
                    OKim = 0;
                }
        }
        if(imgmd[0].datatype != data.image[ID].md[0].datatype)
        {
            OKim = 0;
        }

        if(OKim == 0)
        {
            delete_image_ID(imgmd[0].name);
            ID = -1;
        }
    }



    if(OKim == 0)
    {
        printf("IMAGE %s HAS TO BE CREATED\n", imgmd[0].name);
        ID = create_image_ID(imgmd[0].name, imgmd[0].naxis, imgmd[0].size,
                             imgmd[0].datatype, imgmd[0].shared, 0);
        printf("Created image stream %s - shared = %d\n", imgmd[0].name,
               imgmd[0].shared);
    }
    else
    {
        printf("REUSING EXISTING IMAGE %s\n", imgmd[0].name);
    }





    COREMOD_MEMORY_image_set_createsem(imgmd[0].name, IMAGE_NB_SEMAPHORE);

    xsize = data.image[ID].md[0].size[0];
    ysize = data.image[ID].md[0].size[1];
    NBslices = 1;
    if(data.image[ID].md[0].naxis > 2)
        if(data.image[ID].md[0].size[2] > 1)
        {
            NBslices = data.image[ID].md[0].size[2];
        }


    char typestring[8];

    switch(data.image[ID].md[0].datatype)
    {

        case _DATATYPE_INT8:
            framesize = SIZEOF_DATATYPE_INT8 * xsize * ysize;
            sprintf(typestring, "INT8");
            break;

        case _DATATYPE_UINT8:
            framesize = SIZEOF_DATATYPE_UINT8 * xsize * ysize;
            sprintf(typestring, "UINT8");
            break;

        case _DATATYPE_INT16:
            framesize = SIZEOF_DATATYPE_INT16 * xsize * ysize;
            sprintf(typestring, "INT16");
            break;

        case _DATATYPE_UINT16:
            framesize = SIZEOF_DATATYPE_UINT16 * xsize * ysize;
            sprintf(typestring, "UINT16");
            break;

        case _DATATYPE_INT32:
            framesize = SIZEOF_DATATYPE_INT32 * xsize * ysize;
            sprintf(typestring, "INT32");
            break;

        case _DATATYPE_UINT32:
            framesize = SIZEOF_DATATYPE_UINT32 * xsize * ysize;
            sprintf(typestring, "UINT32");
            break;

        case _DATATYPE_INT64:
            framesize = SIZEOF_DATATYPE_INT64 * xsize * ysize;
            sprintf(typestring, "INT64");
            break;

        case _DATATYPE_UINT64:
            framesize = SIZEOF_DATATYPE_UINT64 * xsize * ysize;
            sprintf(typestring, "UINT64");
            break;

        case _DATATYPE_FLOAT:
            framesize = SIZEOF_DATATYPE_FLOAT * xsize * ysize;
            sprintf(typestring, "FLOAT");
            break;

        case _DATATYPE_DOUBLE:
            framesize = SIZEOF_DATATYPE_DOUBLE * xsize * ysize;
            sprintf(typestring, "DOUBLE");
            break;

        default:
            printf("ERROR: WRONG DATA TYPE\n");
            sprintf(typestring, "ERR");
            exit(0);
            break;
    }

    printf("image frame size = %ld\n", framesize);

    switch(data.image[ID].md[0].datatype)
    {

        case _DATATYPE_INT8:
            ptr0 = (char *) data.image[ID].array.SI8;
            break;
        case _DATATYPE_UINT8:
            ptr0 = (char *) data.image[ID].array.UI8;
            break;

        case _DATATYPE_INT16:
            ptr0 = (char *) data.image[ID].array.SI16;
            break;
        case _DATATYPE_UINT16:
            ptr0 = (char *) data.image[ID].array.UI16;
            break;

        case _DATATYPE_INT32:
            ptr0 = (char *) data.image[ID].array.SI32;
            break;
        case _DATATYPE_UINT32:
            ptr0 = (char *) data.image[ID].array.UI32;
            break;

        case _DATATYPE_INT64:
            ptr0 = (char *) data.image[ID].array.SI64;
            break;
        case _DATATYPE_UINT64:
            ptr0 = (char *) data.image[ID].array.UI64;
            break;

        case _DATATYPE_FLOAT:
            ptr0 = (char *) data.image[ID].array.F;
            break;
        case _DATATYPE_DOUBLE:
            ptr0 = (char *) data.image[ID].array.D;
            break;

        default:
            printf("ERROR: WRONG DATA TYPE\n");
            exit(0);
            break;
    }



    if(data.processinfo == 1)
    {
        char msgstring[200];
        sprintf(msgstring, "<- %s [%d x %d x %ld] %s", imgmd[0].name, (int) xsize,
                (int) ysize, NBslices, typestring);
        sprintf(processinfo->description, "%s %dx%dx%ld %s", imgmd[0].name, (int) xsize,
                (int) ysize, NBslices, typestring);
        processinfo_WriteMessage(processinfo, msgstring);
    }



    // this line is not needed, as frame_md is declared below
    // frame_md = (TCP_BUFFER_METADATA*) malloc(sizeof(TCP_BUFFER_METADATA));

    framesize1 = framesize + sizeof(TCP_BUFFER_METADATA);
    buff = (char *) malloc(sizeof(char) * framesize1);

    frame_md = (TCP_BUFFER_METADATA *)(buff + framesize);



    if(data.processinfo == 1)
    {
        processinfo->loopstat = 1;    //notify processinfo that we are entering loop
    }

    socketOpen = 1;
    long loopcnt = 0;
    int loopOK = 1;

    while(loopOK == 1)
    {
        if(data.processinfo == 1)
        {
            while(processinfo->CTRLval == 1)   // pause
            {
                usleep(50);
            }

            if(processinfo->CTRLval == 2)   // single iteration
            {
                processinfo->CTRLval = 1;
            }

            if(processinfo->CTRLval == 3)   // exit loop
            {
                loopOK = 0;
            }
        }


        if((recvsize = recv(fds_client, buff, framesize1, MSG_WAITALL)) < 0)
        {
            printf("ERROR recv()\n");
            socketOpen = 0;
        }


        if((data.processinfo == 1) && (processinfo->MeasureTiming == 1))
        {
            processinfo_exec_start(processinfo);
        }

        if(recvsize != 0)
        {
            totsize += recvsize;
        }
        else
        {
            socketOpen = 0;
        }

        if(socketOpen == 1)
        {
            frame_md = (TCP_BUFFER_METADATA *)(buff + framesize);


            data.image[ID].md[0].cnt1 = frame_md[0].cnt1;


            if(NBslices > 1)
            {
                memcpy(ptr0 + framesize * frame_md[0].cnt1, buff, framesize);
            }
            else
            {
                memcpy(ptr0, buff, framesize);
            }

            data.image[ID].md[0].cnt0++;
            for(semnb = 0; semnb < data.image[ID].md[0].sem ; semnb++)
            {
                sem_getvalue(data.image[ID].semptr[semnb], &semval);
                if(semval < SEMAPHORE_MAXVAL)
                {
                    sem_post(data.image[ID].semptr[semnb]);
                }
            }

            sem_getvalue(data.image[ID].semlog, &semval);
            if(semval < 2)
            {
                sem_post(data.image[ID].semlog);
            }

        }

        if(socketOpen == 0)
        {
            loopOK = 0;
        }

        if((data.processinfo == 1) && (processinfo->MeasureTiming == 1))
        {
            processinfo_exec_end(processinfo);
        }


        // process signals

        if(data.signal_TERM == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                processinfo_SIGexit(processinfo, SIGTERM);
            }
        }

        if(data.signal_INT == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                processinfo_SIGexit(processinfo, SIGINT);
            }
        }

        if(data.signal_ABRT == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                processinfo_SIGexit(processinfo, SIGABRT);
            }
        }

        if(data.signal_BUS == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                processinfo_SIGexit(processinfo, SIGBUS);
            }
        }

        if(data.signal_SEGV == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                processinfo_SIGexit(processinfo, SIGSEGV);
            }
        }

        if(data.signal_HUP == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                processinfo_SIGexit(processinfo, SIGHUP);
            }
        }

        if(data.signal_PIPE == 1)
        {
            loopOK = 0;
            if(data.processinfo == 1)
            {
                processinfo_SIGexit(processinfo, SIGPIPE);
            }
        }

        loopcnt++;
        if(data.processinfo == 1)
        {
            processinfo->loopcnt = loopcnt;
        }
    }

    if(data.processinfo == 1)
    {
        processinfo_cleanExit(processinfo);
    }


    free(buff);

    close(fds_client);

    printf("port %d closed\n", port);
    fflush(stdout);

    free(imgmd);


    return ID;
}






//
// pixel decode for unsigned short
// sem0, cnt0 gets updated at each full frame
// sem1 gets updated for each slice
// cnt1 contains the slice index that was just written
//
imageID COREMOD_MEMORY_PixMapDecode_U(
    const char *inputstream_name,
    uint32_t    xsizeim,
    uint32_t    ysizeim,
    const char *NBpix_fname,
    const char *IDmap_name,
    const char *IDout_name,
    const char *IDout_pixslice_fname
)
{
    imageID   IDout = -1;
    imageID   IDin;
    imageID   IDmap;
    long      slice, sliceii;
    long      oldslice = 0;
    long      NBslice;
    long     *nbpixslice;
    uint32_t  xsizein, ysizein;
    FILE     *fp;
    uint32_t *sizearray;
    imageID   IDout_pixslice;
    long      ii;
    unsigned long long      cnt = 0;
    //    int RT_priority = 80; //any number from 0-99

    //    struct sched_param schedpar;
    struct timespec ts;
    long scnt;
    int semval;
    //    long long iter;
    //    int r;
    long tmpl0, tmpl1;
    int semr;

    double *dtarray;
    struct timespec *tarray;
//    long slice1;


    PROCESSINFO *processinfo;

    IDin = image_ID(inputstream_name);
    IDmap = image_ID(IDmap_name);

    xsizein = data.image[IDin].md[0].size[0];
    ysizein = data.image[IDin].md[0].size[1];
    NBslice = data.image[IDin].md[0].size[2];

    char pinfoname[200];  // short name for the processinfo instance
    sprintf(pinfoname, "decode-%s-to-%s", inputstream_name, IDout_name);
    char pinfodescr[200];
    sprintf(pinfodescr, "%ldx%ldx%ld->%ldx%ld", (long) xsizein, (long) ysizein,
            NBslice, (long) xsizeim, (long) ysizeim);
    char msgstring[200];
    sprintf(msgstring, "%s->%s", inputstream_name, IDout_name);

    processinfo = processinfo_setup(
                      pinfoname,             // short name for the processinfo instance, no spaces, no dot, name should be human-readable
                      pinfodescr,    // description
                      msgstring,  // message on startup
                      __FUNCTION__, __FILE__, __LINE__
                  );
    // OPTIONAL SETTINGS
    processinfo->MeasureTiming = 1; // Measure timing
    processinfo->RT_priority =
        20;  // RT_priority, 0-99. Larger number = higher priority. If <0, ignore


    int loopOK = 1;

    processinfo_WriteMessage(processinfo, "Allocating memory");

    sizearray = (uint32_t *) malloc(sizeof(uint32_t) * 3);

    int in_semwaitindex = ImageStreamIO_getsemwaitindex(&data.image[IDin], 0);

    if(xsizein != data.image[IDmap].md[0].size[0])
    {
        printf("ERROR: xsize for %s (%d) does not match xsize for %s (%d)\n",
               inputstream_name, xsizein, IDmap_name, data.image[IDmap].md[0].size[0]);
        exit(0);
    }
    if(ysizein != data.image[IDmap].md[0].size[1])
    {
        printf("ERROR: xsize for %s (%d) does not match xsize for %s (%d)\n",
               inputstream_name, ysizein, IDmap_name, data.image[IDmap].md[0].size[1]);
        exit(0);
    }
    sizearray[0] = xsizeim;
    sizearray[1] = ysizeim;
    IDout = create_image_ID(IDout_name, 2, sizearray,
                            data.image[IDin].md[0].datatype, 1, 0);
    COREMOD_MEMORY_image_set_createsem(IDout_name, IMAGE_NB_SEMAPHORE);
    IDout_pixslice = create_image_ID("outpixsl", 2, sizearray, _DATATYPE_UINT16, 0,
                                     0);



    dtarray = (double *) malloc(sizeof(double) * NBslice);
    tarray = (struct timespec *) malloc(sizeof(struct timespec) * NBslice);


    nbpixslice = (long *) malloc(sizeof(long) * NBslice);
    if((fp = fopen(NBpix_fname, "r")) == NULL)
    {
        printf("ERROR : cannot open file \"%s\"\n", NBpix_fname);
        exit(0);
    }

    for(slice = 0; slice < NBslice; slice++)
    {
        int fscanfcnt = fscanf(fp, "%ld %ld %ld\n", &tmpl0, &nbpixslice[slice], &tmpl1);
        if(fscanfcnt == EOF)
        {
            if(ferror(fp))
            {
                perror("fscanf");
            }
            else
            {
                fprintf(stderr,
                        "Error: fscanf reached end of file, no matching characters, no matching failure\n");
            }
            return RETURN_FAILURE;
        }
        else if(fscanfcnt != 3)
        {
            fprintf(stderr,
                    "Error: fscanf successfully matched and assigned %i input items, 2 expected\n",
                    fscanfcnt);
            return RETURN_FAILURE;
        }


    }
    fclose(fp);

    for(slice = 0; slice < NBslice; slice++)
    {
        printf("Slice %5ld   : %5ld pix\n", slice, nbpixslice[slice]);
    }




    for(slice = 0; slice < NBslice; slice++)
    {
        sliceii = slice * data.image[IDmap].md[0].size[0] *
                  data.image[IDmap].md[0].size[1];
        for(ii = 0; ii < nbpixslice[slice]; ii++)
        {
            data.image[IDout_pixslice].array.UI16[ data.image[IDmap].array.UI16[sliceii +
                                                   ii] ] = (unsigned short) slice;
        }
    }

    save_fits("outpixsl", IDout_pixslice_fname);
    delete_image_ID("outpixsl");

    /*
        if(sigaction(SIGTERM, &data.sigact, NULL) == -1) {
            printf("\ncan't catch SIGTERM\n");
        }

        if(sigaction(SIGINT, &data.sigact, NULL) == -1) {
            printf("\ncan't catch SIGINT\n");
        }

        if(sigaction(SIGABRT, &data.sigact, NULL) == -1) {
            printf("\ncan't catch SIGABRT\n");
        }

        if(sigaction(SIGBUS, &data.sigact, NULL) == -1) {
            printf("\ncan't catch SIGBUS\n");
        }

        if(sigaction(SIGSEGV, &data.sigact, NULL) == -1) {
            printf("\ncan't catch SIGSEGV\n");
        }

        if(sigaction(SIGHUP, &data.sigact, NULL) == -1) {
            printf("\ncan't catch SIGHUP\n");
        }

        if(sigaction(SIGPIPE, &data.sigact, NULL) == -1) {
            printf("\ncan't catch SIGPIPE\n");
        }
    */



    processinfo_WriteMessage(processinfo, "Starting loop");

    // ==================================
    // STARTING LOOP
    // ==================================
    processinfo_loopstart(
        processinfo); // Notify processinfo that we are entering loop


    // long loopcnt = 0;
    while(loopOK == 1)
    {
        loopOK = processinfo_loopstep(processinfo);

        /*
                if(data.processinfo == 1) {
                    while(processinfo->CTRLval == 1) { // pause
                        usleep(50);
                    }

                    if(processinfo->CTRLval == 2) { // single iteration
                        processinfo->CTRLval = 1;
                    }

                    if(processinfo->CTRLval == 3) { // exit loop
                        loopOK = 0;
                    }
                }
        */

        if(data.image[IDin].md[0].sem == 0)
        {
            while(data.image[IDin].md[0].cnt0 == cnt)   // test if new frame exists
            {
                usleep(5);
            }
            cnt = data.image[IDin].md[0].cnt0;
        }
        else
        {
            if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
            {
                perror("clock_gettime");
                exit(EXIT_FAILURE);
            }
            ts.tv_sec += 1;
#ifndef __MACH__
            semr = ImageStreamIO_semtimedwait(&data.image[IDin], in_semwaitindex, &ts);
            //semr = sem_timedwait(data.image[IDin].semptr[0], &ts);
#else
            alarm(1);
            semr = ImageStreamIO_semwait(&data.image[IDin], in_semwaitindex);
            //semr = sem_wait(data.image[IDin].semptr[0]);
#endif

            if(processinfo->loopcnt == 0)
            {
                sem_getvalue(data.image[IDin].semptr[in_semwaitindex], &semval);
                for(scnt = 0; scnt < semval; scnt++)
                {
                    sem_trywait(data.image[IDin].semptr[in_semwaitindex]);
                }
            }
        }





        processinfo_exec_start(processinfo);

        if(processinfo_compute_status(processinfo) == 1)
        {
            if(semr == 0)
            {
                slice = data.image[IDin].md[0].cnt1;
                if(slice > oldslice + 1)
                {
                    slice = oldslice + 1;
                }

                if(oldslice == NBslice - 1)
                {
                    slice = 0;
                }

                //   clock_gettime(CLOCK_REALTIME, &tarray[slice]);
                //  dtarray[slice] = 1.0*tarray[slice].tv_sec + 1.0e-9*tarray[slice].tv_nsec;
                data.image[IDout].md[0].write = 1;

                if(slice < NBslice)
                {
                    sliceii = slice * data.image[IDmap].md[0].size[0] *
                              data.image[IDmap].md[0].size[1];
                    for(ii = 0; ii < nbpixslice[slice]; ii++)
                    {
                        data.image[IDout].array.UI16[data.image[IDmap].array.UI16[sliceii + ii] ] =
                            data.image[IDin].array.UI16[sliceii + ii];
                    }
                }
                //     printf("[%ld] ", slice); //TEST

                if(slice == NBslice - 1)   //if(slice<oldslice)
                {
                    COREMOD_MEMORY_image_set_sempost_byID(IDout, -1);


                    data.image[IDout].md[0].cnt0 ++;

                    //     printf("[[ Timimg [us] :   ");
                    //  for(slice1=1;slice1<NBslice;slice1++)
                    //      {
                    //              dtarray[slice1] -= dtarray[0];
                    //           printf("%6ld ", (long) (1.0e6*dtarray[slice1]));
                    //      }
                    // printf("]]");
                    //  printf("\n");//TEST
                    // fflush(stdout);
                }

                data.image[IDout].md[0].cnt1 = slice;

                sem_getvalue(data.image[IDout].semptr[2], &semval);
                if(semval < SEMAPHORE_MAXVAL)
                {
                    sem_post(data.image[IDout].semptr[2]);
                }

                sem_getvalue(data.image[IDout].semptr[3], &semval);
                if(semval < SEMAPHORE_MAXVAL)
                {
                    sem_post(data.image[IDout].semptr[3]);
                }

                data.image[IDout].md[0].write = 0;

                oldslice = slice;
            }
        }


        processinfo_exec_end(processinfo);


        // process signals
        /*
                if(data.signal_TERM == 1) {
                    loopOK = 0;
                    if(data.processinfo == 1) {
                        processinfo_SIGexit(processinfo, SIGTERM);
                    }
                }

                if(data.signal_INT == 1) {
                    loopOK = 0;
                    if(data.processinfo == 1) {
                        processinfo_SIGexit(processinfo, SIGINT);
                    }
                }

                if(data.signal_ABRT == 1) {
                    loopOK = 0;
                    if(data.processinfo == 1) {
                        processinfo_SIGexit(processinfo, SIGABRT);
                    }
                }

                if(data.signal_BUS == 1) {
                    loopOK = 0;
                    if(data.processinfo == 1) {
                        processinfo_SIGexit(processinfo, SIGBUS);
                    }
                }

                if(data.signal_SEGV == 1) {
                    loopOK = 0;
                    if(data.processinfo == 1) {
                        processinfo_SIGexit(processinfo, SIGSEGV);
                    }
                }

                if(data.signal_HUP == 1) {
                    loopOK = 0;
                    if(data.processinfo == 1) {
                        processinfo_SIGexit(processinfo, SIGHUP);
                    }
                }

                if(data.signal_PIPE == 1) {
                    loopOK = 0;
                    if(data.processinfo == 1) {
                        processinfo_SIGexit(processinfo, SIGPIPE);
                    }
                }

                loopcnt++;
                if(data.processinfo == 1) {
                    processinfo->loopcnt = loopcnt;
                }

                //    if((data.signal_INT == 1)||(data.signal_TERM == 1)||(data.signal_ABRT==1)||(data.signal_BUS==1)||(data.signal_SEGV==1)||(data.signal_HUP==1)||(data.signal_PIPE==1))
                //        loopOK = 0;

                //iter++;
                */
    }

    // ==================================
    // ENDING LOOP
    // ==================================
    processinfo_cleanExit(processinfo);

    /*    if((data.processinfo == 1) && (processinfo->loopstat != 4)) {
            processinfo_cleanExit(processinfo);
        }*/

    free(nbpixslice);
    free(sizearray);
    free(dtarray);
    free(tarray);

    return IDout;
}














/* =============================================================================================== */
/* =============================================================================================== */
/*                                                                                                 */
/* 14. DATA LOGGING                                                                                */
/*                                                                                                 */
/* =============================================================================================== */
/* =============================================================================================== */



/// creates logshimconf shared memory and loads it
LOGSHIM_CONF *COREMOD_MEMORY_logshim_create_SHMconf(
    const char *logshimname
)
{
    int             SM_fd;
    size_t          sharedsize = 0; // shared memory size in bytes
    char            SM_fname[200];
    int             result;
    LOGSHIM_CONF   *map;

    sharedsize = sizeof(LOGSHIM_CONF);

    sprintf(SM_fname, "%s/%s.logshimconf.shm", data.shmdir, logshimname);

    SM_fd = open(SM_fname, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    if(SM_fd == -1)
    {
        printf("File \"%s\"\n", SM_fname);
        fflush(stdout);
        perror("Error opening file for writing");
        exit(0);
    }

    result = lseek(SM_fd, sharedsize - 1, SEEK_SET);
    if(result == -1)
    {
        close(SM_fd);
        PRINT_ERROR("Error calling lseek() to 'stretch' the file");
        exit(0);
    }

    result = write(SM_fd, "", 1);
    if(result != 1)
    {
        close(SM_fd);
        perror("Error writing last byte of the file");
        exit(0);
    }

    map = (LOGSHIM_CONF *) mmap(0, sharedsize, PROT_READ | PROT_WRITE, MAP_SHARED,
                                SM_fd, 0);
    if(map == MAP_FAILED)
    {
        close(SM_fd);
        perror("Error mmapping the file");
        exit(0);
    }

    map[0].on = 0;
    map[0].cnt = 0;
    map[0].filecnt = 0;
    map[0].interval = 1;
    map[0].logexit = 0;
    strcpy(map[0].fname, SM_fname);

    return map;
}



// IDname is name of image logged
errno_t COREMOD_MEMORY_logshim_printstatus(
    const char *IDname
)
{
    LOGSHIM_CONF *map;
    char          SM_fname[200];
    int           SM_fd;
    struct        stat file_stat;

    // read shared mem
    sprintf(SM_fname, "%s/%s.logshimconf.shm", data.shmdir, IDname);
    printf("Importing mmap file \"%s\"\n", SM_fname);

    SM_fd = open(SM_fname, O_RDWR);
    if(SM_fd == -1)
    {
        printf("Cannot import file - continuing\n");
        exit(0);
    }
    else
    {
        fstat(SM_fd, &file_stat);
        printf("File %s size: %zd\n", SM_fname, file_stat.st_size);

        map = (LOGSHIM_CONF *) mmap(0, file_stat.st_size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, SM_fd, 0);
        if(map == MAP_FAILED)
        {
            close(SM_fd);
            perror("Error mmapping the file");
            exit(0);
        }

        printf("LOG   on = %d\n", map[0].on);
        printf("    cnt  = %lld\n", map[0].cnt);
        printf(" filecnt = %lld\n", map[0].filecnt);
        printf("interval = %ld\n", map[0].interval);
        printf("logexit  = %d\n", map[0].logexit);

        if(munmap(map, sizeof(LOGSHIM_CONF)) == -1)
        {
            printf("unmapping %s\n", SM_fname);
            perror("Error un-mmapping the file");
        }
        close(SM_fd);
    }
    return RETURN_SUCCESS;
}
















// set the on field in logshim
// IDname is name of image logged
errno_t COREMOD_MEMORY_logshim_set_on(
    const char *IDname,
    int         setv
)
{
    LOGSHIM_CONF  *map;
    char           SM_fname[200];
    int            SM_fd;
    struct stat    file_stat;

    // read shared mem
    sprintf(SM_fname, "%s/%s.logshimconf.shm", data.shmdir, IDname);
    printf("Importing mmap file \"%s\"\n", SM_fname);

    SM_fd = open(SM_fname, O_RDWR);
    if(SM_fd == -1)
    {
        printf("Cannot import file - continuing\n");
        exit(0);
    }
    else
    {
        fstat(SM_fd, &file_stat);
        printf("File %s size: %zd\n", SM_fname, file_stat.st_size);

        map = (LOGSHIM_CONF *) mmap(0, file_stat.st_size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, SM_fd, 0);
        if(map == MAP_FAILED)
        {
            close(SM_fd);
            perror("Error mmapping the file");
            exit(0);
        }

        map[0].on = setv;

        if(munmap(map, sizeof(LOGSHIM_CONF)) == -1)
        {
            printf("unmapping %s\n", SM_fname);
            perror("Error un-mmapping the file");
        }
        close(SM_fd);
    }
    return RETURN_SUCCESS;
}



// set the on field in logshim
// IDname is name of image logged
errno_t COREMOD_MEMORY_logshim_set_logexit(
    const char *IDname,
    int         setv
)
{
    LOGSHIM_CONF  *map;
    char           SM_fname[200];
    int            SM_fd;
    struct stat    file_stat;

    // read shared mem
    sprintf(SM_fname, "%s/%s.logshimconf.shm", data.shmdir, IDname);
    printf("Importing mmap file \"%s\"\n", SM_fname);

    SM_fd = open(SM_fname, O_RDWR);
    if(SM_fd == -1)
    {
        printf("Cannot import file - continuing\n");
        exit(0);
    }
    else
    {
        fstat(SM_fd, &file_stat);
        printf("File %s size: %zd\n", SM_fname, file_stat.st_size);

        map = (LOGSHIM_CONF *) mmap(0, file_stat.st_size, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, SM_fd, 0);
        if(map == MAP_FAILED)
        {
            close(SM_fd);
            perror("Error mmapping the file");
            exit(0);
        }

        map[0].logexit = setv;

        if(munmap(map, sizeof(LOGSHIM_CONF)) == -1)
        {
            printf("unmapping %s\n", SM_fname);
            perror("Error un-mmapping the file");
        }
        close(SM_fd);
    }
    return RETURN_SUCCESS;
}





/** logs a shared memory stream onto disk
 * uses semlog semaphore
 *
 * uses data cube buffer to store frames
 * if an image name logdata exists (should ideally be in shared mem), then this will be included in the timing txt file
 */
errno_t __attribute__((hot)) COREMOD_MEMORY_sharedMem_2Dim_log(
    const char *IDname,
    uint32_t    zsize,
    const char *logdir,
    const char *IDlogdata_name
)
{
    // WAIT time. If no new frame during this time, save existing cube
    int        WaitSec = 5;

    imageID    ID;
    uint32_t   xsize;
    uint32_t   ysize;
//    long       ii;
//    long       i;
    imageID    IDb;
    imageID    IDb0;
    imageID    IDb1;
    long       index = 0;
    unsigned long long       cnt = 0;
    int        buffer;
    uint8_t    datatype;
    uint32_t  *imsizearray;
    char       fname[200];
    char       iname[200];

    time_t          t;
//    struct tm      *uttime;
    struct tm      *uttimeStart;
    struct timespec ts;
    struct timespec timenow;
    struct timespec timenowStart;
//    long            kw;
    int             ret;
    imageID         IDlogdata;

    char *ptr0_0; // source image data
    char *ptr1_0; // destination image data
    char *ptr0; // source image data, after offset
    char *ptr1; // destination image data, after offset

    long framesize; // in bytes

//	char *arrayindex_ptr;
//    char *arraytime_ptr;
//    char *arraycnt0_ptr;
//    char *arraycnt1_ptr;

//    FILE *fp;
    char fnameascii[200];

    pthread_t  thread_savefits;
    int        tOK = 0;
    int        iret_savefits;
    //	char tmessage[500];
    //struct savethreadmsg *tmsg = malloc(sizeof(struct savethreadmsg));
    STREAMSAVE_THREAD_MESSAGE *tmsg = malloc(sizeof(STREAMSAVE_THREAD_MESSAGE));


//    long fnb = 0;
    long NBfiles = -1; // run forever

    long long cntwait;
    long waitdelayus = 50;  // max speed = 20 kHz
    long long cntwaitlim = 10000; // 5 sec
    int wOK;
    int noframe;


    char logb0name[500];
    char logb1name[500];

    int is3Dcube = 0; // this is a rolling buffer
//    int exitflag = 0; // toggles to 1 when loop must exit

    LOGSHIM_CONF *logshimconf;

    // recording time for each frame
    double *array_time;
    double *array_time_cp;

    // counters
    uint64_t *array_cnt0;
    uint64_t *array_cnt0_cp;
    uint64_t *array_cnt1;
    uint64_t *array_cnt1_cp;

    int RT_priority = 80; //any number from 0-99
    struct sched_param schedpar;

    int use_semlog;
    int semval;


    int VERBOSE = 1;
    // 0: don't print
    // 1: print statements outside fast loop
    // 2: print everything

    // convert wait time into number of couunter steps (counter mode only)
    cntwaitlim = (long)(WaitSec * 1000000 / waitdelayus);



    schedpar.sched_priority = RT_priority;
#ifndef __MACH__
    if(seteuid(data.euid) != 0)     //This goes up to maximum privileges
    {
        PRINT_ERROR("seteuid error");
    }
    sched_setscheduler(0, SCHED_FIFO,
                       &schedpar); //other option is SCHED_RR, might be faster
    if(seteuid(data.ruid) != 0)     //Go back to normal privileges
    {
        PRINT_ERROR("seteuid error");
    }
#endif



    IDlogdata = image_ID(IDlogdata_name);
    if(IDlogdata != -1)
    {
        if(data.image[IDlogdata].md[0].datatype != _DATATYPE_FLOAT)
        {
            IDlogdata = -1;
        }
    }
    printf("log data name = %s\n", IDlogdata_name);


    logshimconf = COREMOD_MEMORY_logshim_create_SHMconf(IDname);


    logshimconf[0].on = 1;
    logshimconf[0].cnt = 0;
    logshimconf[0].filecnt = 0;
    logshimconf[0].logexit = 0;
    logshimconf[0].interval = 1;



    imsizearray = (uint32_t *) malloc(sizeof(uint32_t) * 3);



    read_sharedmem_image(IDname);
    ID = image_ID(IDname);
    datatype = data.image[ID].md[0].datatype;
    xsize = data.image[ID].md[0].size[0];
    ysize = data.image[ID].md[0].size[1];

    if(data.image[ID].md[0].naxis == 3)
    {
        is3Dcube = 1;
    }

    /** create the 2 buffers */

    imsizearray[0] = xsize;
    imsizearray[1] = ysize;
    imsizearray[2] = zsize;

    sprintf(logb0name, "%s_logbuff0", IDname);
    sprintf(logb1name, "%s_logbuff1", IDname);

    IDb0 = create_image_ID(logb0name, 3, imsizearray, datatype, 1, 1);
    IDb1 = create_image_ID(logb1name, 3, imsizearray, datatype, 1, 1);
    COREMOD_MEMORY_image_set_semflush(logb0name, -1);
    COREMOD_MEMORY_image_set_semflush(logb1name, -1);


    array_time = (double *) malloc(sizeof(double) * zsize);
    array_cnt0 = (uint64_t *) malloc(sizeof(uint64_t) * zsize);
    array_cnt1 = (uint64_t *) malloc(sizeof(uint64_t) * zsize);

    array_time_cp = (double *) malloc(sizeof(double) * zsize);
    array_cnt0_cp = (uint64_t *) malloc(sizeof(uint64_t) * zsize);
    array_cnt1_cp = (uint64_t *) malloc(sizeof(uint64_t) * zsize);


    IDb = IDb0;

    switch(datatype)
    {

        case _DATATYPE_FLOAT:
            framesize = SIZEOF_DATATYPE_FLOAT * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.F;
            break;

        case _DATATYPE_INT8:
            framesize = SIZEOF_DATATYPE_INT8 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.SI8;
            break;

        case _DATATYPE_UINT8:
            framesize = SIZEOF_DATATYPE_UINT8 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.UI8;
            break;

        case _DATATYPE_INT16:
            framesize = SIZEOF_DATATYPE_INT16 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.SI16;
            break;

        case _DATATYPE_UINT16:
            framesize = SIZEOF_DATATYPE_UINT16 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.UI16;
            break;

        case _DATATYPE_INT32:
            framesize = SIZEOF_DATATYPE_INT32 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.SI32;
            break;

        case _DATATYPE_UINT32:
            framesize = SIZEOF_DATATYPE_UINT32 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.UI32;
            break;

        case _DATATYPE_INT64:
            framesize = SIZEOF_DATATYPE_INT64 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.SI64;
            break;

        case _DATATYPE_UINT64:
            framesize = SIZEOF_DATATYPE_UINT64 * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.UI64;
            break;


        case _DATATYPE_DOUBLE:
            framesize = SIZEOF_DATATYPE_DOUBLE * xsize * ysize;
            ptr0_0 = (char *) data.image[ID].array.D;
            break;

        default:
            printf("ERROR: WRONG DATA TYPE\n");
            exit(0);
            break;
    }



    switch(datatype)
    {

        case _DATATYPE_FLOAT:
            ptr1_0 = (char *) data.image[IDb].array.F;
            break;

        case _DATATYPE_INT8:
            ptr1_0 = (char *) data.image[IDb].array.SI8;
            break;

        case _DATATYPE_UINT8:
            ptr1_0 = (char *) data.image[IDb].array.UI8;
            break;

        case _DATATYPE_INT16:
            ptr1_0 = (char *) data.image[IDb].array.SI16;
            break;

        case _DATATYPE_UINT16:
            ptr1_0 = (char *) data.image[IDb].array.UI16;
            break;

        case _DATATYPE_INT32:
            ptr1_0 = (char *) data.image[IDb].array.SI32;
            break;

        case _DATATYPE_UINT32:
            ptr1_0 = (char *) data.image[IDb].array.UI32;
            break;

        case _DATATYPE_INT64:
            ptr1_0 = (char *) data.image[IDb].array.SI64;
            break;

        case _DATATYPE_UINT64:
            ptr1_0 = (char *) data.image[IDb].array.UI64;
            break;

        case _DATATYPE_DOUBLE:
            ptr1_0 = (char *) data.image[IDb].array.D;
            break;

    }




    cnt = data.image[ID].md[0].cnt0 - 1;

    buffer = 0;
    index = 0;

    printf("logdata ID = %ld\n", IDlogdata);
    list_image_ID();

    // exitflag = 0;


    // using semlog ?
    use_semlog = 0;
    if(data.image[ID].semlog != NULL)
    {
        use_semlog = 1;
        sem_getvalue(data.image[ID].semlog, &semval);

        // bring semaphore value to 1 to only save 1 frame
        while(semval > 1)
        {
            sem_wait(data.image[ID].semlog);
            sem_getvalue(data.image[ID].semlog, &semval);
        }
        if(semval == 0)
        {
            sem_post(data.image[ID].semlog);
        }
    }



    while((logshimconf[0].filecnt != NBfiles) && (logshimconf[0].logexit == 0))
    {
        int timeout; // 1 if timeout has occurred

        cntwait = 0;
        noframe = 0;
        wOK = 1;

        if(VERBOSE > 1)
        {
            printf("%5d  Entering wait loop   index = %ld %d\n", __LINE__, index, noframe);
        }

        timeout = 0;
        if(likely(use_semlog == 1))
        {
            if(VERBOSE > 1)
            {
                printf("%5d  Waiting for semaphore\n", __LINE__);
            }

            if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
            {
                perror("clock_gettime");
                exit(EXIT_FAILURE);
            }
            ts.tv_sec += WaitSec;

            ret = sem_timedwait(data.image[ID].semlog, &ts);
            if(ret == -1)
            {
                if(errno == ETIMEDOUT)
                {
                    printf("%5d  sem_timedwait() timed out (%d sec) -[index %ld]\n", __LINE__,
                           WaitSec, index);
                    if(VERBOSE > 0)
                    {
                        printf("%5d  sem time elapsed -> Save current cube [index %ld]\n", __LINE__,
                               index);
                    }

                    strcpy(tmsg->iname, iname);
                    strcpy(tmsg->fname, fname);
                    tmsg->partial = 1; // partial cube
                    tmsg->cubesize = index;

                    memcpy(array_time_cp, array_time, sizeof(double)*index);
                    memcpy(array_cnt0_cp, array_cnt0, sizeof(uint64_t)*index);
                    memcpy(array_cnt1_cp, array_cnt1, sizeof(uint64_t)*index);

                    tmsg->arrayindex = array_cnt0_cp;
                    tmsg->arraycnt0 = array_cnt0_cp;
                    tmsg->arraycnt1 = array_cnt1_cp;
                    tmsg->arraytime = array_time_cp;

                    timeout = 1;
                }
                if(errno == EINTR)
                {
                    printf("%5d  sem_timedwait [index %ld]: The call was interrupted by a signal handler\n",
                           __LINE__, index);
                }

                if(errno == EINVAL)
                {
                    printf("%5d  sem_timedwait [index %ld]: Not a valid semaphore\n", __LINE__,
                           index);
                    printf("               The value of abs_timeout.tv_nsecs is less than 0, or greater than or equal to 1000 million\n");
                }

                if(errno == EAGAIN)
                {
                    printf("%5d  sem_timedwait [index %ld]: The operation could not be performed without blocking (i.e., the semaphore currently has the value zero)\n",
                           __LINE__, index);
                }


                wOK = 0;
                if(index == 0)
                {
                    noframe = 1;
                }
                else
                {
                    noframe = 0;
                }
            }

        }
        else
        {
            if(VERBOSE > 1)
            {
                printf("%5d  Not using semaphore, watching counter\n", __LINE__);
            }

            while(((cnt == data.image[ID].md[0].cnt0) || (logshimconf[0].on == 0))
                    && (wOK == 1))
            {
                if(VERBOSE > 1)
                {
                    printf("%5d  waiting time step\n", __LINE__);
                }

                usleep(waitdelayus);
                cntwait++;

                if(VERBOSE > 1)
                {
                    printf("%5d  cntwait = %lld\n", __LINE__, cntwait);
                    fflush(stdout);
                }

                if(cntwait > cntwaitlim) // save current cube
                {
                    if(VERBOSE > 0)
                    {
                        printf("%5d  cnt time elapsed -> Save current cube\n", __LINE__);
                    }


                    strcpy(tmsg->iname, iname);
                    strcpy(tmsg->fname, fname);
                    tmsg->partial = 1; // partial cube
                    tmsg->cubesize = index;

                    memcpy(array_time_cp, array_time, sizeof(double)*index);
                    memcpy(array_cnt0_cp, array_cnt0, sizeof(uint64_t)*index);
                    memcpy(array_cnt1_cp, array_cnt1, sizeof(uint64_t)*index);

                    tmsg->arrayindex = array_cnt0_cp;
                    tmsg->arraycnt0 = array_cnt0_cp;
                    tmsg->arraycnt1 = array_cnt1_cp;
                    tmsg->arraytime = array_time_cp;

                    wOK = 0;
                    if(index == 0)
                    {
                        noframe = 1;
                    }
                    else
                    {
                        noframe = 0;
                    }
                }
            }
        }



        if(index == 0)
        {
            if(VERBOSE > 0)
            {
                printf("%5d  Setting cube start time [index %ld]\n", __LINE__, index);
            }

            /// measure time
            t = time(NULL);
            uttimeStart = gmtime(&t);
            clock_gettime(CLOCK_REALTIME, &timenowStart);

            //     sprintf(fname,"!%s/%s_%02d:%02d:%02ld.%09ld.fits", logdir, IDname, uttime->tm_hour, uttime->tm_min, timenow.tv_sec % 60, timenow.tv_nsec);
            //            sprintf(fnameascii,"%s/%s_%02d:%02d:%02ld.%09ld.txt", logdir, IDname, uttime->tm_hour, uttime->tm_min, timenow.tv_sec % 60, timenow.tv_nsec);
        }


        if(VERBOSE > 1)
        {
            printf("%5d  logshimconf[0].on = %d\n", __LINE__, logshimconf[0].on);
        }


        if(likely(logshimconf[0].on == 1))
        {
            if(likely(wOK == 1)) // normal step: a frame has arrived
            {
                if(VERBOSE > 1)
                {
                    printf("%5d  Frame has arrived [index %ld]\n", __LINE__, index);
                }

                /// measure time
                //   t = time(NULL);
                //   uttime = gmtime(&t);

                clock_gettime(CLOCK_REALTIME, &timenow);


                if(is3Dcube == 1)
                {
                    ptr0 = ptr0_0 + framesize * data.image[ID].md[0].cnt1;
                }
                else
                {
                    ptr0 = ptr0_0;
                }

                ptr1 = ptr1_0 + framesize * index;

                if(VERBOSE > 1)
                {
                    printf("%5d  memcpy framesize = %ld\n", __LINE__, framesize);
                }

                memcpy((void *) ptr1, (void *) ptr0, framesize);

                if(VERBOSE > 1)
                {
                    printf("%5d  memcpy done\n", __LINE__);
                }

                array_cnt0[index] = data.image[ID].md[0].cnt0;
                array_cnt1[index] = data.image[ID].md[0].cnt1;
                //array_time[index] = uttime->tm_hour*3600.0 + uttime->tm_min*60.0 + timenow.tv_sec % 60 + 1.0e-9*timenow.tv_nsec;
                array_time[index] = timenow.tv_sec + 1.0e-9 * timenow.tv_nsec;

                index++;
            }
        }
        else
        {
            // save partial if possible
            //if(index>0)
            wOK = 0;

        }


        if(VERBOSE > 1)
        {
            printf("%5d  index = %ld  wOK = %d\n", __LINE__, index, wOK);
        }


        // SAVE CUBE TO DISK
        /// cases:
        /// index>zsize-1  buffer full
        /// timeout==1 && index>0  : partial
        if((index > zsize - 1)  || ((timeout == 1) && (index > 0)))
        {
            long NBframemissing;

            /// save image
            if(VERBOSE > 0)
            {
                printf("%5d  Save image   [index  %ld]  [timeout %d] [zsize %ld]\n", __LINE__,
                       index, timeout, (long) zsize);
            }

            sprintf(iname, "%s_logbuff%d", IDname, buffer);
            if(buffer == 0)
            {
                IDb = IDb0;
            }
            else
            {
                IDb = IDb1;
            }


            if(VERBOSE > 0)
            {
                printf("%5d  Building file name: ascii\n", __LINE__);
                fflush(stdout);
            }

            sprintf(fnameascii, "%s/%s_%02d:%02d:%02ld.%09ld.txt", logdir, IDname,
                    uttimeStart->tm_hour, uttimeStart->tm_min, timenowStart.tv_sec % 60,
                    timenowStart.tv_nsec);


            if(VERBOSE > 0)
            {
                printf("%5d  Building file name: fits\n", __LINE__);
                fflush(stdout);
            }
            sprintf(fname, "!%s/%s_%02d:%02d:%02ld.%09ld.fits", logdir, IDname,
                    uttimeStart->tm_hour, uttimeStart->tm_min, timenowStart.tv_sec % 60,
                    timenowStart.tv_nsec);



            strcpy(tmsg->iname, iname);
            strcpy(tmsg->fname, fname);
            strcpy(tmsg->fnameascii, fnameascii);
            tmsg->saveascii = 1;



            if(wOK == 1) // full cube
            {
                tmsg->partial = 0; // full cube
                if(VERBOSE > 0)
                {
                    printf("%5d  SAVING FULL CUBE\n", __LINE__);
                    fflush(stdout);
                }

            }
            else // partial cube
            {
                tmsg->partial = 1; // partial cube
                if(VERBOSE > 0)
                {
                    printf("%5d  SAVING PARTIAL CUBE\n", __LINE__);
                    fflush(stdout);
                }
            }


            //  fclose(fp);

            // Wait for save thread to complete to launch next one
            if(tOK == 1)
            {
                if(pthread_tryjoin_np(thread_savefits, NULL) == EBUSY)
                {
                    if(VERBOSE > 0)
                    {
                        printf("%5d  PREVIOUS SAVE THREAD NOT TERMINATED -> waiting\n", __LINE__);
                    }
                    pthread_join(thread_savefits, NULL);
                    if(VERBOSE > 0)
                    {
                        printf("%5d  PREVIOUS SAVE THREAD NOW COMPLETED -> continuing\n", __LINE__);
                    }
                }
                else if(VERBOSE > 0)
                {
                    printf("%5d  PREVIOUS SAVE THREAD ALREADY COMPLETED -> OK\n", __LINE__);
                }
            }


            COREMOD_MEMORY_image_set_sempost_byID(IDb, -1);
            data.image[IDb].md[0].cnt0++;
            data.image[IDb].md[0].write = 0;


            tmsg->cubesize = index;
            strcpy(tmsg->iname, iname);
            memcpy(array_time_cp, array_time, sizeof(double)*index);
            memcpy(array_cnt0_cp, array_cnt0, sizeof(uint64_t)*index);
            memcpy(array_cnt1_cp, array_cnt1, sizeof(uint64_t)*index);

            NBframemissing = (array_cnt0[index - 1] - array_cnt0[0]) - (index - 1);

            printf("===== CUBE %8lld   Number of missed frames = %8ld  / %ld  / %8ld ====\n",
                   logshimconf[0].filecnt, NBframemissing, index, (long) zsize);

            if(VERBOSE > 0)
            {
                printf("%5d  Starting image save thread\n", __LINE__);
                fflush(stdout);
            }

            tmsg->arrayindex = array_cnt0_cp;
            tmsg->arraycnt0 = array_cnt0_cp;
            tmsg->arraycnt1 = array_cnt1_cp;
            tmsg->arraytime = array_time_cp;
            iret_savefits = pthread_create(&thread_savefits, NULL, save_fits_function,
                                           tmsg);

            logshimconf[0].cnt ++;

            tOK = 1;
            if(iret_savefits)
            {
                fprintf(stderr, "Error - pthread_create() return code: %d\n", iret_savefits);
                exit(EXIT_FAILURE);
            }

            index = 0;
            buffer++;
            if(buffer == 2)
            {
                buffer = 0;
            }
            //            printf("[%ld -> %d]", cnt, buffer);
            //           fflush(stdout);
            if(buffer == 0)
            {
                IDb = IDb0;
            }
            else
            {
                IDb = IDb1;
            }

            switch(datatype)
            {

                case _DATATYPE_FLOAT:
                    ptr1_0 = (char *) data.image[IDb].array.F;
                    break;

                case _DATATYPE_INT8:
                    ptr1_0 = (char *) data.image[IDb].array.SI8;
                    break;

                case _DATATYPE_UINT8:
                    ptr1_0 = (char *) data.image[IDb].array.UI8;
                    break;

                case _DATATYPE_INT16:
                    ptr1_0 = (char *) data.image[IDb].array.SI16;
                    break;

                case _DATATYPE_UINT16:
                    ptr1_0 = (char *) data.image[IDb].array.UI16;
                    break;

                case _DATATYPE_INT32:
                    ptr1_0 = (char *) data.image[IDb].array.SI32;
                    break;

                case _DATATYPE_UINT32:
                    ptr1_0 = (char *) data.image[IDb].array.UI32;
                    break;

                case _DATATYPE_INT64:
                    ptr1_0 = (char *) data.image[IDb].array.SI64;
                    break;

                case _DATATYPE_UINT64:
                    ptr1_0 = (char *) data.image[IDb].array.UI64;
                    break;

                case _DATATYPE_DOUBLE:
                    ptr1_0 = (char *) data.image[IDb].array.D;
                    break;

            }

            data.image[IDb].md[0].write = 1;
            logshimconf[0].filecnt ++;
        }


        cnt = data.image[ID].md[0].cnt0;
    }

    free(imsizearray);
    free(tmsg);

    free(array_time);
    free(array_cnt0);
    free(array_cnt1);

    free(array_time_cp);
    free(array_cnt0_cp);
    free(array_cnt1_cp);

    return RETURN_SUCCESS;
}














