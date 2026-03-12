#pragma config FCANIO=ON                    //will use default CAN port RF0/RF1
#pragma config FSRSSEL=PRIORITY_5           //SRS ufsed for priority 5, SPI4
//#pragma config FPLLIDIV=DIV_3, FPLLMUL=MUL_20, FPLLODIV=DIV_1, FPBDIV=DIV_2       //for 12MHz quartz
#pragma config FPLLIDIV=DIV_2, FPLLMUL=MUL_16, FPLLODIV=DIV_1, FPBDIV=DIV_2         //for 10MHz quartz
#pragma config POSCMOD=HS,IESO=OFF,FNOSC=PRIPLL,OSCIOFNC=OFF,FSOSCEN=OFF
#pragma config FWDTEN=OFF,WDTPS=PS4096      //used for low power mode during cranking
#pragma config FCKSM=CSECMD                 //clock switch enabled (required during low power
#pragma config CP=OFF, BWP=OFF, PWP=OFF     //code protect and block protect
#pragma config ICESEL=ICS_PGx1              //program interface on port 2

#include <xc.h>

#include "Defines.h"
#include "Globals.h"
#include "cpusetup.h"

void main(void){
    static uint32_t tmrx;
    cpu_setup();
    tmrx=t_1ms;
    if(cpu_init())
        inlowpwr();
//    else while(((t_1ms-tmrx)>PWRUPTIME) && stat_bits.onpending)
//        CANCOM_Tasks();
//    if(stat_bits.onpending)
        inlowpwr();
//    else while(true){
//        CANCOM_Tasks();
//        SysTasks();
    }
//}