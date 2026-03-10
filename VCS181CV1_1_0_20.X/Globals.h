/* 
 * File:   Globals.h
 * Author: Paul Muell
 *
 * Created on September 1, 2015, 10:44 PM
 */

#ifndef GLOBALS_H
#define	GLOBALS_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#define _SUPPRESS_PLIB_WARNING   
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 
    
#include <stdint.h>
#include <stdbool.h>
#include <plib.h>
#include "StateDefinitions.h"
#include "Defines.h"
    
#define TX101LENGTH 35
#define TX130LENGTH 68
typedef enum{
    EXT_ADC_WAIT4ZERO=0,
    EXT_ADC_WAIT4H,
    EXT_ADC_SAMPLING,
    EXT_ADC_DTAAVAIL,
    EXT_ADC_TIMEOUT,
    EXT_ADC_READZERO,
}EXT_ADC_STAT;    
    

typedef struct{
        uint32_t adcdata;           //stores actual ADC output
        uint16_t adccntr;           //sample counter
        EXT_ADC_STAT stat;
//        
//        unsigned adcstat:   3;      //ADC status:   000: waiting for zero to begin sampling
//                                    //              001: waiting for H to beginsampling
//                                    //              010: sampling
//                                    //              011: data available
//                                    //              100: data is junk. Signal H or L too long. Application needs to confirm
    }adcchannel;
    
    
    
    volatile adcchannel adcch[8]; // u1,u2,u3,i1,i2,i3,uhv,tcomp,in this order

//time keeping    
    volatile uint32_t t_1ms,t_1s,t_uread,t_u4rx,tbusfail;
    volatile uint8_t u4rxbuf[256],u4txbuf[256],u4rxptr,u4txptr,adcind;  //index which buffer is in process 
    volatile uint8_t serrxbuf[256],sertxbuf[256],serrxptr,sertxptr,ledstat;
    volatile uint16_t t3ovr,nocan;
    volatile bool istriphase;
    

volatile union{
    struct{
        unsigned   actidone:   1;       //will indicate actuator current is available
        unsigned   gotfreq:    1;       //will indicate new frequency data available
        unsigned   gotpf:      1;       //will indicate new power factor data available
        unsigned   lastupuls:  1;       //valid last pulse on AC frequency sensing
        unsigned   iscap:      1;       //power factor is capacitive
        unsigned   spi4fail:   1;       //temporarily store fault of SPI 4 ADC 
        unsigned   rxon:       1;       //ongoing reception on U4 (USB)
        unsigned   nay:        1;       //not assigned yet
        uint32_t   lastutime;             //timer value when last capturing frequency signal
        uint32_t   period;               //period of voltage signal in units of 1.6uns
        uint32_t   phshift;              //phase shift between current and voltage in units of 1.6us
        uint32_t   lastitime;            //time when reading last current triggered pulse
    }Bits;
    uint8_t bts[13];
}isrcom;

volatile CANCOM_STATES cancomstate;     
volatile CANRxMessageBuffer canrxbuf[256];   //circular buffer of inbound messages
volatile uint8_t canrxptr;                  //will point to last received message


//Status feedback
union{
    struct{
        unsigned    actstat:    1;  //reflects actual status
        unsigned    startinhib: 1;  //set if start is not permitted
        unsigned    newin:      1;  //set if new data from panel(s) is available
        unsigned    alpending:  1;  //an alarm is pending, will not clear any alarm bits
        unsigned    req_in:     1;  //requested input (only valid if newin==true)
        unsigned    pwrdwn:     1;  //power down is requested
        unsigned    notyet:     2;  //bit is not defined yet
        uint32_t    statustime;     //freezes t_1ms at status transfer   
    }fields;
    uint8_t bts[5];
}stfeedb;

union{
    struct{
        uint16_t vcsadr;              //no longer used, may be assigned a different variable
        uint16_t unused;          //no longer used, may be assigned a different variable
        uint16_t vcssnl;
        uint16_t vcssnh;
    }val;
    uint16_t trans[4];
}tx000A;


union{      //regular transfer frame
    struct{ //first 2 
        uint16_t    statvar;    //opstat and VCS feedback. Needs to be 
        uint16_t    uac1;
        uint16_t    uac2;
        uint16_t    uac3;
        uint16_t    iac1;
        uint16_t    iac2;
        uint16_t    iac3;
        uint16_t    pac;
        uint16_t     pacr; 
        int16_t     cosfi;
        uint16_t    fac;
        uint16_t    ubatt;
        int16_t     tcoolin;
        int16_t     tcylhead;
        int16_t     texhaust;
        int16_t     tcoil;
        int16_t     tbearing;
        int16_t     tcoolout;
        int16_t     tengoil;
        int16_t     tvcs;
        uint8_t     poil;
        uint8_t     tank;
        uint16_t    engspeed;
        uint16_t    actu;
        uint16_t    acti;
        uint16_t    runlast;
        uint16_t    enlast;
        uint16_t    codeWarn;
        uint16_t    codeAlm;
        uint16_t    mwordlow;
        uint16_t    mwordhigh;
        uint16_t    stwordlow;
        uint16_t    stwordhigh;
        uint16_t    miscwordlow;
        uint16_t    miscwordhigh;
        uint16_t    servwordlow;
        uint16_t    servwordhigh;
        uint8_t     actcom;
        uint8_t     compon;
        uint16_t    tankres;
        uint8_t     tcomp;
        uint8_t     stastoreason;
    }val;
    uint16_t trans[TX101LENGTH];
}tx0101;

union{
    struct{
        uint16_t logcntr;
        uint16_t logrtclow;
        uint16_t logrtchigh;
        uint16_t x101[TX101LENGTH];     //fulll transfer 0x0101
        uint16_t x13A[6];
    }logfields;
    uint16_t trans[44];
}tx0103;
//Settings for generator control
union{
    struct{
        uint16_t    nidle;
        uint16_t    nnom;
        uint16_t    uacnom;
        uint16_t    treg;
        uint16_t    kp;
        uint16_t    kp_speed;
        uint16_t    kd;
        uint16_t    kd_speed;
        uint16_t    acvhighw;
        uint16_t    acvhigho;
        uint16_t    acvloww;
        uint16_t    acvlowo;
        uint16_t    fnom;
        uint16_t    fhighw;
        uint16_t    floww;
        uint16_t    acihighw;
        uint16_t    acihigho;
        uint16_t    acphighw;
        uint16_t    acphigho;
        uint16_t    acpnom;
        uint16_t    udcloww;
        uint16_t    udchighw;
        uint16_t    udchigho;
        uint16_t    nmaxw;
        uint16_t    nmaxo;
        uint16_t    d_umax;
        uint16_t    d_umin;
        uint16_t    d_outhw;
        uint16_t    d_outho;
        uint16_t    d_dchigho;
        uint16_t    d_nhigho;
        uint16_t    rtkfull;
        uint16_t    rtkempty;
        uint16_t    genconfig_l;
        uint16_t    genconfig_h;
        uint16_t    sensconfig_l;
        uint16_t    sensconfig_h;
        uint16_t    svnenable_l;
        uint16_t    svnenable_h;
        uint8_t     tidlenorm;
        uint8_t     tcranknom;        
        uint8_t     coolinw;
        uint8_t     altbw;
        uint8_t     cylhw;
        uint8_t     exhmw;
        uint8_t     coilw;
        uint8_t     poiloww;
        uint8_t     coolino;
        uint8_t     altbo;
        uint8_t     cylho;
        uint8_t     exhmo;
        uint8_t     coilo;
        uint8_t     poillowo;
        uint8_t     cooloutw;
        uint8_t     coolouto;
        uint8_t     d_altemps;
        uint8_t     d_poillowonorm;
        uint8_t     tankw;
        uint8_t     d_tankw;
        uint8_t     cooldwntemp;
        uint8_t     cooldwntime;
        uint8_t     actcrank;
        uint8_t     actmxidle;
        uint8_t     mxoil;
        uint8_t     poles;
        uint8_t     exhabs;
        uint8_t     cylhabs;
        uint8_t     fuelprime;
        uint16_t    boostcap;
        uint8_t     tfanlow;
        uint8_t     tfanhigh;
        int8_t      tlowthresh;
        uint8_t     tidlellow;
        uint8_t     tcranklow;
        uint8_t     d_poillowolow;        
    }val;
    uint16_t wrds[TX130LENGTH]; 
}tx0130;               //mirrored VCS settings, transfer 0x0130

union{
    struct{
        uint8_t gensn[32];
        uint8_t fireboy[32];
    }val;
    uint16_t trans[32];
}tx0138;

union{
    struct{
        uint16_t runtl;
        uint16_t runth;
        uint16_t startcntl;
        uint16_t startcnth;
        uint16_t totalenl;
        uint16_t totalenh;
    }val;
    struct{
        uint32_t optime;
        uint32_t starts;
        uint32_t energy;
    }v32;
    uint16_t trans[6];
}tx013A;     //info transfer

uint16_t lasttrans;     //if waiting for a certain transfer, then CANCOM may use this variable to confirm it got that transfer
uint32_t caldate, now,enrem;  //used to keep track of time and date
uint32_t confword0,confword1,valsvn;    //confirmation words,  
uint16_t targvolt,pwmspeed,limactlowspeed,limacthighspeed;     //used to maintain floating target voltage; internal speed, for PWM regulation purpose. Limits for actuator movement
bool onpending,calok,pan_estop,pan_bs,bldc,linact;



uint16_t    pwdwnr;
uint16_t    stwl;
uint16_t    stwh;
uint16_t    c_alm;    

bool    gotspeed, gotvoltage;

#ifdef	__cplusplus
}
#endif

#endif	/* GLOBALS_H */

