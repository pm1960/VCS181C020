/* 
 * File:   cpusetup.h
 * Author: paulmuell
 *
 * Created on September 1, 2015, 9:46 AM
 */

#ifndef CPUSETUP_H
#define	CPUSETUP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "Defines.h"
#include <stdint.h>
#include <stdbool.h>
#include <xc.h>
#include <plib.h>
#include "Globals.h"

void cpusetup(void);
void initcap(void);
bool cpuinit(void);

#ifdef	__cplusplus
}
#endif

#endif	/* CPUSETUP_H */

