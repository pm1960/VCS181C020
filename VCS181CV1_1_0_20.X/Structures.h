

#ifndef _STRUCTURES_H    /* Guard against multiple inclusion */
#define _STRUCTURES_H

#include <xc.h>
#include <stdint.h>
#include "Defines.h"

typedef enum{
    GOTUPULS = (uint32_t)1<<0,
    GOTIPULS = (uint32_t)1<<1,        
    
}isr_flags_t;

typedef struct{
    uint8_t status;
    uint32_t bufferx;
    uint16_t samplesx;
    uint32_t tmrstart;
}ACCHANNEL;

typedef union{
    uint8_t bf8[128];
    uint16_t b16[64];
    uint32_t b32[32];   
}TXB3232;   //buffer 32x32, used for trnasfers 1B0 .. 1B5 and other applications


typedef struct{
    uint16_t status;
    uint16_t cntr;
    uint32_t bufferx;
}DCCHANNEL;

typedef enum {
    SPI_MCP_START,   // send first byte 0x01 (start)
    SPI_MCP_CMD,     // send second byte: S + channel
    SPI_MCP_DUMMY    // send third byte: dummy to clock out LSBs
} SPI_MCP_STATE;

typedef enum{
    UNKNOWN=0,
    ISC006,
    ISC007,
    ISC008,
}HWTYPE;

typedef struct{
    uint16_t transfer;
    uint32_t lastissued;    //time the request was issued. Auto-Update routine requires this
    uint8_t source;         //this is to be 0x20 .. 0x27 for panels or 0x00 for VCS
    uint8_t status;         //0 if not pending. 1 if just asked for, i.e. transmit immediately. 2 if potential re-transmit is required
}RQHANDLER;

typedef union{
    struct{
        unsigned sec2:      5;   
        unsigned minutes:   6;
        unsigned hours:     5;
        unsigned day_of_m:  5;
        unsigned month:     4;
        unsigned year:      7;
      
    }val;
    uint32_t trans;
}RTCX;
typedef union{
    struct{
        unsigned    startstop:  1;  //start/stop request. 
        unsigned    rqsavnorm:  1;  //next data save is to normal if set. Will clear this bit if that saving was successful
        unsigned    rqsavdef:   1;  //next data saving will be to default. Will clear this bit if that saving was successful
        unsigned    staticstat: 1;  //If start/stop is static then the control panel will set this bit if the generator was requested to be static started and clear it if it was requested to be static stopped
        unsigned    holdcoil:   1;  
        unsigned    rwvalve:    1;  //status of water valve 
        unsigned    testmode:   1;  
        unsigned    prgmembl:   1;  //program a memory block
        unsigned    manop:      1;  //manually operate the VCS
        unsigned    glpl1:      1;  //glow plug 1 on/off
        unsigned    glpl2:      1;  //glow plug 2 on/off
        unsigned    pullcoil:   1;  //pull coil
        unsigned    startsol:   1;  //start solenoid on/off
        unsigned    fpump:      1;  //fuel pump on/off
        unsigned    stopsol:    1;  //stop solenoid on/off
        unsigned    gdbk:       1;  //ground breaker on/off
        uint8_t     actval;         //required actuator pos. in %. Only valid in manual control
        unsigned: 5;
        unsigned    testfan:    1;  //inverter only, IDA will disregard this
        unsigned    testinv:    1;  //run PWM at 50% DC; inverter only, IDA will disregard this
        unsigned    invgen:     1;  //inverter generate on off, test purpose inverter only. IDA will disregard this bit
    }val;
    uint16_t trans[2];
}TX0003;

typedef union{
    struct{
        uint16_t nodid;              
        uint16_t unused;          
        uint16_t snl;
        uint16_t snh;
    }val;
    uint16_t trans[4];
}TX000A;

typedef union{
    uint8_t  trans_b[TR130LEN<<1];
    uint16_t trans_w[TR130LEN];
}TX0130;  

typedef union{
    struct{
        uint8_t gensn[32];
        uint8_t fireboy[32];
    }val;
    uint16_t trans[32];
}TX0138;

typedef union{
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
}TX013A;

typedef union{
    struct{
        uint16_t    shipdate_l;
        uint16_t    shipdate_h;
        uint16_t    warrantydate_l;
        uint16_t    warrantydate_h;
        uint16_t    status;
    }val;
    uint16_t trans[5];
}TX01B6;

typedef struct{
    uint8_t sourceadr;
    uint32_t ext_contr;
    uint32_t lastcom;   
    uint32_t pansw;             //panel firmware version
}PANEL;

typedef struct{
    uint32_t nodesw;
    uint32_t nodesn;
    uint16_t protrel;
    uint8_t tx10xlen;
    uint8_t tx13xlen;
    uint8_t nodetype[20];
}CANNODE;

typedef union{
    uint8_t  trans_b[TR101LEN<<1];
    uint16_t trans_w[TR101LEN];
}TX0101;


typedef enum{
    SYS_INIT=0,
    SYS_CONNECT,
    SYS_ON,
    SYS_WAITSTOP,
    SYS_PWRDWN,
}SYS_STAT;

typedef enum{
    CAN_STATE_INIT=0,    
    CAN_STATE_HWIRESP,              //CAN hardware is not responding
    CAN_STATE_BUSOFF,
    CAN_STATE_BUSON,
    CAN_STATE_TURNON,
    CAN_STATE_RXONLY,
    CAN_STATE_RESET,
}CANCOM_STATES;

typedef enum{
    GENCONTR_STATE_INIT=0,           //0: initial status, just got out of reset
    GENCONTR_STATE_CALPROGFAIL,      //1: Missing calibration and/or program data
    GENCONTR_STATE_INSTOP,           //2: this covers all stopped modes
    GENCONTR_STATE_BEGSTOP,          //3: begin of stop routine, ramp up charge pumps for 5ms
    GENCONTR_STATE_STOPLOOP,         //4: performing stop loop
    GENCONTR_STATE_PREPDRY,          //5: preparation for dry test, basically ramping up charge pumps
    GENCONTR_STATE_DRYTEST,          //6: testing VCS in stop condition
    GENCONTR_STATE_UNACKAL,          //7: unacknowledged alarm is pending
    GENCONTR_STATE_CANNOTSTOP,       //8: short-circuit on GND breaker or stop solenoid, cannot stop engine
    GENCONTR_STATE_RQPWROF,          //9: requires a hard power off because of some error
    GENCONTR_STATE_PREPMANOP,        //10: Prepare manual operation
    GENCONTR_STATE_MANOP,            //11: in manual operation
    GENCONTR_STATE_ENDMANOP,         //12: ending manual operation
//beyond this definition no "non-moving" status must be declared         
    GENCONTR_STATE_PREPSTART,        //13: Start process was initialized
    GENCONTR_STATE_CRANKING,         //14: cranking the engine. Any subsequent states need to be running conditions!
    GENCONTR_STATE_IDLING,           //15: generator is in between having cranking gear engaged and having good output voltage
//all of the below status will check for oil pressure            
    GENCONTR_STATE_NOMSPEED,         //16: ramping up towards nominal speed
    GENCONTR_STATE_INRUN,            //17: generator is running     
    GENCONTR_STATE_WRMUP,            //18: generator warming up. After warm-up water sensors get armed
    GENCONTR_STATE_COOLDWN,          //19: generator cooling down
    GENCONTR_STATE_WAITUAC,          //20: Waiting for AC voltage, started on oil pressure 
}GEN_APP_STATUS;

typedef struct{
    unsigned onpending: 1;
    unsigned checkexhm: 1;      //will indicate the device may now check exhaust manifold temperature. applicable only if exhm sensor is enabled, fix time 4 sec. after start
    unsigned gotgp203:  1;      //will indicate a user interface (panel or USB interface board) was detected
    unsigned gotacti:   1;
    unsigned ch_water:  1;      //will be set if water based temperatures are supposed to be checked
    unsigned ch_oil:    1;      //will be set if oil pressure is supposed to be checked
    unsigned gottank:   1;      //will indicate that tank data was calculated      
    unsigned pwrdwn:    1;
    unsigned battles:   1;      //will indicate the local battle short is activated
    unsigned gotsinfi:  1;      //will indicate the control system did calculate a new phase shift
    unsigned saval:     1;       //requires an alarm to be logged
    unsigned logreason: 1;  //will flag the request to log the start/ stop reason to memory
    unsigned actcalini: 1;  //requesting initialization of actuator calibration
    unsigned actcalon:  1;       //indicates act. calibration in process, will discontinue monitoring the act. current and voltage
    unsigned gotspeed:  1;  //engine speed got updated
    unsigned nocmdata:  1;  //will indicate there is no cumulative data available in the panel
    unsigned gotuac:    1;
    unsigned gotiac:    1;
    unsigned: 14;
}STAT_BITS;


typedef union{
    struct{
        unsigned    actstat:    1;  //reflects actual status (true for running or commanded to run, false for stopped)
        unsigned    nocooldwn:  1;  //stop without cooling down
        unsigned    startinhib: 1;  //set if start is not permitted
        unsigned    newin:      1;  //set if new data from panel(s) is available
        unsigned    alpending:  1;  //an alarm is pending, will not clear any alarm bits
        unsigned    req_in:     1;  //requested input for start/stop bit (only valid if newin==true)
        unsigned    pwrdwn:     1;  //power down is requested
        unsigned: 1; 
        uint32_t    statustime;     //freezes t_1ms at status transfer   
        uint32_t    startidle;      //holds timer t_1ms when idling period did start
    }val;
    uint8_t trans[9];
}ST_FEEDB;

// *****************************************************************************
/* CAN TX Message SID Word
    This data structure represents the SID section of the CAN TX message. The data
    data structure is an element of the CANTxMessageBuffer data structure.
*/

typedef struct
{
    // CAN TX Message Standard ID. This value should
    // be between 0x0 - 0x7FF.
    unsigned SID:11;
    unsigned :21;
}CAN_TX_MSG_SID;

// *****************************************************************************
/* CAN Message EID Word
    This data structure represents the EID section of the CAN TX and RX message.
    The data structure is an element of the CANTxMessageBuffer and CANRxMessageBuffer
    data structures.
*/

typedef struct
{
    // Data Length Control. Specifies the size of the
    // data payload section of the CAN packet. Valid
    // values are 0x0 - 0x8.
    unsigned DLC:4;

    // Reserved bit. Should be always 0.
    unsigned RB0:1;
    unsigned :3;

    // Reserved bit. Should be always 0.
    unsigned RB1:1;

    // Remote Transmit Request bit. Should be set for
    // RTR messages, clear otherwise.
    unsigned RTR:1;

    // CAN TX and RX Extended ID field. Valid values
    // are in range 0x0 - 0x3FFFF.
    unsigned EID:18;

    // Identifier bit. If 0 means that message is SID.
    // If 1 means that message is EID type.
    unsigned IDE:1;

    // Susbtitute Remote request bit. This bit should
    // always be clear for an EID message. It is ignored
    // for an SID message.
    unsigned SRR:1;
    unsigned :2;

}CAN_MSG_EID;

// *****************************************************************************
/* CAN TX Message Buffer

  Summary:
    Defines the structure of the CAN TX Message Buffer.

  Description:
    This data structure represents the CAN TX Message Buffer. This data structure
    should be used for writing data to a CAN TX channel and transmitting data. A
    pointer to this type of data structure is returned by the CANGetTxMessageBuffer()
    function.

    The data structure allows the TX message buffer to be accessed as fields of a CAN
    TX message and also as an array of four 32-bit words. The latter allows for quick
    intialization of the message buffer.
*/


typedef union {

    struct
    {
        // This is SID portion of the CAN TX message.
        CAN_TX_MSG_SID msgSID;

        // This is EID portion of the CAN TX message.
        CAN_MSG_EID msgEID;

        // This is the data portion of the CAN TX message.
        uint8_t data[8];
    };

    // This is CAN TX message organized as a set of 32 bit
    // words.
    uint32_t messageWord[4];

}CANTxMessageBuffer;

// *****************************************************************************
/* CAN RX Message SID Word

  Summary:
    Defines the structure of the SID word section of the RX message.

  Description:
    This data structure represents the SID section of the CAN RX message. The data
    data structure is an element of the CANRxMessageBuffer data structure.
*/

typedef struct
{
    // SID of the Received CAN Message.
    unsigned SID:11;

    // Filter which accepted this message.
    unsigned FILHIT:5;

    // Time stamp of the received message. This is
    // valid only if the Timestamping is enabled.
    unsigned CMSGTS:16;

}CAN_RX_MSG_SID;

// *****************************************************************************
/* CAN RX Message Buffer

  Summary:
    Defines the structure of the CAN RX Message Buffer.

  Description:
    This data structure represents the CAN RX Message Buffer. Received messages could
    be either full-receive messages or data-only messages. Full receive message contains
    the message header and data payload section.  For a full receive CAN RX channel, the
    caller should use the msgSID, msgEID and data members . A data-only message contains
    only a 8-byte data payload. While using this data structure with a Data-Only type of
    CAN RX channel, the caller should use the dataOnlyMsgData member of the structure and
    should read only 8 bytes of data.


*/

typedef union {

    struct
    {
        // This is SID portion of the CAN RX message.
        CAN_RX_MSG_SID msgSID;

        // This is EID portion of the CAN RX message
        CAN_MSG_EID msgEID;

        // This is the data payload section of the
        // received message.
        uint8_t data[8];
    };

    // This can be used if the message buffer is to
    // be read from a Data-Only type of CAN RX Channel.
    uint8_t dataOnlyMsgData[8];

    // This is CAN RX message organized as a set of 32 bit
    // words.
    uint32_t messageWord[4];

}CANRxMessageBuffer;

typedef struct{ //no reference to data inside structure. It is obvious, that first mfr element uses mfrdata[0][223], next one mfrdata[1]223...and so on]
    uint16_t    idword;         //ID byte
    uint8_t     length;         //length of data received
    uint8_t     status;         //will reflect status of transfer
    uint8_t     dset;           //data set number. first 3 bits are sequence, last 5 bits are CAN frame number
    uint32_t    rxstart;        //time this reception started
    uint8_t     wroffs;         //offset for bytes to write
    uint8_t     source;         //source bits
    uint16_t    rxtime;         //time stamp of CAN bus reception, first frame of a MFR message, resolution 500us
} CANMFR;                       //CAN multiframe message

typedef union{
    struct{
        uint16_t    idword;
        uint8_t     buf[8];
        uint8_t     status;
        uint8_t     source;
        uint16_t    rxtime;     //time stamp CAN reception. resolution is 500us
    }val;
    uint8_t trans[14];
}CANSFR;      
  

#endif /* _STRUCTURES_H */

/* *****************************************************************************
 End of File
 */
