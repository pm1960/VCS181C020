/* 
 * File:   StateDefinitions.h
 * Author: Paul Muell
 *
 * Created on September 2, 2015, 10:23 PM
 */

#ifndef STATEDEFINITIONS_H
#define	STATEDEFINITIONS_H

#ifdef	__cplusplus
extern "C" {
#endif
    
    
typedef enum{
	/* Application's state machine's initial state. */
    USBCOM_STATE_INIT=0,    //same as USB error
    USBCOM_STATE_READY,     //no activity
    USBCOM_STATE_TXON,      //currently transmitting regular frame
    USBCOM_STATE_TXWAITACK, //waiting for ack frame to be sent
    USBCOM_STATE_TXWAIT,    //wait time between ack transfer and requested return transfer
    USBCOM_STATE_RESET,     //Resetting port
}USBCOM_STATES;

volatile USBCOM_STATES usbstat;


typedef enum{
    GENCONTR_STATE_INIT=0,           //initial status, just got out of reset
    GENCONTR_STATE_INITEND,
    GENCONTR_STATE_WAITPWRUP,        //waiting for the buck converter to stabilize
    GENCONTR_STATE_CALPROGFAIL,      //Missing calibration and/or program data
    GENCONTR_STATE_ACTCALINICOM,     //Calibrating actuator and initializing CAN communication
    GENCONTR_STATE_INSTOP,           //this covers all stopped modes
    GENCONTR_STATE_STOPLOOP,         //performing stop loop
    GENCONTR_STATE_PREPSTART,        //Start process was initialized
    GENCONTR_STATE_CRANKING,         //cranking the engine
    GENCONTR_STATE_PWRDWN,           //in low power mode
    GENCONTR_STATE_IDLING,           //generator is in between having cranking gear engaged and having good output voltage
    GENCONTR_STATE_INRUN,            //generator is running     
    GENCONTR_STATE_PWRCYCLE,         //cold start is required
    GENCONTR_STATE_DRYTEST,          //testing VCS in stop condition
    GENCONTR_STATE_RUNTEST,          //testing in running condition (compensation test)
    GENCONTR_STATE_UNACKAL,          //unacknowledged alarm is pending
    GENCONTR_STATE_WRMUP,
    GENCONTR_STATE_CONFPWRUP,
    GENCONTR_STATE_COOLDWN,
}GENCONTR_STATES;
  
GENCONTR_STATES gencontrstate;  

typedef enum{
    CAN_STATE_INIT=0,    
    CAN_STATE_HWIRESP,              //CAN hardware is not responding
    CAN_STATE_BUSOFF,
    CAN_STATE_BUSON,
    CAN_STATE_TURNON,
}CANCOM_STATES;


#ifdef	__cplusplus
}
#endif

#endif	/* STATEDEFINITIONS_H */

