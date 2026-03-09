/* 
 * File:   Defines.h
 * Author: paulmuell
 *
 * Created on August 21, 2015, 10:40 PM
 */

#ifndef DEFINES_H
#define	DEFINES_H

#include "Globals.h"


#ifdef	__cplusplus
extern "C" {
#endif
#define     SWVER  0x01010014       //format is uint64-T
#define     PROTREL 0x0002

//initial SW version 01.01.0000 First delivery Jan. 2016
//Version 01010001 / March 2016:        Major revision on options and communication. Implements protocol version 1.1.1 as of March 2016.    
//version 01010002 /April 2016:         Bug fixes of previous version, added programming option, added reset options, added retrieve log data option    
    
//without version change(shipment from April did not go to the customer but FP paderborn)
//                 /Aug. 2016:          Bug fixes (assignment of gen. status variable); Will support static remote input from panel, will support remote start on VCS
//  01010002       /Sept. 2016:         Programming or resetting cumulative data will cause power cycle of the VCS. Bug with false compensation current calculation removed.
//                                      User shall program capacitance of 1 cap. in lowest bank; Nominal voltage needs to be phase to neutral (NOT!!!! phase to phase).
//                                      PID algorithm is different for speed and voltage, engine will not hunt at low idle.
// 01010003        /Dec. 2016:          Reduced max. actuator current, reduced time threshold for max. actuator current     
// 01010004        08/08/2017:          Modified routine rootx. Will end without achieving desired accuracy if no further gain in accuracy is to be expected
//                                      Removed SPI error from error list    
    
//01010005, 08/31:  Added transfer 0x000B, changed log data upload, added cooldown disable option. 
    
//01010006  11/08/17:    Modified actuator calibration, different limits and time-out. Modified pwr-up (CAN-communication for 3 seconds). WIll work with new FP actuator
    
//01010007  12/04/17:    Modified to run within tight frequency but less tight voltage limits. Adaptive voltage control
    
//01010008  1/17/2018:   Will allow limits for adaptive voltage control to be programmed. Will allow user to enable/disable adaptive voltage control mode    
//01010009  1/30/2018:   If ADC times out on any AC sensing channel then this will immediately cause that channel to read 0. Will allow limit "0" on oil pressure shutdown
//0101000A  02/05/2018:  Added some delay in output current calculation, min. ADC rate 48 counts per each sampling    
//0101000B  04/17/2018:  Enabled fan breaker monitor. Modified water leak limit to fit fan breaker monitor. Enabled remote power up and remote power down along with start command.       
//                       Modified tank level sensing. Biasing resistor is 270Ohm, 1%
//0101000C 1/14/2020:   Added status EXT_ADC_READZERO. Will be called if an AC channel reads below MINADC for MAXNOADC. IN such case the corresponding data in tx0101 will be set to 0 upon next call of readextadc
//                      will no longer use non-mirrored data. Added option to configure oil pressure sensor to be active or resistive. Removed factor and offset for actuator current and actuator voltage
//0101000D 5/23/2020    Debug items as detected by CPP check    
 
//1.1.0.14  8/4/2020    Modified for PWM control using linear actuator

//0101000F  9/15/2020   Minimum PWM DC programmable    
    
//01010010  12/27/2020  modified CAN com to always channel transfer 0x013A into structure TX0138 and vice versa
//                      modified limits for cool down time and act. idling
//          01/04/2021  Modified total alarms to 80. Log data will be re-read for validation after saving            
//                      Alarm log area thus is 0x0A00 ... 0x31FF (80x0x80 bytes), periodic log data form 0x3200 to 0x59FF;

//1.1.0.17  12/30/2021  Modified voltage sensing; factor for res. divider changed because of parallel path on filter input on the VCS    
//                      Exhaust manifold sensor no longer mandatory  
    
//1.1.0.18  04/16/2023  Changed code to run test routine according to the actuator that was programmed
    
//1.1.0.19  01/09/2023  Added option to program BLDC actuator
    
//1.1.0.20  03/08/2026  Modified memory access 
    
    
//definition to avoid plib warnings
#define _SUPPRESS_PLIB_WARNING   
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 
    
    
//Macros to test, set and c.ear bits in bitfield defined variables
#define TESTBIT(var,bit)    ((var)&(1<<bit))
#define SETBIT(var,bit)     ((var)|=(1<<bit))
#define CLEARBIT(var,bit)   ((var)&=~(1<<bit))

#define TX101LENGTH 35
#define TX130LENGTH 68
    
#define NOCANMAX     7   //max. time without CAN communication that would trigger shutdown    
#define MAXNODATE    5000   //max. time withput valid calendar date
#define CANOFF          250       //bus will stay off after a fault for this time
    
    
#define MINSPEED    850 //this is considered engine runnning
#define COLDVCS     40  //this is the threshold for low temperature issues: cranking time, idling time, oil pressure evaluation and flame start
#define NOSENSOR    50  //this is the threshold VCS temperature to accept missing sensors    
 
//memory allocation, data that shall not be overwritten    
#define VCSSNLOC    0x01A0  //start of serial number
#define VCSSNLEN    2       //number of memory locations (uint18) occupied by serial No
#define CALDATALOC  0x01D8  //Start of calibration data
#define CALDATALEN  6       //number of memory locations reserved for calibration data    
    
    
//Communication
#define RXMAX   500     //timeout for serial reception unit is ms    
#define MAXFIFOFULL     500         //max wait time if TX FIFO is full
    
    
//VCS data expectation, if in programing or calibrating mode
#define NOEXPECT    0
#define PROGMIRR    1
#define PROGNONM    2
#define PROGINFO    3
#define CALVCS      4
#define PROGSW      5
#define PROGALLMEM  6    
    
    
//actuator
#define	STEPVALMAX      17000 //min and max values for OC5RS. Stepper works in increments of 5us (200ns on timer 2)
#define	STEPVALMIN      200
#define	STEPMOVE        25	 //creates a move of 1 increment on stepper motor, i.e. output changes by 5us
#define STEPVALMAX_BLDC 1300 //min value BLDC actuator. Corresponding to MAX position
#define STEPVALMIN_BLDC 6250 //max value BLDC actuator. Corresponding to MIN position    
//test linear actuator
#define TESTLINMOT 

    
#define ACTDELTA        200  //Delta actuator range, upper and lower end, between calibrated range and usable range
#define MINSTEPRANGE	2000 //minimum delta between stepvalmin and stepvalmax
#define D_MAXACT        3000 //8000  //Time delay for actuator alarms
#define RED_ACTMAX      200  //while regulating, if actuator current is above max for this time, then acutaor limits will be shifted
    
    
    
//led status
#define ALLOFF      0    
#define GNSOLID     1
#define RDSOLID     2
#define GNBLK       3
#define RDBLK       4
#define GNRDBLK     5
#define RDGNBLK     6
#define BLKALT      7
#define BLKSYNC     8
    
//ADC reading, AC side
#define MINADCVAL   12  //anything less than this is considered zero
#define STOPADCVAL  10   //anything less than this will stop ongoing conversion
#define MAXNOACT    375 //will stop searching of no treshhold found in this amount of samples. Assuming 40us per sample period
#define HVDCMIN     11500   //minimum voltage on positive output of HVDC converter, unit is mV
#define HVDCMAX     16500   //maximum 
#define ACTUMINW    400 //min. actuator voltage, unit 10mV                       
#define ACTUMAXW    620
#define ACTUMAXO    680
#define ACTIMAXW    90 //actuator current, unit is 10mA
#define ACTIMAXO    195     
#define ACTIMAXRED  140
#define CALIBDOWN   130
#define CALIBUP     150    
    
#define VREFL       2500
#define VREFH       2500    
    
//battery voltage range, required to keep the DC-DC-Converter in save operating limits    
#define BATMIN      1900
#define BATMAX      3500    
    

#define FANDELAY    5000    //fan switchover time
#define COMPCYC     2000    //cycle time compensation
#define MINCRNK     500     //minimum cranking time, speed reading continuous >MINSPEED
    
    
//Data for resistive oil pressure sensor, if ever used
#define OILPRL      0       //oil pressure at low resistance, unit is 100mbar
#define OILPRH      50      //oil pressure at high resistance, unit is 100mbar
#define RPOILL      10      //resistance at low oil pressure, unit is Ohm
#define RPOILH      180     //resistance at high oil pressure, unit is Ohm    
    
    
//this portion just to simplify coding
    
//variable definitions
#define C_WARN      tx0101.val.codeWarn
#define C_ALM       tx0101.val.codeAlm
#define MSGWRDL     tx0101.val.mwordlow
#define MSGWRDH     tx0101.val.mwordhigh
#define STATWRDL    tx0101.val.stwordlow
#define STATWRDH    tx0101.val.stwordhigh   
#define GENCFGL     tx0130.val.genconfig_l
#define GENCFGH     tx0130.val.genconfig_h
#define SENSCFGL    tx0130.val.sensconfig_l
#define SENSCFGH    tx0130.val.sensconfig_h
#define ALPTR       tx0131.val.alptr
#define EVPTR       tx0131.val.evptr    
#define UAC1        tx0101.val.uac1
#define UAC2        tx0101.val.uac2    
#define UAC3        tx0101.val.uac3
#define IAC1        tx0101.val.iac1
#define IAC2        tx0101.val.iac2    
#define IAC3        tx0101.val.iac3    
#define UACNOM      tx0130.val.uacnom    
    
//bit definitions
#define AL_PENDING  stfeedb.fields.alpending    
#define ACTSTAT     stfeedb.fields.actstat
#define REQSTAT     stfeedb.fields.req_in    

    
//tolarances to calculate floating voltage limit    
#define DELTAF      4  //rstrict to this frequency deviation, unit is Hz/10
#define DELTAU      70  //Allow this voltage deviation to cope with frequency bandwith. Unit is V/10
    
#ifdef	__cplusplus
}
#endif
                     
#endif	/* DEFINES_H */


