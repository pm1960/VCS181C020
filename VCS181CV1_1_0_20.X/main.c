#pragma config FVBUSONIO=OFF, FUSBIDIO=ON, UPLLEN=OFF, UPLLIDIV=DIV_1
#pragma config FCANIO=ON
#pragma config FSRSSEL=PRIORITY_4
#pragma config FPLLIDIV=DIV_2, FPLLMUL=MUL_16, FPLLODIV=DIV_1, FPBDIV=DIV_2     //Will set frequency to 80MHz and peripheral clock to 40 MHz.
#pragma config POSCMOD=HS,IESO=OFF,FNOSC=PRIPLL,OSCIOFNC=OFF,FSOSCEN=OFF         //turn off secondary OSC
#pragma config FWDTEN=OFF, WDTPS=PS4096
#pragma config FCKSM=CSECMD//enable clock switching. Required for low power mode
#pragma config CP=OFF, BWP=OFF, PWP=OFF
#pragma config ICESEL=ICS_PGx1

#include "Defines.h"
#include <xc.h>
#include <plib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "cpusetup.h"
#include "usbcom.h"


void main(void){
    cpusetup();
    checkpwrup();
    RCONbits.BOR=RCONbits.POR=RCONbits.EXTR=false;
    usbstat=USBCOM_STATE_INIT;
    gencontrstate=GENCONTR_STATE_INIT;
    cancomstate=CAN_STATE_INIT;
    tbusfail=t_1ms;
    calok=false;
    while(true){
        usbcont();
        GENCONTR_Tasks();
        CANCOM_Tasks();
        SYS_Tasks();
    }
}
