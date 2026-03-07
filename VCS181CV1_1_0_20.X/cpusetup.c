#include "cpusetup.h"    
//funnction will return "true" if intialization mode went OK. This does not incude checking on errors 
void cpusetup(void){
    uint16_t pd;
    
    DDPCONbits.JTAGEN=false;
    //port directions:
    TRISA=0x0680;
    LATACLR=0xC03F; //all outputs low, including unused ones
    LATASET=0x0400; //5V supply off
    OC2RS=0;
    TRISB=0xFFFF;   //all input
    TRISC=0x9010;   //Clock is input
    LATCSET=0x6000;  //disable SPI 4 DC-DC-Converter off.
    LATCCLR=0x000E; //turn low unused ones
    LATCSET=0x2000; //power down the DC/DC-converter
    TRISD=0x43E0;
    LATDCLR=0x201F; //Keep unused ports, PWM and SPI low.
    LATDSET=0x0800;
    LATDSET=0x3000; //memory CE and Reset USB high
    ODCDbits.ODCD14=ODCDbits.ODCD15=true;   //USB interface
    ODCAbits.ODCA6=true;    //Control actuator supply, P-channel MOSFET

    
//prepare CN bits
    CNPUE=0;    //no pull-up
    CNENbits.CNEN14=CNENbits.CNEN16=true;
    CNCONbits.ON=1;
    CNCONbits.SIDL=0;           //allow interrupt in idle mode
    
    TRISE=0x0200;   //All output,except battle short
    LATECLR=0x01FF; //all outputs low
    TRISF=0x002D;   // will leave F3 in input mode, buck converter is still floating
    LATFCLR=0x313A; // RS485 in read mode, fire up buck converter
    
    TRISG=0x8F;     //unused bits are input on PORT G
    LATGCLR=0xF140;
    LATGSET=0x0200; //CS for SPI ADC
    
//ADC: WIll use all 16 ports, external reference
    AD1CON1=0;    
    AD1CON1bits.SSRC=0b111;	//set to auto convert
    AD1CON1bits.CLRASAM=1;	//clear sampling automatically
    AD1CON1bits.ASAM=1;
    AD1CON1bits.SAMP=1;

    AD1CON2=0;              //Set buffer mode to 16 word buffer, forces usage of always MUX A
    AD1CON2bits.VCFG=1;		//Reference is AVSS and positive Vref
    AD1CON2bits.CSCNA=1;	//scan inputs;
    AD1CON2bits.SMPI=0b1101;    //interrupt after 14 samples, programming port will not get sampled
   
    AD1CON3=0;              //ADC clock derived from peripheral bus
    AD1CON3bits.SAMC=0b111;	//Auto Sample timer is set to 7 TAD; with TAD=200ns sampling time will be 1.4 us
    AD1CON3bits.ADCS=3;		//TAD will be 200ns; One conversion=12TAD=2.4us
    //with a total of 1.4us sampling plus 2.4us conversion time a total of 3.8us is required per channel and 53.2us for 14 channels

    AD1CHS=0;               //select VR- as negative reference for MUX A; MUX B is not used anyway
    AD1CSSL=0xFFFC;        //Do not scan ports 0 and 1 (programming) 

    AD1PCFG=0x03;
    
    //timer resources:
//timer 4 is used for general time-keeping with overflow rate set to 1ms. Will use shift register  set in ISR
    
    T4CON=0;
    T4CONbits.TCKPS=3;          //will prescale 1:8, prescaler output is 200ns
    PR4=5000;                   //will cause 1ms overflow rate
    T4CONbits.ON=1;
    
//timer 2 is used for PWM. will not need to interrupt.
//version 1.1.14 modified to drive stop solenoid as linear actuator. If configured for actuator then that will be fixed after loading conf. settings
    T2CON=0;			//see above
    T2CONbits.TCKPS=3;  //prescale 1:8, prescaler output is 200nS
    PR2=8000;	  		//8*000*0.2us=1.6ms=625 Hz
    T2CONbits.ON=1;
    
    //timer 3 is used for capture input. Will increment at with 1.6us, overflow time is about 9.5 Hz
    T3CON=0;			//see above
    T3CONbits.TCKPS=6;  //prescale 1:64, prescaler output is 1.6us
    PR3=65535;	  		//continuous overflow is required
    T3CONbits.ON=1;
    
    //timer 1 is a simple roll-over timer without interrupts. Used inside ISR to ensure long enough CS high period on ADC
    T1CON=0;        //use 1:1 pre-scaler
    PR1=65535;
    T1CONbits.ON=true;
    
//modification for version 1.1.4. OC2 now driving the stop solenoid with PWM to control engine speed.
    OC2CON=0;           //select timer 2
    OC2CONbits.OCM=6;   //run as PWM
    OC2RS=0;
    OC2CONbits.ON=true;
    
    //SPI 1, memory access. Initialize in 16 bit mode, will change to 8 bit whenever required
    SPI1CON=0;              //initialization. Will disable all framing, keep SDO as output pin. Select mode 0,0
    SPI1CONbits.MSTEN=1;	//master mode for this port. Will switch between 8 and 16 bit mode
    SPI1CONbits.CKE=1;
    SPI1BRG=3;              //run at 5 MHz         
    SPI1CONbits.ON=1;        //may run continuously
    
    //SPI4, ADC access. Master mode, 32 bits
    SPI4CON=0;                  //will default to transmit interrupting when TSR is empty,
    SPI4CONbits.MSTEN=true;    
    SPI4BRG=39;                 //select 2MHz communication speed
    SPI4CONbits.MODE32=true;
    SPI4CONbits.CKE=true;
    
    

    //USB via UART 4. Application will take care of initialization
    
    //Serial port RS485 (UART 1) not enabled yet
    //CAN communication will be set up in CAN initialization file
    
    
    //turn on interrupts:
    
    //change notification has lowest priority
    pd=PORTD;               //resolve any potential mismatch
    IPC6bits.CNIP=1;
    IPC6bits.CNIS=0;        //lowest in group 0
    IFS1bits.CNIF=0;
    IEC1bits.CNIE=1;
    
    //timers: timer 4 needs to be priority 4 to use SRS
    IPC4bits.T4IP=4;    //highest in class
    IPC4bits.T4IS=3;    //highest in sub priority
    IFS0bits.T4IF=false;
    IEC0bits.T4IE=1;
    
    //capture events
    IPC1bits.IC1IP=3;   //one below timer 4 priority
    IPC1bits.IC1IS=1;
    IPC2bits.IC2IP=3;   //same as voltage
    IPC2bits.IC2IS=0;   //inside class one below voltage
    IFS0bits.IC1IF=false;
    IFS0bits.IC2IF=false;

    
    //timer 3, just to keep track of overflow.
    //same priority as input captures but higher in class
    IPC3bits.T3IP=5;
    IPC3bits.T3IS=3;    //highest in class, 
    IFS0bits.T3IF=false;
    IEC0bits.T3IE=true;
    
    //SPI 4, ADC communication
    IPC8bits.SPI4IP=4;  //same as timer 4
    IPC8bits.SPI4IS=2;  //one below timer within class
    IFS1bits.SPI4EIF=IFS1bits.SPI4RXIF=false;
    IEC1bits.SPI4EIE=IEC1bits.SPI4RXIE=true;    //interrupt on overflow and buffer full
    
    //Set priority if communication interrupts. Will not initialize here, channel specific apps will do
    IPC7bits.U3IP=2;        //RS485 port
    IPC7bits.U3IS=2;
    
    IPC12bits.U4IP=3;       //USB port
    IPC12bits.U4IS=2;       //second-highest in class
    
    IPC11bits.CAN1IP=6;     //CAN port
    IPC11bits.CAN1IS=3;	
    tx0101.val.mwordhigh=tx0101.val.mwordlow=0;
    INTCONbits.MVEC=true;
    stfeedb.fields.pwrdwn=false;
    __builtin_enable_interrupts();
    
//if the user just turned the battery switch on then the CN ISR will not trigger. 
//To avoid an unwanted shutdown all pwr. up sources need to be checked
}

void initcap(void){
    while((!PORTDbits.RD8)&&(!PORTDbits.RD9));
    
    //Capture on pins IC1 and IC2 (RD8 and RD9)
    IC1CON=0;               //default status, use timer 3 (16bit) interrupt on every event
    IC1CONbits.ICM=3;       //capture every rising edge, every event creates an interrupt
    IC1CONbits.ON=true;
    
    IC2CON=0;               //default status, use timer 3 (16bit) interrupt on every event
    IC2CONbits.ICM=3;       //capture every rising edge, every event creates an interrupt
    IC2CONbits.ON=true;
    
    IEC0bits.IC1IE=true;
    IEC0bits.IC2IE=true;
}