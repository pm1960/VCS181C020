
#ifndef _GLOBALS_H    /* Guard against multiple inclusion */
#define _GLOBALS_H

#include <stdint.h>
#include <stdbool.h>
#include "Defines.h"
#include "Structures.h"
//#include "Alarmmgr.h"

extern volatile uint32_t t_1ms,v_period,isr_flags;
extern volatile uint32_t i_period;
extern volatile uint16_t nocan,adchan[14],vcstemp,oiltemp;
extern volatile uint8_t adccntr,u17cntr;

extern volatile ACCHANNEL u1[2],u2[2],u3[2],i1[2],i2[2],i3[2];
extern volatile  ACCHANNEL *acchans[12];
extern volatile DCCHANNEL pfctemp[2],iso15[2];
extern volatile uint16_t isoptrs;       //indicates which channel to process in ISR. Bits 0..5 indicate which pointer from achchptr to use, bits 6..7 refer to dcchptr. bits 8.. refer to second row of acchptr, bits 14 and 15 to second row of dcchptr
extern volatile ACCHANNEL *acchptr[2][6];  //x cordinate points to chanel number, y coordinate points to buffer number
extern volatile DCCHANNEL *dcchptr[2][2];   //y cordinate points to chanel number, y coordinate points to buffer number
extern volatile SPI_MCP_STATE spistep;
extern volatile uint8_t spichannel;
extern volatile CANCOM_STATES cancomstat; 
extern volatile CANRxMessageBuffer canrxbuf[];
extern volatile uint8_t canrxptr;


//transfer 0x0101, exact same structure as for VCS183. some fileds are not used, those will be simply not even referenced
extern uint16_t *statvar,*uac1,*uac2,*uac3,*iac1,*iac2,*freq,*pabs,*prel,*ubatt,*engspeed,*actu,*acti,*ifuelp,*runlast,*enlast,*codeWarn,*codeAlm,
                *mwordlow,*mwordhigh,*stwordlow,*stwordhigh,*miscwordlow,*miscwordhigh,*pendsnl,*pendsnh,*tankres,*iac3,*coil2;
extern int16_t *tcoolin,*cosfi,*tcylh,*texhm,*tcoil,*tvcs,*engoil,*altbear,*coolout;
extern uint8_t *poil,*tank,*comact,*banks,*comptemp,*stastoreas,*t_idle,*max_crank;

extern uint16_t *idlespeed,*nomspeed,*uoutnom,*treg,*kp,*kd,*ki,*maxint,*uouth_w,*uouth_o,*uoutl_w,*uoutl_o,*fac_nom,*fac_h,*fac_l,*iouth_w,*iouth_o,
                *pouth_w,*pouth_o,*poutnom,*ubattl_w,*ubatth_w,*ubatth_o,*speedh_w,*speedh_o,*d_uouth,*d_uoutl,*d_iph_w,*d_iph_o,*d_ubatth,*d_speedh,
                *rtkfull,*rtkempty,*genconfl,*genconfh,*sensconfl,*sensconfh,*progsn_h,*progsn_l,*progdate_l,*progdateh;


extern uint8_t *coolin_w,*coolin_o,*altb_w,*altb_o,*exhm_w,*exhm_o,*abs_exhm,*cylh_w,*cylh_o,*abs_cylh,*coil_w,*coil_o,*poil_w,*poil_o,*d_temps,*d_poil,*tk_low,*d_tank,
                *cooldown_temp,*cooldown_time,*act_crank,*act_idle,*maxoil,*priming,*poles,*compb0,*fan_low,*fan_high,*toil_w,*toil_o,*coolout_w,*coolout_o,*threslow,*cranklow,*idlelow,*d_poillow;

extern uint16_t enrem,rtkmin,rtkmax;
extern uint16_t alptr,evptr,actmax,actmin;
extern uint16_t lgreq,sinfi;
extern uint16_t stastoptr;
extern uint32_t valsn,perssn,lastuserint,lastrx0005,now,showstasto;


extern HWTYPE hwtype;
extern TX0003 tx0003;
extern TX000A tx000A;
extern TX0101 tx0101;
extern TX0130 tx0130;
extern TX013A tx013A;
extern TX0138 tx0138;
extern TX01B6 tx01B6;
extern TXB3232 tx01B0,tx01B1,tx01B2,tx01B3,tx01B4,tx01B5;
extern RQHANDLER rq0100,rq0005,rq000B,rq0138,rq013A,rq01B0,rq01B1,rq01B2,rq01B3,rq01B4,rq01B5,rq01B6;
extern RQHANDLER x01B0,x01B1,x01B2,x01B3,x01B4,x01B5;
extern RQHANDLER *rqs[MAXRQS];
extern uint16_t tx0104[40][16];        //array of 40 transfers type tx0104.
extern RTCX caldate;
extern SYS_STAT sysstat;
extern STAT_BITS stat_bits;
extern ST_FEEDB st_feedb;
extern PANEL panels[9];
extern CANNODE panelx20;

extern CANMFR mfr[MFBUFS];   //outbound data may be placed into mfrout. One buffer at a time only
extern CANMFR *mfrx[MFBUFS];
extern CANSFR sfr[SFBUFS];   //same as MFR outbound data
extern CANSFR *sfrx[SFBUFS];
extern uint8_t mfdata[MFBUFS][223];
extern SYS_STAT sysstat;
extern GEN_APP_STATUS gcontstat;

extern LEDSTAT ledstat;

extern uint16_t batvolt;

#endif /* _GLOBALS_H */

/* *****************************************************************************
 End of File
 */
