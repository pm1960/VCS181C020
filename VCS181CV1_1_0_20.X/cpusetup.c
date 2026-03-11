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

bool cpuinit(void){
    uint32_t tmrx=t_1ms;
    uint16_t ubx[3];
    uint8_t i,cntr;
    bool retval=false;
    while ((t_1ms-tmrx)<10);
    LATCSET=0x4000;             //leave CS for external ADC high
    LATDSET=0x1000;         //CS memory de-asserted
    ledstat=GNRDBLK;
    LATECLR=0xFE;   //all outputs low
    TRISFCLR=0x0008;
    LATFCLR=0x0008;             //buck converter on
    for(i=0;i!=TX101LENGTH;i++)
       tx0101.trans[i]=0; 
    onpending=true;
    for(i=0;i!=13;i++)
        isrcom.bts[i]=0;
    AD1CON1bits.ASAM=true;
    AD1CON1bits.SAMP=true;
    AD1CON1bits.ON=1;      

    while(((t_1ms-tmrx)<600) && !retval){
        cntr=ubx[0]=0;
        while(cntr<64){
            while(!AD1CON1bits.DONE);
            ubx[0]=ADC1BUF1;    //battery
            cntr++;
        }
        ubx[0]>>=6;
        tx0101.val.ubatt=(uint32_t)ubx[0]*VREFL*17/10;
        retval=(tx0101.val.ubatt>VBATTMIN) ||  (tx0101.val.ubatt<VBATTMAX);
        
    }
    if(retval){
        if(ubx[0]<15)
            hwtype=ISC006;
        else if(ubx[0]<40)       //1k0 : 68K
            hwtype=ISC007;
        
        
//detect battery type, assign SC threshold
        if(ubx[2] <16000)       //12V battery
            minubatsc=7200;     //trigger if above 7.2V
        else 
            minubatsc=16000;    //trigger on 16V for any other battery
        
        readeeprom(0x1A4,&tx000A.trans[2],2);
        readeeprom(0x00,tx0130.trans_w,TR130LEN-2);  
        SETBIT(*mwordlow,LOSTINV_WARN);       //if inverter is found this bit will be cleared. If it is then lost again then an alarm status will be triggered.
        if(!inlim(tx000A.val.ecsnh>>8,20,99) || !inlim(tx000A.val.ecsnh & 0xFF, 1,12))
            SETBIT(*stwordlow,PROG_INVAL);
        
        if(!checkmirr(tx0130.trans_w))
            SETBIT(*stwordlow,PROG_INVAL);
        else{
            readeeprom(0x02B0,tx01B6.trans,5);
            ldtx0132();
            switch(*genconf_l & 0x0440){
                case 0: gp2func=GLOWPLUG;
                break;
                case 0x0040: gp2func=GNDBK;
                break;
                case 0x04000: gp2func=FLAMEPLUG;
                break;
                case 0x0440: gp2func=COOLP;
                break;
            }
            switch(*genconf_l & 0x1800){
                case 0: auxout=GDBK;
                break;
                case 0x0800: auxout=TRANSFERP;
                break;
                case 0x1000: auxout=FANRELAY;
                break;
                case 0x1800: auxout=FUELHEAT;
                break;
            }
            if((auxout==GDBK) && (gp2func==GNDBK))
               SETBIT (*stwordlow,PROG_INVAL);
            if(TESTBIT(*genconf_h,10)){
                stopsolconfig=RWPUMP;
                OC4CONbits.OCM=6;
                OC4RS=0;
            }
            else if(TESTBIT(*genconf_l,14))
                stopsolconfig=LINACT;
            else if(TESTBIT(*genconf_h,1))
                stopsolconfig=PULLCOIL;
            else if(TESTBIT(*genconf_l,2))
                stopsolconfig=EN2STOP;
            else
                stopsolconfig=EN2RUN;
        }     
        readeeprom(0x01A0,&tx0130.trans_w[TR130LEN-2],2);     //date of programming 
        readeeprom(0x1E8,tx013A.trans,6);
        if((tx013A.v32.energy>9999998) || (tx013A.v32.optime>9999998) || (tx013A.v32.starts>9999998)){
            SETBIT(*miscwordlow,SOURCECMDATA);     //will need to retrieve data from the panel
            stat_bits.nocmdata=true;    //if panel at address 0x20 has cumulative data then this bit will be cleared
        }
        readeeprom(0x01A8,tx0138.trans,32);
       
        if(TESTBIT(*miscwordlow,SOURCECMDATA)){
            valsn=perssn=0;      //invalidate all notifications, VCS is about to download data from panel
            SETBIT(tx01B6.val.status,1);       //disable warranty start, need to download
        }
        else{
            for(i=0;i!=16;i++){
                readeeprom(0x0308+i*0x40,&tx01B0.b16[4*i],4);
                readeeprom(0x0708+i*0x40,&tx01B1.b16[4*i],4);
                readeeprom(0x0310+i*0x40,&tx01B2.b16[4*i],4);
                readeeprom(0x0710+i*0x40,&tx01B3.b16[4*i],4);
                readeeprom(0x0318+i*0x40,&tx01B4.b16[4*i],4);
                readeeprom(0x0718+i*0x40,&tx01B5.b16[4*i],4);
            }
            validatesvns();
        }
        checkasc(tx0138.val.gensn,32);
        checkasc(tx0138.val.fireboy,32);

        readeeprom(0x01F4,&enrem,1);
        if(enrem>10000)
            enrem=0;

        if(TESTBIT(*stwordlow,PROG_INVAL))
            ledstat=RD_ON;
        else
            ledstat=GN_BLK;
        stat_bits.onpending=true;
        st_feedb.val.statustime=t_1ms;
        
//will now calculate the timing parameters for AC sensing for speed ranging LOWESTSPEED to HIGHESTSPEED based on the number of poles that was programmed. Unit is 400ns, 
        if(TESTBIT(*stwordlow,PROG_INVAL))
            minhalfper=maxhalfper=minlowtime=100;
        else{
            maxhalfper=(uint32_t)150000000/(LOWESTSPEED*(*poles));
            minhalfper=(uint32_t)150000000/(HIGHESTSPEED*(*poles));
            minlowtime=minhalfper*8/10;
        } 
        readeeprom(0x0200,ubx,2);
        alptr=ubx[0];
        evptr=ubx[1];
        if(((alptr & 0xFF)>MAXLOG) || ((alptr>>8)>1) || ((evptr & 0xFF)>MAXLOG) || ((evptr>>8)>1))
            reslogmem();   //alarm or event pointers out of range, erase all log data
        for(i=0;i!=40;i++){
            readeeprom(0x0B00+i*32,tx0104[i],16);       //will load last set of fast shutdown
            chkfastlog(i);                              //will clear it to zero if not within reasonable range
        }
        
//initialize request handler. 
        rq0100p.transfer=rq0100c.transfer=0x0100;
        rq0005.transfer=5;
        rq000B.transfer=0x0B;
        rq0102.transfer=0x0102;
        rq0132.transfer=0x0132;
        rq0138.transfer=0x0138;
        rq013A.transfer=0x013A;
        rq01B0.transfer=x01B0.transfer=0x01B0;
        rq01B1.transfer=x01B1.transfer=0x01B1;
        rq01B2.transfer=x01B2.transfer=0x01B2;
        rq01B3.transfer=x01B3.transfer=0x01B3;
        rq01B4.transfer=x01B4.transfer=0x01B4;
        rq01B5.transfer=x01B5.transfer=0x01B5;
        rq01B6.transfer=0x01B6;
        
        for(i=0;i!=MAXRQS;i++){     //all of the above transfers will only be requested from the panel at address 0x20
            rqs[i]->status=0;
            rqs[i]->source=0x20;
        }
        
        x01B0.status=x01B1.status=x01B2.status=x01B3.status=x01B4.status=x01B5.status=0;
        
        rq0100c.source=rq0102.source=rq0132.source=0x08;
        initspi2();   
        for(i=0;i!=8;i++)
            panels[i].sourceadr=0;
        if(!TESTBIT(*genconf_h,9) && (stopsolconfig != LINACT))
            actcalib(true);
        LATCSET=0x2000;     //tun on AVDD. Will only turn this off when entering low power mode
    }
    return(retval);
}

void chkfastlog(uint8_t row){
    uint8_t i;
    bool ok=inlim(tx0104[row][0],0,6000);   //voltage L1
    ok&=inlim(tx0104[row][1],0,6000);   //voltage L2
    ok&=inlim(tx0104[row][2],0,1000);   //current L1
    ok&=inlim(tx0104[row][3],0,1000);   //current L2
    ok&=inlim(tx0104[row][4],0,4500);   //AC input voltage
    ok&=inlim(tx0104[row][5],0,5500);   //DC link 1
    ok&=inlim(tx0104[row][6],0,5500);   //DC link 2
    ok&=inlim(tx0104[row][8],0,3800);   //demanded engine speed
    ok&=inlim(tx0104[row][9] & 0xFF,0,80);   //oil pressure
    ok&=inlim(tx0104[row][9] >>8,0,100);   //act. position
    ok&=inlim(tx0104[row][10],0,3800);   //engine speed
    ok&=inlim(tx0104[row][11],0,750);   //actuator voltage
    ok&=inlim(tx0104[row][12],0,250);   //actuator current
    ok&=inlim(tx0104[row][13],0,180);   //abs. power
    ok&=inlim(tx0104[row][14],0,36000);   //battery voltage
    if(!ok)
        for(i=0;i!=16;i++)
            tx0104[row][i]=0;
}