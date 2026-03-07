/* 
 * File:   auxfunc.h
 * Author: paulmuell
 *
 * Auxiliary functions, shared between remaining functions. No state machines defined
 * for any function in this folder 
 * 
 * Created on August 22, 2015, 1:37 PM
 */

#ifndef AUXFUNC_H
#define	AUXFUNC_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "Globals.h"
#include "Defines.h"

#define MEMSIZE 16834   //length in uint16
#define MEMPAGE 64      //length in uint8;

bool readeeprom(uint16_t adr,uint16_t *buf,uint16_t len);
bool writeeeprom(uint16_t adr, uint16_t *buf, uint16_t len);
void serinval (uint8_t *chx);
void prepsoftres (void);
void inlowpower(bool forever);
int32_t sqr(uint64_t arg,uint8_t acc);
void add4bytes(volatile uint32_t src, volatile uint8_t *snk);
void writecal(uint16_t adr);
void clrmem(uint16_t adr);
bool inlim(int32_t val,int32_t minval,int32_t maxval);
void storemsg(uint8_t *dta, uint16_t trans);
uint32_t get4bytes( uint8_t *strg);
void rstrq(uint32_t rstfield);
void progmirrored(uint8_t *dta);
void prognonmirrored(uint8_t *dta);
void proginfo(uint8_t *dta);
void calvcs(uint8_t *dta);
void  mem2trans(uint8_t wrds, uint32_t adr,uint8_t *targ,bool be);
uint8_t ldid(uint8_t *dta);
bool ldlog(void);
void clearenergy(void);
void checksvn(void);
void ressrvnot(uint8_t notno);
void progsvwarn(uint8_t notno,uint8_t *txt,uint16_t *tmr);
void prog1block (uint8_t *bf);
uint16_t read1block(uint16_t blockcntr, uint8_t *bf);
void cpureset(void);
void rstrq(uint32_t rst);
void checkpwrup(void);
void writesn(void);
void wrincident(uint16_t adr, uint16_t nr);

#ifdef	__cplusplus
}
#endif

#endif	/* AUXFUNC_H */

