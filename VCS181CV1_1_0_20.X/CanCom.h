
#ifndef _CANCOM_H    /* Guard against multiple inclusion */
#define _CANCOM_H

#include <xc.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/attribs.h>
#include <sys/kmem.h>
#include "Globals.h"
#include "Defines.h"
#include "auxfunc.h"


const uint8_t unknown[]="unknown";
const uint8_t verc06[]="VCS181C006";
const uint8_t verc07[]="VCS181C007";

//maximum time assigned for a multi-frame transfer. Any transfer taking longer will be cancelled
#define MAXRXT          100
#define CANDELAY        257     //retry to gain communication after this time
#define MAXFIFOFULL     500     //max pending time for one MFR transfer
#define CANOFF          250     //bus will stay off after a fault for this time

#define RXMAX           500     //timeout for serial reception unit is ms 

#define TR101RATE       500     //update rate for transfer 101 in ms
#define TR13ARATE       60000   //update rate transfer 0x013A

uint32_t C1FiFoBuff[384];

uint8_t canprocptr;
uint32_t tr13A;  //tr13A is a timer that takes care of sending transfer 0x013A

#endif /* _CANCOM_H */

/* *****************************************************************************
 End of File
 */
