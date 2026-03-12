
#ifndef _CPUSETUP_H    /* Guard against multiple inclusion */
#define _CPUSETUP_H

#include <xc.h>
#include <stdbool.h>
#include <stdint.h>
#include "Defines.h"
#include "Globals.h"
#include "Structures.h"
#include "auxfunc.h"
//#include "BitsManager.h"

void cpu_setup(void);
bool cpu_init(void);


#endif /* _CPUSETUP_H */

/* *****************************************************************************
 End of File
 */
