/* 
 * File:   usbcom.h
 * Author: Paulm
 *
 * Created on November 5, 2015, 11:01 AM
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "Defines.h"
#include <plib.h>
#include "Globals.h"
#include "StateDefinitions.h"

#ifndef USBCOM_H
#define	USBCOM_H

#ifdef	__cplusplus
extern "C" {
#endif

    void usbcont(void);
    void usbreset(void);


#ifdef	__cplusplus
}
#endif

#endif	/* USBCOM_H */

