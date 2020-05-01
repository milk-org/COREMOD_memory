/** @file clearall.c
 */


#include "CommandLineInterface/CLIcore.h"
#include "image_ID.h"
#include "delete_image.h"
#include "delete_variable.h"


// ==========================================
// Forward declaration(s)
// ==========================================

errno_t clearall();


// ==========================================
// Command line interface wrapper function(s)
// ==========================================





// ==========================================
// Register CLI command(s)
// ==========================================

errno_t clearall_addCLIcmd()
{

    RegisterCLIcommand(
        "rmall",
        __FILE__,
        clearall,
        "remove all images",
        "no argument",
        "rmall",
        "int clearall()");


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



