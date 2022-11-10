/** \file
 *  \brief Utility functions for easier debugging from non C/C++ code
 *  \author <tine@isotel.eu>
 */

#include <stdio.h>
#include "isn_logger.h"
#include "isn_def.h"

/**
 * @brief Disable stdout buffer for immediate output
 * 
 */
void disable_stdout_buffer() {
   setvbuf(stdout, NULL, _IONBF, 0);
}

/**
 * @brief flush stdout
 * 
 */
void flush_stdout() {
    fflush(stdout);
}
