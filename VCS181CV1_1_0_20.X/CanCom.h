/* 
 * File:   CanCom.h
 * Author: paulmuell
 *
 * Created on September 8, 2015, 7:51 PM
 */

#ifndef CANCOM_H
#define	CANCOM_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "Defines.h"
#include <stdint.h>
#include <stdbool.h>
#include <xc.h>
#include <plib.h>
#include "Globals.h"
#include "auxfunc.h"
    
    void CANCOM_Tasks (void);
    bool txtransfer(uint16_t transferid, uint8_t *msg);
    bool initcan(void);    
    bool txtrans(uint16_t rqtrans, uint8_t *bf);

typedef struct{ //no reference to data inside structure. It is obvious, that first mfr element uses mfrdata[0][223], next one mfrdata[1]223...and so on]
    uint16_t    idword;         //ID byte
    uint8_t     length;         //length of data received
    uint8_t     status;         //will reflect status of transfer
    uint8_t     dset;           //data set number. first 3 bits are sequence, last 5 bits are CAN frame number
    uint32_t    rxstart;        //time this reception started
    uint8_t     wroffs;         //offset for bytes to write
    uint8_t     source;         //source bits
    uint16_t    rxtime;         //time stamp of CAN bus reception, first frame of a MFR message, resolution 500us
} canmfr;                       //CAN multiframe message

typedef struct{
    uint16_t    idword;
    uint8_t     buf[8];
    uint8_t     status;
    uint8_t     source;
    uint16_t    rxtime;     //time stamp CAN reception. resolution is 500us
}cansfr;                    //CAN single frame message
    
#define MFBUFS  8
#define SFBUFS 8

//status definition for CAN single frame and multi frame buffers
#define IDLE            0
#define BUSY            1
#define DTA_AVAIL       2
#define TX_REQ          3   //outbound buffers only, is requested
//maximum time assigned for a multi-frame transfer. Any transfer taking longer will be cancelled
#define MAXRXT          100
#define CANDELAY        1500      //retry to gain communication after 1.5seconds

//CAN communication will use 1 RX and 2 TX message buffers, each buffer with 32 messages. Each message requires
// a total of 4 uint32t (2xData, 2xOther): 3FIFO*32buffers*4uint/buffer=384UINT32 required
uint32_t C1FiFoBuff[384];

canmfr mfr[MFBUFS],*mfx[MFBUFS];   //outbound data may be placed into mfrout. One buffer at a time only
cansfr sfr[SFBUFS],*sfx[SFBUFS];   //same as MFR outbound data

uint8_t mfdata[MFBUFS][223];  //data from multi-frame transfers; mfrdata[0][xxx]assigned to mfr0, mfrdata[1][xxx]assigned to mfr1, ...
uint8_t canprocptr;
    


#ifdef	__cplusplus
}
#endif

#endif	/* CANCOM_H */

