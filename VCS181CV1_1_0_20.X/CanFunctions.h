

#ifndef _CANFUNCTIONS_H    /* Guard against multiple inclusion */
#define _CANFUNCTIONS_H

#include <xc.h>
#include <stdbool.h>
#include <stdint.h>
#include "Globals.h"

    bool initcan(void);
    void CANCOM_Tasks (void);
    bool sendrq(RQHANDLER *rqx);
    bool txtrans(uint16_t rqtrans,uint8_t *bf);

#endif /* _CANFUNCTIONS_H */

/* *****************************************************************************
 End of File
 */
