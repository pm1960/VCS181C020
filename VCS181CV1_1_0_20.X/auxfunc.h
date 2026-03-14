#ifndef _AUXFUNC_H    /* Guard against multiple inclusion */
#define _AUXFUNC_H

#define MEMPAGE 0x80
#define MEMSIZE 0x10000


#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "CanFunctions.h"
#include "Defines.h"
#include "Globals.h" 
//#include "BitsManager.h"

void initextadc(void);
bool readeeprom(uint16_t adr,uint16_t *buf,uint16_t len);
bool writeeeprom(uint16_t adr, uint16_t *buf, uint16_t len);
bool checkmirr(uint16_t *bf);
bool checkasc(uint8_t *str,uint8_t len);
void validatesvns(void);
void inlowpwr(void);
void cpureset(void);
void prepsoftres(void);
void reslogmem(void);
uint32_t get4bytes( uint8_t **strg);
void logstasto(uint8_t reason, uint8_t src);
bool chkvalrtc(RTCX *cdt);
void ressrvnot(uint32_t notno);
void initsvn(void);
uint32_t newcaldat(uint16_t days,uint32_t refdat);
void writecal(uint16_t adr);
bool actcalib(bool init);
bool delaycheck(int16_t val, int16_t limval, uint32_t delay, uint32_t *tmr,
    uint16_t *stat,uint8_t bitno,bool highlim);
#endif /* _auxfunc_H */

/* *****************************************************************************
 End of File
 */
