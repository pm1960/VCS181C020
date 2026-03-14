
#ifndef _DEFINES_H    /* Guard against multiple inclusion */
#define _DEFINES_H


#include <stdint.h>
//#include "Structures.h"


#define SWVER   0x01010001      //32 bit type; Project under development
#define PROTREL 2


//Macros to test, set and clear bits in bitfield defined variables
#define TESTBIT(var,bit)    ((var)&(1<<bit))
#define SETBIT(var,bit)     ((var)|=(1<<bit))
#define CLEARBIT(var,bit)   ((var)&=~(1<<bit))
#define TOGGLE_BIT(var, bit)   ((var) ^= (uint32_t)(1UL << (bit)))

#define ADDALM ((gcontstat!=GENCONTR_STATE_CRANKING) && ((gcontstat<GENCONTR_STATE_BEGSTOP) || (gcontstat>GENCONTR_STATE_UNACKAL)))

#define MAXACERROR  4


#define MAXNOCAN    5000    //max time without CAN communication
#define VREF        2500    //same reference for AC and DC

#define TR101LEN    39
#define TR130LEN    60
#define MAXRQS      12
#define MAXLOG      75

#define MFBUFS      8
#define SFBUFS      8
#define IDLE        0
#define BUSY        1
#define DTA_AVAIL   2
#define TX_REQ      3   //outbound buffers only, is requested

#define VBATTMIN    1800
#define VBATTMAX    3600 
#define PWRUPTIME   7000
#define RQUPD       250

#define VREFL 2500
#define VCSTEMPWARN 90
#define IFUELPMIN   50
#define IFUELPMAX   400
#define NOSPEED     200
#define MINSPEED    750         //minimum engine speed that may be considered running generator
#define MINSPEED_COLD   1200    //min. speed in low environment
#define STARTDETECT     400    //Minimum time in ms the speed needs to be >MINSPEED in order to detect successfull enigne start
#define STARTDETECT_COLD    3000    //As above, but for low ambient

#define MINRUNTIME      1500    //minimum run time to reset consecutive no start counter and to open r.w. valve
#define WRMUPTIME       15000   //warm-up time. After idling, water temp. alarms will be surpressed for an additional time as defined here, uint is ms
#define MINOFFTIME      5000    //minimum off time since last time the engine ran

#define FANDELAY    5000    //fan switchover time
#define FANHYST     3   //temp hysteresis
#define COMPCYC     2000    //cycle time compensation
#define HS_WARN     80
#define HS_SHDN     100
#define HYST        5

//ADC sample status
#define ADC_IDLE        0
#define ADC_WAIT4ZERO   1
#define ADC_WAIT4START  2
#define ADC_SAMPLING    3
#define ADC_READY       4
#define ADC_TIMEOUT     5
#define ADC_FAILHIGH    6
#define ADC_FAILSHORT   7

#define ADC_MINADC      10  //triggers measuring
#define MINLOW          20000  //time is measured in multiples of 100ns derived from timer 5/4. need to read a low for this long (2ms) prior to searchig for a high 
#define MAXHALFPER      150000  //this would be min. 15ms -> 33Hz. 
#define MAXWAIT         500000 //will not wait longer than 50ms in either case  in latched High or Low
#define MINHALFPER      50000  //min. half period needs to be 5ms -> max. 100 Hz 

//start / stop reason as used in this project:
#define POR_ON  0   //used wen coming out of POR
#define BUTTSTA 1   //Start by button on the panel
#define BUTTSTO    2   //Stop by button on the panel
#define STATSTA    3   //start via static input
#define STATSTO    4   //stop via static input
#define DYNSTA     5   //start via dynamic input
#define DYNSTO     6   //stop via dynamic input
#define NMEASTA    7   //start via NMEA input
#define NMEASTO    8   //stop via NMEA input
#define ALSTOP     9   //stop because alarm condition


#define	STEPMOVE        5       //creates a move of 1 increment on stepper motor, i.e. output changes by 2us
#define ACTDELTA        100     //Delta actuator range, upper and lower end, between calibrated range and usable range
#define MINSTEPRANGE	800     //minimum delta between stepvalmin and stepvalmax
#define ACTIMAXC        190     //actuator current limit for calibration BLDC actuators
#define STEPVALMAX      1300    //min value BLDC actuator. Corresponding to MAX position
#define STEPVALMIN      6250    //max value BLDC actuator. Corresponding to MIN position
#define ACTIMINC          2     //threshold for actuator disconnected
#define ACTUMINW        630     //actuator voltages, unit 10mV  
#define ACTUMINO        600
#define ACTUMAXW        690    
#define ACTUMAXO        720
#define ACTIMAXW        85 //actuator current, unit is 10mA
#define ACTIMAXO        145   

#define WARRANTYMINUTES 0x0001A70C      //warranty in operating time is max 1805 hours
#define WARRANTYSTART   300             //warrant should start after 5 hours of operation

#define DIGDEBOUNCE 20


#define MAXTEMPVCS      80
#define AFTERCRANK      2000    //duration of extended after-crank period. Only valid if started based on il pressure which in turn is only valid if used on asynchronous generators

#define MAXPER         62500    //100ms-> 10Hz -> 600rpm
#define MINPER          6250    //10mS ->100Hz -> 6000rpm

#endif /* _DEFINES_H */

/* *****************************************************************************
 End of File
 */
