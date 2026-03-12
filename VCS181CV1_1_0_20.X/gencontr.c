#include "gencontr.h"
#include "LookupTable.h"
#include "cpusetup.h"
#include "usbcom.h"

//#define UONLY

uint16_t istep,actminspeed,actmaxspeed,sinfi; //current per comp. step, actuator limits
uint32_t tdisch;          //discharge time for compenasion bank 0; bank 1 is double, banks 2, and 3 are four times as high


//static functions:
static void readintad(bool initrq);
static int16_t gettemp(uint16_t adcx, uint16_t *table);
static void calctank(void);
static void actcheck(void);
static void slowreadcheck(bool eovcs,bool init);
static bool delaycheck(uint16_t val, uint16_t limval, uint32_t delay, uint32_t *tmr,
uint16_t *stat,uint8_t bitno,bool highlim);
static void checknonmirrored(void);
static void checkinfo(void);
static bool actfailoff(uint8_t *calst);
static void savalm(void);
static void savhist(void);
static bool savbuf(uint16_t adr);
static void readextadc(bool init);
static void alltest(void);
static void compcalc(bool init);
static void pidcontrol(uint16_t targval,bool isspeed,bool newpar);
static void accheck(bool init);
static bool preheat(bool init);
static bool engstart(bool init);
static bool checksys(void);
static bool checkstoprq(void);
static void procu17(void);
static void resetacchans(void);
static void calcval(uint8_t chan);
static uint32_t rootx(uint32_t val);
static void ask4transfer(uint16_t trx);
static void chkpwrdwn(void);
static void alloff(void);
static void stdefaultdata(void);
//static void checktargvolt(void);
static void chan2zero(bool init,uint8_t chan);
static bool checkremof(void);
static bool stopcycle(uint32_t tmrx);
static bool actcalib(bool init);
static bool testlinact(bool up);
static bool testservo(bool up);


void GENCONTR_Tasks (void){
    static uint32_t tmrx,tmunackof,tstop;
    static uint8_t maxfailstart,vcsupd,calfail;
    static GENCONTR_STATES goafterstop;
    static bool isalarm,unackof,stopping;
    static uint16_t regspeed;
    uint8_t i,bf[8];
    /* Check the application's current state. 
     since ISR may require a PWR DWN, any case needs to check for such request
     */
    switch ( gencontrstate ){
        case GENCONTR_STATE_INIT:       //after POR or soft reset; 
            alloff();
            vcsupd=0;
            tx0101.val.engspeed=0;
            LATCSET=0x4000;  //disable SPI 4
            LATDSET=0x0801; //Keep USB in reset, disable SPI 1
            LATECLR=0xFE;   //all outputs low
            TRISFCLR=0x0008;
            LATFCLR=0x0008;
            for(i=0;i!=35;i++)
               tx0101.trans[i]=0; 
            onpending=false;
            if(PORTDbits.RD7)
                SETBIT(tx0101.val.mwordlow,4);   //powered up by USB
            else if(PORTDbits.RD6)
                SETBIT(tx0101.val.mwordlow,5);   //powered up by remote on
            else{    
                SETBIT(tx0101.val.mwordlow,6);  //powered up by CAN
                onpending=true;
            }
            SETBIT(tx0101.val.stwordhigh,3);      //will not allow the external ADC to come on with battery out of range
            ledstat=BLKSYNC;
            readintad(true);
            for(i=0;i!=13;i++)
                isrcom.bts[i]=0;
            AD1CON1bits.ASAM=true;
            AD1CON1bits.SAMP=true;
            AD1CON1bits.ON=1;
            tmrx=t_1ms;        
            gencontrstate=GENCONTR_STATE_INITEND;
            break;
            
        case GENCONTR_STATE_INITEND:    //will need to give the CAN module enough time to power up, get going and detect that still initializing status
            if((t_1ms-tmrx)>(CANOFF+100)){
                engstart(true);     //will also initialise preheating, compcalc,
                readextadc(true);
                accheck(true);
                gencontrstate=GENCONTR_STATE_WAITPWRUP;
                goafterstop=GENCONTR_STATE_INIT;
                for(i=0;i!=5;i++)
                    stfeedb.bts[i]=0;
                unackof=false;
            }
            break;
            
        case GENCONTR_STATE_WAITPWRUP:
            if(AD1CON1bits.DONE){
            //the processor will disable the LDO if voltage is at least up to 4.8V; assuming a reference of 3.5V
            //and given the 10bit ADC and biasing resistor the output needs to be at least at 840 on the ADC output  
                if(ADC1BUF4>573){ //need to wait for voltage to build up
                    CLEARBIT(STATWRDL,2);
                    CLEARBIT(STATWRDL,3);
                    CLEARBIT(STATWRDL,0);
                    if(checkmirrored(true)){
                        if(readeeprom(0,tx0130.wrds,68)){   
//calculate discharging time. Resistors are 220kOhm. Assume the charge needs to decrease down to 1/e (app. 36%) of initial charge
//in 3~ systems one resistor is across one capacitor. Solving V=V0*e^-(t/R*C) and V/V0=e yields t=R*C. With R=.22*10^-6Ohm and C in uF (10^-6)
//the time t in ms may be calculated as 220*C, with C in uF. This is for bank 0; will be double for bank 1 and 4 times this time for banks 2..3                                
                            tdisch=220*tx0130.val.boostcap;
                            if(TESTBIT (GENCFGL,0))     //on 3~ generators caps are across line- to line-voltage but nominal voltage is line to neutral
                                                        //this introduces a factor of sqr(3) for voltage and again sqr(3) for caps assembly. Thus factor of 3 is good
                                istep=(uint32_t)((189*((tx0130.val.fnom)/10)*(tx0130.val.uacnom/10)*tx0130.val.boostcap)/1000000); //2pi and factor of 3 are rounded to 189/10

                            else    //on 120/240 generators the voltage again is line to N but caps are across the 240 line, so factor is 2
                                istep=(uint32_t)((126*((tx0130.val.fnom)/10)*(tx0130.val.uacnom/10)*tx0130.val.boostcap)/1000000); //2pi and factor of 2 are rounded 126/10
                            
                            if(istep<4)
                                SETBIT(STATWRDL,2);     //this needs to be checked or compensation might fail
                            slowreadcheck(false,true);//will initialize temperature delays
                            if(!linact){ //device is configured for servo motor, configure OC5 and un-configure OC2
                                OC2CON=OC5CON=0;
                                T2CON=0;			
                                T2CONbits.TCKPS=0b100;  //prescale 1:16, prescaler output is 400nS
                                PR2=50000;	  			//50000*0.4us=20ms=50Hz.
                                T2CONbits.ON=1;
                                OC5CONbits.OCM=0x0006;  //regular PWM
                                actcalib(true);
                                calfail=0;
                            }
                             
                        }
                    }
                    else
                        SETBIT(STATWRDL,2);      //VCS is not programmed
                    checknonmirrored();
                    checkinfo();
                    if(TESTBIT(STATWRDL,2)||TESTBIT(STATWRDL,3)||TESTBIT(STATWRDL,0)){
                        ledstat=RDBLK;
                        gencontrstate=GENCONTR_STATE_CALPROGFAIL;
                    }
                    else if(TESTBIT(MSGWRDL,6)){
                        gencontrstate=GENCONTR_STATE_CONFPWRUP;
                        onpending=true;
                        stfeedb.fields.statustime=t_1ms;
                    }
                    else{
                        ledstat=BLKSYNC;
                        gencontrstate=GENCONTR_STATE_ACTCALINICOM;
                        initcap();
                    }
                }
                AD1CON1bits.ASAM=true;
                AD1CON1bits.SAMP=true;
            }
            break;
            
        case GENCONTR_STATE_ACTCALINICOM: 
            readintad(false);
            readextadc(false);
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){     
                gencontrstate=GENCONTR_STATE_PWRDWN;
                tmrx=t_1ms;
            }
            else if(tx0101.val.engspeed>MINSPEED){
                goafterstop=gencontrstate;
                gencontrstate=GENCONTR_STATE_STOPLOOP;
            }
            else{//will do max. 3 calibration attempts. Counter is in calib. routine//will do max. 3 calibration attempts. Counter is in calib. routine
                nocan=0;        //keep device on for at least until end of act. calibration
                if(linact)
                    gencontrstate=GENCONTR_STATE_INSTOP;
                else{
                    actcheck();
                    if(actcalib(false)){ //if the calibration ended because of over- or under-voltage or overcurrent it will not be repeated
                        if(TESTBIT(tx0101.val.mwordhigh,6)){
                            if(++calfail>2)
                                gencontrstate=GENCONTR_STATE_INSTOP;
                            else{
                                CLEARBIT(tx0101.val.mwordhigh,6);
                                actcalib(true);
                            }
                        }
                        else
                            gencontrstate=GENCONTR_STATE_INSTOP;
                    }
                }
            }
            break;
    
        case GENCONTR_STATE_INSTOP:         //generator is stopped; perform regular check, omit AC check, and start if requested to do so
            if(TESTBIT(tx0101.val.miscwordlow,0))
                if((t_1ms-tstop)>3000)              //this is the re-start delay
                    CLEARBIT(tx0101.val.miscwordlow,0);
            checkremof();
            alloff();
            if(TESTBIT(tx0101.val.mwordlow,5))
                if(!PORTDbits.RD6)
                    inlowpower(true);
            unackof=false;
            readextadc(false);
            compcalc(true);
            accheck(true);
            readintad(false);
            if(TESTBIT(MSGWRDH,5)&&(cancomstate==CAN_STATE_BUSON)){
                if(!TESTBIT(vcsupd,0)){
                    if(lasttrans==0x013A)
                        SETBIT(vcsupd,0);
                    else    
                        ask4transfer(0x013A);
                }
                else if(!TESTBIT(vcsupd,1)){
                    if(lasttrans==0x013B)
                        SETBIT(vcsupd,0);
                    else    
                        ask4transfer(0x013B);
                }
                else if(!TESTBIT(vcsupd,2)){
                    if(lasttrans==0x013C)
                        SETBIT(vcsupd,0);
                    else    
                        ask4transfer(0x013C);
                }
                else
                    CLEARBIT(MSGWRDH,5);
                
            }
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){
                gencontrstate=GENCONTR_STATE_PWRDWN;
                tmrx=t_1ms;
            }
            if(checksys())
                stfeedb.fields.startinhib=false;
            else
                stfeedb.fields.startinhib=false;
            if(stfeedb.fields.newin && !TESTBIT(tx0101.val.miscwordlow,0)){
                stfeedb.fields.newin=false;
                if(stfeedb.fields.req_in){
                    LATACLR=0x0040;
                    gencontrstate=GENCONTR_STATE_PREPSTART;
                    ACTSTAT=true;
                }
            }
            break;
            
        case GENCONTR_STATE_PREPSTART:
            readintad(false);
            readextadc(false);
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){    
                gencontrstate=GENCONTR_STATE_PWRDWN;
                tmrx=t_1ms;
            }
            if(checkstoprq()){//user decided to abandon start process
                if(TESTBIT(STATWRDL,8)){ //check for demand mismatch
                    stfeedb.fields.startinhib=true;
                    AL_PENDING=true;
                    gencontrstate=GENCONTR_STATE_UNACKAL;
                    savalm();
                }
                else{
                    ACTSTAT=false;
                    gencontrstate=GENCONTR_STATE_INSTOP;
                }
            }
            else{
                if(checksys()){
                    if(preheat(false)){
                        preheat(true);
                        gencontrstate=GENCONTR_STATE_CRANKING;
                    }
                }
                else{
                    preheat(true);
                    savalm();
                    gencontrstate=GENCONTR_STATE_UNACKAL;
                }
            }
            break;
            
        case GENCONTR_STATE_CRANKING:
            readintad(false);
            readextadc(false);
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){
                gencontrstate=GENCONTR_STATE_STOPLOOP;
                goafterstop=GENCONTR_STATE_PWRDWN;
            }
            if(checkstoprq()){//user decided to abandon start process
                engstart(true);
                if(TESTBIT(STATWRDL,8)){ //check for demand mismatch
                    stfeedb.fields.startinhib=true;
                    AL_PENDING=true;
                    isalarm=true;
                }
                else{
                    ACTSTAT=false;
                    if(TESTBIT(tx0101.val.mwordlow,5))
                        goafterstop=GENCONTR_STATE_PWRDWN;
                    else
                        goafterstop=GENCONTR_STATE_UNACKAL;
                    goafterstop=GENCONTR_STATE_INSTOP;
                }
                gencontrstate=GENCONTR_STATE_STOPLOOP;
            }
            else{
                if(checksys()){
                    if(engstart(false)){//will now need to check whether engine did start or not
                        OC5CONbits.ON=true;
                        if(TESTBIT(STATWRDL,7)){        //did not start
//                            if(++maxfailstart>3)          //dry exhaust, will not need to check for max. failed starts 
//                                SETBIT(STATWRDL,12);   //this bit is persistent, will not allow the engine to restart
                            gencontrstate=GENCONTR_STATE_UNACKAL;
                        }
                        else{
                            maxfailstart=0;
                            gencontrstate=GENCONTR_STATE_IDLING;
                            unackof=false;
                            LATESET=0x0002;
                            regspeed=tx0130.val.nidle;
                            accheck(true);
                            pidcontrol(regspeed,true,true);
                            stfeedb.fields.statustime=t_1ms;
                        }   
                    }  
                }
                else{
                    engstart(true);
                    goafterstop=GENCONTR_STATE_UNACKAL;
                    gencontrstate=GENCONTR_STATE_STOPLOOP;
                    AL_PENDING=stfeedb.fields.startinhib=true;
                }
            }
            break;
            
        case GENCONTR_STATE_IDLING:     //this is a speed controlled loop, either idle or nominal
            readintad(false);
            readextadc(false);
            accheck(false);
            if(tx0101.val.engspeed<MINSPEED){
                if(unackof){
                    if((tmunackof-t_1ms)>1000){
                        goafterstop=GENCONTR_STATE_UNACKAL;
                        isalarm=true;
                        AL_PENDING=true;
                        stfeedb.fields.startinhib=true;
                        SETBIT(STATWRDL,13);
                        gencontrstate=GENCONTR_STATE_STOPLOOP;
                    }
                }
                else{
                    unackof=true;
                    tmunackof=t_1ms;
                }
                    
            }
            else
                unackof=false;
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){
                gencontrstate=GENCONTR_STATE_STOPLOOP;
                goafterstop=GENCONTR_STATE_PWRDWN;
            }
            if(checkstoprq()){//user decided to abandon start process
                if(TESTBIT(STATWRDL,8)){ //check for demand mismatch
                    stfeedb.fields.startinhib=true;
                    AL_PENDING=true;
                    goafterstop=GENCONTR_STATE_UNACKAL;
                    isalarm=true;
                }
                else{
                    ACTSTAT=false;
                    if(TESTBIT(tx0101.val.mwordlow,5))
                        goafterstop=GENCONTR_STATE_PWRDWN;
                    else
                        goafterstop=GENCONTR_STATE_INSTOP;
                }
                gencontrstate=GENCONTR_STATE_STOPLOOP;
            }
            else{ 
                if(checksys()){
                    pidcontrol(regspeed,true,false);
                    if(TESTBIT(tx0130.val.genconfig_l,15)){         
                        if(((tx0101.val.uac1+tx0101.val.uac2+tx0101.val.uac3)/3)>(tx0130.val.uacnom*8/10)){     //break loop if voltage is above 80% of nominal
                            gencontrstate=GENCONTR_STATE_WRMUP;
                            stfeedb.fields.statustime=t_1ms;
                            targvolt=tx0130.val.uacnom;
                            CLEARBIT(MSGWRDH,0);
                        }
                        
                    }
                    else if(((tx0101.val.uac1+tx0101.val.uac2)>>1)>(tx0130.val.uacnom*8/10)){
                        gencontrstate=GENCONTR_STATE_WRMUP;
                        stfeedb.fields.statustime=t_1ms;
                        targvolt=tx0130.val.uacnom;
                        CLEARBIT(MSGWRDH,0);
                    }
                    if(gencontrstate==GENCONTR_STATE_IDLING){       //keep monitoring idling status
                        if((regspeed==tx0130.val.nnom)&&((t_1ms-stfeedb.fields.statustime)>5000)&&(UAC1<(UACNOM<<1)))
                            SETBIT(MSGWRDH,0);       //warning bit if voltage does not increase within 5 seconds
                        
                        if(tx0101.val.tvcs<=COLDVCS){   //needs to idle for extended time
                            if((t_1ms-stfeedb.fields.statustime)>(tx0130.val.tidlellow*1000)){
                                regspeed=tx0130.val.nnom; 
                                stfeedb.fields.statustime=t_1ms;
                            }
                        }
                        
                        else if((t_1ms-stfeedb.fields.statustime)>(tx0130.val.tidlenorm*1000)){
                            regspeed=tx0130.val.nnom;
                            stfeedb.fields.statustime=t_1ms;
                        }
                    }           
                }
                else{   //checksys() did not pass
                    isalarm=true;
                    goafterstop=GENCONTR_STATE_UNACKAL;
                    AL_PENDING=stfeedb.fields.startinhib=true;
                    gencontrstate=GENCONTR_STATE_STOPLOOP;
                }
            }
            break;
            
        case GENCONTR_STATE_WRMUP:
            if((t_1ms-stfeedb.fields.statustime)>=tx0130.val.talm_suppr)
                gencontrstate=GENCONTR_STATE_INRUN;
             
        case GENCONTR_STATE_COOLDWN:
        case GENCONTR_STATE_INRUN:
            readintad(false);
            readextadc(false);
            accheck(false);
            if(tx0101.val.stwordlow>0)
                writeeeprom(0x01DE,&tx0101.val.stwordlow,1);
            if(tx0101.val.stwordhigh>0)
                writeeeprom(0x01E0,&tx0101.val.stwordhigh,1);
            if(tx0101.val.codeAlm>0)
                writeeeprom(0x01E2,&tx0101.val.codeAlm,1); 
            pidcontrol(targvolt,false,false);
            compcalc(false);
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){
                gencontrstate=GENCONTR_STATE_STOPLOOP;
                goafterstop=GENCONTR_STATE_PWRDWN;
            }
            if(tx0101.val.engspeed<MINSPEED){
                if(unackof){
                    if((t_1ms-tmunackof)>1000){
                        goafterstop=GENCONTR_STATE_UNACKAL;
                        isalarm=true;
                        AL_PENDING=true;
                        stfeedb.fields.startinhib=true;
                        SETBIT(STATWRDL,13);
                        gencontrstate=GENCONTR_STATE_STOPLOOP;
                    }
                }
                else{
                    unackof=true;
                    tmunackof=t_1ms;
                }
                    
            }
            else
                unackof=false;
            if(checkstoprq()){//user requested stop. enter cooldown on first call and save time
                if(TESTBIT(STATWRDL,8)){ //check for demand mismatch
                    stfeedb.fields.startinhib=true;
                    AL_PENDING=true;
                    goafterstop=GENCONTR_STATE_UNACKAL;
                    isalarm=true;
                    gencontrstate=GENCONTR_STATE_STOPLOOP;
                }
                else{   //find out if cooldown is required
                    if(TESTBIT(tx0130.val.genconfig_l,4)&&(tx0101.val.pacr<20)){
                        if(gencontrstate!=GENCONTR_STATE_COOLDWN){
                            gencontrstate=GENCONTR_STATE_COOLDWN;
                            stfeedb.fields.statustime=t_1ms;
                        }
                    }
                    else{
                        ACTSTAT=false;
                        if(TESTBIT(tx0101.val.mwordlow,5))
                            goafterstop=GENCONTR_STATE_PWRDWN;
                        else
                            goafterstop=GENCONTR_STATE_INSTOP;
                        gencontrstate=GENCONTR_STATE_STOPLOOP;
                        
                    }
                }
            }
            else if(!checksys()){
                isalarm=true;
                goafterstop=GENCONTR_STATE_UNACKAL;
                AL_PENDING=stfeedb.fields.startinhib=true;
                gencontrstate=GENCONTR_STATE_STOPLOOP;
            }
            //end cool-down period and stop the engine if the user re-applied load, if cyl. head temp. is below threshold or if cool-down period timed out
            else if(gencontrstate==GENCONTR_STATE_COOLDWN){ //end cool-down period if it timed out, if cyl. head is
                if((tx0101.val.pacr>30)||(tx0101.val.tcylhead<tx0130.val.cooldwntemp)||((t_1ms-stfeedb.fields.statustime)>(tx0130.val.cooldwntime*60000))){
                    REQSTAT=ACTSTAT=false;
                    if(TESTBIT(tx0101.val.mwordlow,5))
                        goafterstop=GENCONTR_STATE_PWRDWN;
                    else
                        goafterstop=GENCONTR_STATE_INSTOP;
                    gencontrstate=GENCONTR_STATE_STOPLOOP;  
                }
            }
            break;
            
        case GENCONTR_STATE_UNACKAL:
            alloff();
//demand mismatch needs to be resolved by flipping remote switch
            if(checkstoprq()){
                gencontrstate=GENCONTR_STATE_INSTOP;
                tstop=t_1ms;
                SETBIT(tx0101.val.miscwordlow,0);
                ACTSTAT=false;
                accheck(true);          //clear all AC alarms
                slowreadcheck(false,true);  //clear latches for temperatures and oil pressure
                CLEARBIT(tx0101.val.stwordlow,7);    //start failed
                CLEARBIT(tx0101.val.stwordlow,4);    //oil pr. switch
                CLEARBIT(tx0101.val.stwordlow,9);    //faulty oil sensor
                CLEARBIT(tx0101.val.stwordlow,13);   //unexpected stop
                CLEARBIT(tx0101.val.stwordlow,14);   //faulty ADC 
                AL_PENDING=false;
            }
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){
                gencontrstate=GENCONTR_STATE_STOPLOOP;
                goafterstop=GENCONTR_STATE_PWRDWN;
            }
            break;
            
        case GENCONTR_STATE_CALPROGFAIL:    //app usbcom or cancom may change settings, causing a status change
            readintad(false);
            readextadc(false);
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){     
                gencontrstate=GENCONTR_STATE_PWRDWN;
                tmrx=t_1ms;
            }
            
            else{
                CLEARBIT(STATWRDL,2);
                CLEARBIT(STATWRDL,3);
                if(checkmirrored(true))
                    readeeprom(0x00,tx0130.wrds,68);
                
                else
                    SETBIT(STATWRDL,2);
                checknonmirrored();
                checkinfo();
                accheck(true);
                if(!(TESTBIT(STATWRDL,2)||TESTBIT(STATWRDL,3)||TESTBIT(STATWRDL,0))){
                    gencontrstate=GENCONTR_STATE_ACTCALINICOM;
                    ledstat=BLKSYNC;
                }
            }
            break;
            
        case GENCONTR_STATE_STOPLOOP:
            if(!stopping){
                readintad(false);
                readextadc(false);
                compcalc(true);
                if(isalarm){
                    savalm();
                    isalarm=false;  
                }
                stopping=1;
            }
            else{    
                if(!linact){
                    OC2RS=0;
                    alloff();
                    gencontrstate=goafterstop;
                    tstop=t_1ms;
                    SETBIT(tx0101.val.miscwordlow,0);
                    stopping=0;
                }
                else if(stopping==1){   
                    LATACLR=0x0002;     //fuel pump off
                    OC5RS=actminspeed;
                    if(TESTBIT(tx0130.val.genconfig_l,5))
                        LATASET=0x0020;     //GND breaker
                    tmrx=t_1ms;
                    stopping=2;
                }
                else{
                    if(stopcycle(tmrx)){
                        stopping=0;
                        tstop=t_1ms;
                        SETBIT(tx0101.val.miscwordlow,0);
                        gencontrstate=goafterstop;
                    }
                }
            }
            break;
            
        case GENCONTR_STATE_DRYTEST:        //will switch through all outputs at a rate of 1 second. 
            readintad(false);               //will read all IO, and will set bits as appropriate
            readextadc(false);              //will keep reading AC lines
            alltest();
            chkpwrdwn();
            if(stfeedb.fields.pwrdwn){    
                gencontrstate=GENCONTR_STATE_PWRDWN;
                tmrx=t_1ms;
            }
            break;
            
        case GENCONTR_STATE_PWRCYCLE:         //cold start is required
            readintad(false);
            readextadc(false);
            if(tx0101.val.engspeed>MINSPEED){
                goafterstop=gencontrstate;
                gencontrstate=GENCONTR_STATE_STOPLOOP;
            }
            else{
                prepsoftres();
                cpureset();
            }
            break;
            
        case GENCONTR_STATE_PWRDWN:
            readintad(false);
            readextadc(false);
            compcalc(true);
            goafterstop=gencontrstate;
            if(tx0101.val.engspeed>MINSPEED)
                gencontrstate=GENCONTR_STATE_STOPLOOP; 
            else if(cancomstate==CAN_STATE_BUSON){  //wait 800ms to allow panels to pull tx0101 data
                if((t_1ms-tmrx)>800){ 
                    //need to broadcast pwr dwn request
                    bf[0]=0x22;
                    bf[1]=0xDD;
                    bf[2]=0x44;
                    bf[3]=0xCC;
                    if(txtrans(0x0002,bf))
                        inlowpower(true);
                }    
            }
            else
                inlowpower(true);
            break;       
            
        case GENCONTR_STATE_CONFPWRUP:  //CAN on is pending, check if valid
            if((t_1ms-stfeedb.fields.statustime)>2500)
                inlowpower(true);
            else if(!onpending){
                gencontrstate=GENCONTR_STATE_ACTCALINICOM;
                initcap();
            }
            break;

        /* The default state should never be executed. */
        default:{
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


//if called with argument true, readintad will initialize all static variables. Otherwise it will sense all
//temperatures, oil pressure, water leak, actuator voltage and current, tank, and battery voltage. It will 
//check for fault on oil pressure reading and it will further check all analog limits (warning and shutdwn)
//as well as all digital inputs. It will just set or clear bits in MessageWord, StatusWord, codeALm and 
//codeWarn. No further processing of these values occurs here, just setting  or clearing alarm or warning bits. 
//This is to avoid such functions being called more often than analog readings get updated. also the 
//function will check on updates of engine speed and power factor and update values if available. 
//for reactive current calculation the value of SinFi is also calculated.
//Engine speed, freqeuncy and power factor need to be updated within 200ms or they will be cleared.
void readintad(bool initrq){
    uint8_t i;
    int16_t coiltemp[3];
    static uint16_t lastfrq;
    static uint16_t slowcntr;
    static bool nextf;
    uint16_t rx,alfa,adc[14];
    if(initrq){
        for(i=0;i!=14;i++)
            sumbuf[i]=0;
        slowcntr=0;
        isrcom.Bits.gotfreq=0;
        isrcom.Bits.gotpf=false;
    }
    else{ 
        if(adccntr==64){
            for(i=0;i!=14;i++){
                adc[i]=adres[i]>>6;
                adc[i]=0;
            }
            adccntr=0;        
                
            if(TESTBIT(tx0130.val.sensconfig_l,0))
                tx0101.val.tcoolin=gettemp(adc[9],tch_e);  
            else tx0101.val.tcoolin=255;
            
            if(TESTBIT(tx0130.val.sensconfig_l,9))
                tx0101.val.tcoolout=gettemp(adc[13],tch_e);   
            else tx0101.val.tcoolout=255;
            
            if(TESTBIT(tx0130.val.sensconfig_l,2))
                tx0101.val.tcylhead=gettemp(adc[6],tch_e); 
            else tx0101.val.tcylhead=255;
            
            if(TESTBIT(tx0130.val.sensconfig_l,1))
                tx0101.val.tbearing=gettemp(adc[7],tch_e); 
            else tx0101.val.tbearing=255;
                

            if(TESTBIT(tx0130.val.sensconfig_l,3))
                tx0101.val.texhaust=gettemp(adc[8],tch_e);
            else tx0101.val.texhaust=255;

            if(TESTBIT(tx0130.val.sensconfig_l,4)){  
                coiltemp[0]=gettemp(adc[10],tch_e);
                coiltemp[1]=gettemp(adc[11],tch_e);
                coiltemp[2]=gettemp(adc[12],tch_e);
//find hottest coil temp. 
                tx0101.val.tcoil=0;
                for(i=0;i!=3;i++)
                    if(coiltemp[i]!=255)
                        if(coiltemp[i]>tx0101.val.tcoil)
                            tx0101.val.tcoil=coiltemp[i];
                if(tx0101.val.tcoil==0)
                    tx0101.val.tcoil=255;       //all sensors failed
                }
                else tx0101.val.tcoil=255;
//water leak reports ground fan breaker on or off            
            if(TESTBIT(tx0130.val.genconfig_h,3)){
                if(adc[3]>256)
                    SETBIT(tx0101.val.stwordhigh,10);
                else if(!stfeedb.fields.alpending)
                    CLEARBIT(tx0101.val.stwordhigh,10);
            }
            if(TESTBIT(tx0130.val.sensconfig_l,5)){  //active sensor is enabled. Supply is not measured, so always assume it is exactly 5V will multiply voltage by 2 (shift by 9 only) because of resistive divider made up by R23/R24
                                                    //Data sheet formula translates to calculation in units of 100mbar into p=125*Vx/Vcc-12.5; for subtraction of 12.5 first a ratio of 10fold will be used    
                adc[0]=(((uint32_t)adc[0]*VREFL*1250)>>9)/5000;//translate into voltage sensed at sensor, unit 1mV. Assume supply is 5000mV
                if(adc[0]>125){
                    adc[0]=(adc[0]-125)/10;
                    tx0101.val.poil=(uint8_t)(adc[0]);
                }
                else
                    tx0101.val.poil=0;
            }
            else 
                tx0101.val.poil=255;
            if(TESTBIT(tx0130.val.sensconfig_l,6)){
                tx0101.val.tankres=(uint32_t)adc[2]*300/(1024-adc[2]);      //biasing resistor s 300Ohm
                calctank();
            }
            tx0101.val.ubatt=(int32_t)adc[1]*VREFL*17/10;
            if(tx0101.val.ubatt<BATMIN){
                if(gencontrstate==GENCONTR_STATE_CRANKING)
                    readextadc(true);       //initialize SPI 4
                else SETBIT(tx0101.val.stwordlow,3);   
            }
            else if(stfeedb.fields.alpending)
                CLEARBIT(tx0101.val.stwordlow,3); 
            tx0101.val.actu=(uint32_t)adc[4]*VREFL*3426/10000;
            if(adc[5]>adc[4])
                tx0101.val.acti=0;
            else
                tx0101.val.acti=((((adc[4]-adc[5])*VREFL))/10*(604+249)/249)*455/100;  //this is delta U across R112 in units of 10mV. Then multiply with 4.55 because of sense resistor 0R22.
            isrcom.Bits.actidone=1;

            AD1CON1bits.ASAM=1;
            AD1CON1bits.SAMP=1;             //restart sampling
        }
        procu17();                          //read ADC U17, VCS temperature and eng. oil temperature
        
        if((isrcom.Bits.gotfreq)&&(isrcom.Bits.period>200)&&(!TESTBIT(STATWRDL,2))){  //avoid calculation if VCS is not programmed. COuld cause division by 0
            if(nextf){
                pwmspeed=((6250000/isrcom.Bits.period)*12)/tx0130.val.poles;        //calculate momentary speed using momentary frequency
                tx0101.val.fac=(tx0101.val.fac*3+(6250000/isrcom.Bits.period))>>2;
                tx0101.val.engspeed=(tx0101.val.engspeed*3+tx0101.val.fac*12/tx0130.val.poles)>>2;
                gotspeed=true;
            }
            else
                nextf=true;
            isrcom.Bits.gotfreq=false;
            lastfrq=t_1ms;
        }      
        else if((t_1ms-lastfrq)>800){    //this is to detect missing frequency signal. Monitoring routine will detect unexpected stop
            nextf=false;
            lastfrq=t_1ms;
            isrcom.Bits.lastupuls=false;
            isrcom.Bits.gotpf=false;
            sinfi=0;
            tx0101.val.cosfi=0;
            tx0101.val.engspeed=pwmspeed=0;
            tx0101.val.pac=0;
            tx0101.val.fac=0;
            tx0101.val.pacr=0;
            gotspeed=true;
        }
        
        if((isrcom.Bits.gotpf)&&(isrcom.Bits.period>200)){//calculate power factor and reactive current, if inductive
            alfa=isrcom.Bits.phshift*360/isrcom.Bits.period;//will calculate phase shift angle with resolution 1 degree
            if(alfa>90)
                alfa=90;
            tx0101.val.cosfi=cosalfa[alfa]/10;    //lookup-table is made with resolution 1/1000
            if(isrcom.Bits.iscap)
                SETBIT(tx0101.val.cosfi,7);
            sinfi=cosalfa[90-alfa];
            isrcom.Bits.gotpf=0;
            CLEARBIT(MSGWRDH,3);
            tx0101.val.pac=(tx0101.val.uac1*tx0101.val.iac1/10000+
                    tx0101.val.uac2*tx0101.val.iac2/10000+
                    tx0101.val.uac3*tx0101.val.iac3/10000)*cosalfa[alfa]/1000;  //calculate power
            if(!TESTBIT(STATWRDL,2))
                tx0101.val.pacr=tx0101.val.pac*200/tx0130.val.acpnom;
        }
        
        else if((t_1ms-isrcom.Bits.lastitime)>200){ //could not read square signal from current input
            tx0101.val.cosfi=0;
            sinfi=0;                    //disable compensation
            if(tx0101.val.iac1>30)
                SETBIT(MSGWRDH,3);
            else
                CLEARBIT(MSGWRDH,3);
            tx0101.val.pac=(tx0101.val.uac1*tx0101.val.iac1/10000+
                    tx0101.val.uac2*tx0101.val.iac2/10000+
                    tx0101.val.uac3*tx0101.val.iac3/10000);     //calculate power assuming Power factor is 1
            if(!TESTBIT(STATWRDL,2)) //avoid calculation if not programmed. Might result in division by 0
                tx0101.val.pacr=tx0101.val.pac*200/tx0130.val.acpnom;
        }
        slowreadcheck(false,false);
    }
}


//will return temperature in degree celsius related to -20deg if *intex points to internal temp, otherwise related to -50deg.c
int16_t gettemp(uint16_t adcx, uint16_t *table){
    int16_t i=*table++;
    if(adcx<*table++)       //sensor shorted
        return(255);
    else if(adcx<*table)    //sensor not connected
        return(255);
    else while(adcx<=*table++)
        i++;
    return(i);
}

//this function will be called only from inside readad(0) after getting a valid actuator current and batt. voltage
//It will maintain actuator status in codeWarn and codeAlm and in message word.
//Actuator warnings will be delayed 100ms, actuator shutdowns by D_MAXACT
void actcheck(void){
    static uint16_t stat;    //bit 0 is actuator current high, warning, 
                             //bit 1 is actuator current shutdown
                             //bit 2 is actuator voltage low warning, 
                             //bit 3 is actuator voltage high warning, 
                             //bit 4 is actuator voltage high shutdown
                             //bit 5 is battery voltage low, warning
                             //bit 6 is battery voltage high, warning
                             //bit 7 is battery voltage high, shutdown
                             //bit 8 shows actuator is hitting limits while operating
    static uint32_t tdelayact[8];   //timer order same as bit order
        if(delaycheck(tx0101.val.acti,ACTIMAXW,100, &tdelayact[0],&stat,0,true) ||
                delaycheck(tx0101.val.actu,ACTUMINW,500, &tdelayact[2],&stat,2,false) ||
                delaycheck(tx0101.val.actu,ACTUMAXW,500, &tdelayact[3],&stat,3,true)
                )
            SETBIT(tx0101.val.codeWarn,13);     //ACT. CURRENT HIGH 
        else
            CLEARBIT(tx0101.val.codeWarn,13);
    
        if(delaycheck(tx0101.val.acti,ACTIMAXO,D_MAXACT, &tdelayact[1],&stat,1,true) || 
                delaycheck(tx0101.val.actu,ACTUMAXO,D_MAXACT, &tdelayact[4],&stat,4,true))
            SETBIT(tx0101.val.codeAlm,13);     
        else if(!AL_PENDING)
            CLEARBIT(tx0101.val.codeAlm,13);

    if(delaycheck(tx0101.val.ubatt,tx0130.val.udcloww,100, &tdelayact[5],&stat,5,false))
        SETBIT(tx0101.val.miscwordlow,0);     //batt. voltage low
    else
        CLEARBIT(tx0101.val.mwordlow,0);
    
    if(delaycheck(tx0101.val.ubatt, tx0130.val.udchighw,100, &tdelayact[6],&stat,6,true))
        SETBIT(tx0101.val.codeWarn,4);         //batt. voltage high
    else
        CLEARBIT(tx0101.val.codeWarn,4);
    
    if(delaycheck(tx0101.val.ubatt, tx0130.val.udchigho,tx0130.val.d_dchigho, &tdelayact[7],&stat,7,true))
        SETBIT(tx0101.val.codeAlm,4);         //batt. voltage high
    else if(!stfeedb.fields.alpending)
        CLEARBIT(tx0101.val.codeAlm,4);
}

//this function checks limits for all enabled analog readings and for all digital inputs including emergency shutdown
//warnings and digital readings are averaged 500ms, shutdowns as defined in program data. 
//if called with parameter eovcs==true then only Engine oil temp. and VCS temp. will be checked
//VCS temperature warning is fix set to 85deg. C (125 with offset)
//if init==true then delays will be calculated
void slowreadcheck(bool eovcs,bool init){
    static uint16_t stw,sta;    //bit 0 is VCS internal temperature (warning only)
                                //bit 1 is engine oil temperature
                                //bit 2 is oil pressure
                                //bit 3 is oil pressure switch (alarm only)
                                //bit 4 is tank level (warning only)
                                //bit 5 is water leak (alarm only)
                                //bit 6 is radiator fan (alarm only)
                                //bit 7 is air intake switch (warning only)
                                //bit 8 is cylinder head temperature 
                                //bit 9 is alternator bearing temperature
                                //bit 10 is exhaust elbow temperature
                                //bit 11 is coolant in temperature
                                //bit 12 is coolant out temperature 
                                //bit 13 is coil temperature (evaluating hottest one only)
                                //bit 14 is comp. h.s. 
                                //bit 15 is emergency stop (shutdown only)
    
    static uint32_t tw[16],ta[16];
    uint16_t deloil;
    static uint32_t delay_temps,d_tank;
    if(init){
        delay_temps=tx0130.val.d_altemps*100;
        d_tank=tx0130.val.d_tankw*1000;
        stw=sta=0;
    }
    else if(eovcs){
        if(delaycheck(tx0101.val.tvcs,125,500,&tw[0],&stw,0,true))
            SETBIT(tx0101.val.mwordlow,2);
        
        else
            CLEARBIT(tx0101.val.mwordlow,2);
        if(delaycheck(tx0101.val.tengoil,tx0130.val.engoilw,500,&tw[1],&stw,1,true))
            SETBIT(tx0101.val.codeWarn,7);
        else
            CLEARBIT(tx0101.val.codeWarn,7);
        if(delaycheck(tx0101.val.tengoil,tx0130.val.engoilo,delay_temps,&ta[1],&sta,1,true))
            SETBIT(tx0101.val.codeAlm,7);
        else if(!stfeedb.fields.alpending)
            CLEARBIT(tx0101.val.codeAlm,7);     
    }
    else{// all other inputs, except VCS temp. and oil temp.
        //Monitor oil pressure only if engine is running, or if in test mode
        if((tx0101.val.engspeed>MINSPEED)||(gencontrstate==GENCONTR_STATE_DRYTEST)){
            if(tx0101.val.tvcs<=COLDVCS){
                if(tx0130.val.d_poillowolow>65)
                    deloil=0xFFF0;
                else
                    deloil=tx0130.val.d_poillowolow*1000;
            }
            else
                deloil=tx0130.val.d_poillowonorm*100;
            if(delaycheck(tx0101.val.poil,tx0130.val.poiloww,500,&tw[2],&stw,2,false))
                SETBIT(tx0101.val.codeWarn,12);
            else
                CLEARBIT(tx0101.val.codeWarn,12);
            if(delaycheck(tx0101.val.poil,tx0130.val.poillowo,deloil,&ta[2],&sta,2,false))
                SETBIT(tx0101.val.codeAlm,12);
            else if(!stfeedb.fields.alpending)
                CLEARBIT(tx0101.val.codeAlm,12);
            if(TESTBIT(GENCFGL,3)){  
                if(PORTGbits.RG0){
                    if(TESTBIT(sta,3)){
                        if((t_1ms-ta[3])>deloil) //same delay as for analog reading
                            SETBIT(tx0101.val.stwordlow,4);
                    }
                    else{
                        SETBIT(sta,3);
                        ta[3]=t_1ms;
                    }
                }
                else{
                    CLEARBIT(sta,3);
                    if(!stfeedb.fields.alpending)
                        CLEARBIT(tx0101.val.stwordlow,4);
                }
            }
        }
        else{
            if(!stfeedb.fields.alpending){
                CLEARBIT(tx0101.val.stwordlow,4);
                CLEARBIT(tx0101.val.codeAlm,12);
            }
            CLEARBIT(tx0101.val.codeWarn,12);
            CLEARBIT(stw,2);
            CLEARBIT(sta,2);
            CLEARBIT(sta,3);
        }
        
        if(delaycheck(tx0101.val.tank, tx0130.val.tankw,d_tank,&tw[4],&stw,4,false))
            SETBIT(tx0101.val.mwordlow,11);
        else
            CLEARBIT(tx0101.val.mwordlow,11);
        
        
        if(TESTBIT(GENCFGL,0)){  //radiator fan monitoring is enabled
            if(PORTAbits.RA7){
                if(TESTBIT(sta,6)){
                    if((t_1ms-ta[6])>500)
                        SETBIT(tx0101.val.stwordlow,5);
                }
                else{
                    SETBIT(sta,6);
                    ta[6]=t_1ms;
                }
            }
            else{
                CLEARBIT(sta,6);
                if(!stfeedb.fields.alpending)
                    CLEARBIT(tx0101.val.stwordlow,5);
            }
        }
        else
            CLEARBIT(tx0101.val.stwordlow,5);
         
        if(TESTBIT(GENCFGL,2)){  //air intake monitoring is enabled
        if(PORTAbits.RA9){
            if(TESTBIT(stw,7)){
                if((t_1ms-tw[7])>500)
                    SETBIT(tx0101.val.mwordlow,3);
                }
                else{
                    SETBIT(stw,7);
                    tw[7]=t_1ms;
                }
            }
            else{
                CLEARBIT(stw,7);
                CLEARBIT(tx0101.val.mwordlow,3);
            }
        }
        else
            CLEARBIT(tx0101.val.mwordlow,3);
        
        if(delaycheck(tx0101.val.tcylhead,tx0130.val.cylhw,500,&tw[8],&stw,8,true))
            SETBIT(C_WARN,8);
        else
            CLEARBIT(C_WARN,8);
        
        if((gencontrstate==GENCONTR_STATE_INRUN)&&(delaycheck(tx0101.val.tcylhead,tx0130.val.cylho,delay_temps,&ta[8],&sta,8,true)))
            SETBIT(C_ALM,8);
        else if(!AL_PENDING)
            CLEARBIT(C_ALM,8);
        
        if(delaycheck(tx0101.val.tbearing,tx0130.val.altbw,500,&tw[9],&stw,9,true))
            SETBIT(tx0101.val.codeWarn,9);
        else
            CLEARBIT(tx0101.val.codeWarn,9);

        if(delaycheck(tx0101.val.tbearing,tx0130.val.altbo,delay_temps,&ta[9],&sta,9,true))
            SETBIT(tx0101.val.codeAlm,9);
        else if(!stfeedb.fields.alpending)
            CLEARBIT(tx0101.val.codeAlm,9);       
        
        if(delaycheck(tx0101.val.texhaust,tx0130.val.exhmw,500,&tw[10],&stw,10,true))
            SETBIT(tx0101.val.codeWarn,10);
        else
            CLEARBIT(tx0101.val.codeWarn,10);
        if((gencontrstate==GENCONTR_STATE_INRUN)&&(delaycheck(tx0101.val.texhaust,tx0130.val.exhmo,delay_temps,&ta[10],&sta,10,true)))
            SETBIT(tx0101.val.codeAlm,10);
        else if(!stfeedb.fields.alpending)
            CLEARBIT(tx0101.val.codeAlm,10);
        
        if(delaycheck(tx0101.val.tcoolin,tx0130.val.coolinw,500,&tw[11],&stw,11,true))
            SETBIT(tx0101.val.codeWarn,5);
        else
            CLEARBIT(tx0101.val.codeWarn,5);
        if((gencontrstate==GENCONTR_STATE_INRUN)&&(delaycheck(tx0101.val.tcoolin,tx0130.val.coolino,delay_temps,&ta[11],&sta,11,true)))
            SETBIT(tx0101.val.codeAlm,5);
        else if(!stfeedb.fields.alpending)
            CLEARBIT(tx0101.val.codeAlm,5);        
        
        if(delaycheck(tx0101.val.tcoolout,tx0130.val.cooloutw,500,&tw[12],&stw,12,true))
            SETBIT(tx0101.val.codeWarn,6);
        else
            CLEARBIT(tx0101.val.codeWarn,6);
        if((gencontrstate==GENCONTR_STATE_INRUN)&&(delaycheck(tx0101.val.tcoolout,tx0130.val.coolouto,delay_temps,&ta[12],&sta,12,true)))
            SETBIT(tx0101.val.codeAlm,6);
        else if(!stfeedb.fields.alpending)
            CLEARBIT(tx0101.val.codeAlm,6);     
        
        if(delaycheck(tx0101.val.tcoil,tx0130.val.coilw,500,&tw[13],&stw,13,true))
            SETBIT(tx0101.val.codeWarn,11);
        else
            CLEARBIT(tx0101.val.codeWarn,11);
        if((gencontrstate==GENCONTR_STATE_INRUN)&&(delaycheck(tx0101.val.tcoil,tx0130.val.coilo,delay_temps,&ta[13],&sta,13,true)))
            SETBIT(tx0101.val.codeAlm,11);

        else if(!stfeedb.fields.alpending)
            CLEARBIT(tx0101.val.codeAlm,11);     
        
        if(delaycheck(tx0101.val.tcomp,tx0130.val.comphsw,500,&tw[14],&stw,14,true))
            SETBIT(MSGWRDL,14);
        else
            CLEARBIT(MSGWRDL,14);
        if(delaycheck(tx0101.val.tcomp,tx0130.val.comphso,delay_temps,&ta[14],&sta,14,true))
            SETBIT(MSGWRDL,15);
        else
            CLEARBIT(MSGWRDL,15);  
        
        
        if(TESTBIT(GENCFGL,6)){  //Emergency stop is enabled       
            if(!PORTDbits.RD5){
                if(TESTBIT(sta,15)){
                    if((t_1ms-ta[15])>500)
                        SETBIT(tx0101.val.stwordhigh,4);
                }
                else{
                    SETBIT(sta,15);
                    ta[15]=t_1ms;
                }
            }
            else{
                CLEARBIT(sta,15);
                CLEARBIT(tx0101.val.stwordhigh,4);
            }
        }
        else
            CLEARBIT(tx0101.val.stwordhigh,4);
    }//else, anything else but VCS temperature and eng. oil temp. 
}

    //will check all AC readings. warnings are delayed 500ms, alarms as programmed
    //if init is set, all delay bits will be cleared
    //will maintain operating time, history data logging and energy counter
void accheck(bool init){
    static uint32_t enerclock;
    static uint8_t storcntr;
    uint16_t wrx[4];
    bool leg123;
    static bool isimaxo,ispmaxo,isimaxw,ispmaxw;
    static uint16_t stw,sta;            //bit 0 is voltage low leg1
    static uint32_t ta[15],tw[15];      //bit 1 is voltage low leg2
    static uint32_t runts,hysts;        //bit 2 is voltage low leg3    
                                        //bit 3 is voltage high leg1
                                        //bit 4 is voltage high leg2
                                        //bit 5 is voltage high leg3    
                                        //bit 6 is current high leg1
                                        //bit 7 is current high leg2
                                        //bit 8 is current high leg3 
                                        //bit 9 is power high
                                        //bit 10 is frequency low (warning only)
                                        //bit 11 is frequency high (warning only)
                                        //bit 12 current leg 1, warning level extended (Alarm only)
                                        //bit 13 current leg 2, warning level extended (Alarm only)
                                        //bit 14 current leg 3, warning level extended (Alarm only)
                                        //bit 15:Power, warning level extended (Alarm only)
    
    if(init){
        stw=sta=0;  //clear all alarms related to AC and speed output
        CLEARBIT(MSGWRDL,1); //clear frequency alarm
        C_WARN&=0xBFF0;      //clear all AC and speed related bits
        C_ALM&=0xBFF0;
        isimaxo=ispmaxo=isimaxw=ispmaxw=false;
    }
    else{
        if((gencontrstate==GENCONTR_STATE_INRUN)||
            (gencontrstate==GENCONTR_STATE_WRMUP)||
                (gencontrstate==GENCONTR_STATE_COOLDWN)){    //low voltage check only while running! 
            leg123=(delaycheck(tx0101.val.uac1,tx0130.val.acvloww,500,&tw[0],&stw,0,false)||
                        (delaycheck(tx0101.val.uac2,tx0130.val.acvloww,500,&tw[1],&stw,1,false)));
            if(TESTBIT(GENCFGL,0))
                leg123|=delaycheck(tx0101.val.uac3,tx0130.val.acvloww,500,&tw[2],&stw,2,false);
            if(leg123)
                SETBIT(C_WARN,1);
            else
                CLEARBIT(C_WARN,1);

            leg123=delaycheck(tx0101.val.uac1,tx0130.val.acvlowo,tx0130.val.d_umin,&ta[0],&sta,0,false)||
                        delaycheck(tx0101.val.uac2,tx0130.val.acvlowo,tx0130.val.d_umin,&ta[1],&sta,1,false);
            if(TESTBIT(GENCFGL,0))
                leg123|=delaycheck(tx0101.val.uac3,tx0130.val.acvlowo,tx0130.val.d_umin,&ta[2],&sta,2,false);
            if(leg123)
                SETBIT(C_ALM,1);
            else if(!AL_PENDING)
                CLEARBIT(C_ALM,1);        
        }
        else if(!AL_PENDING)
            CLEARBIT(C_ALM,1);   
        
        leg123=(delaycheck(tx0101.val.uac1,tx0130.val.acvhighw,500,&tw[3],&stw,3,true)||
                        (delaycheck(tx0101.val.uac2,tx0130.val.acvhighw,500,&tw[4],&stw,4,true)));
        if(TESTBIT(GENCFGL,0))
            leg123|=delaycheck(tx0101.val.uac3,tx0130.val.acvhighw,500,&tw[5],&stw,5,true);
        if(leg123)
            SETBIT(C_WARN,0);
        else
            CLEARBIT(C_WARN,0);
        
        leg123=(delaycheck(tx0101.val.uac1,tx0130.val.acvhigho,tx0130.val.d_umax,&ta[3],&sta,3,true)||
                        (delaycheck(tx0101.val.uac2,tx0130.val.acvhigho,tx0130.val.d_umax,&ta[4],&sta,4,true)));
        if(TESTBIT(GENCFGL,0))
            leg123|=delaycheck(tx0101.val.uac3,tx0130.val.acvhigho,tx0130.val.d_umax,&tw[5],&stw,5,true);
        if(leg123)
            SETBIT(C_ALM,0);
        else if(!AL_PENDING)
            CLEARBIT(C_ALM,0);
        
        leg123=(delaycheck(tx0101.val.iac1,tx0130.val.acihighw,500,&tw[6],&stw,6,true))||
                (delaycheck(tx0101.val.iac2,tx0130.val.acihighw,500,&tw[7],&stw,7,true));
        if(TESTBIT(GENCFGL,0))
            leg123|=delaycheck(tx0101.val.iac3,tx0130.val.acihighw,500,&tw[8],&stw,8,true);
        if(leg123)
            SETBIT(C_WARN,2);
        else
            CLEARBIT(C_WARN,2);
                    
        leg123=(delaycheck(tx0101.val.iac1,tx0130.val.acihigho,tx0130.val.d_imax,&ta[6],&sta,6,true))||
                (delaycheck(tx0101.val.iac2,tx0130.val.acihigho,tx0130.val.d_imax,&ta[7],&sta,7,true));  
        if(TESTBIT(GENCFGL,0))
            leg123|=delaycheck(tx0101.val.iac3,tx0130.val.acihigho,tx0130.val.d_imax,&ta[8],&sta,8,true);
        if(leg123)
            isimaxo=true;
        else
            isimaxo=false;
        
        if(delaycheck(tx0101.val.pac,tx0130.val.acphighw,500,&tw[9],&stw,9,true))
            SETBIT(C_WARN,3);
        else
            CLEARBIT(C_WARN,3);
        if(delaycheck(tx0101.val.pac,tx0130.val.acphigho,tx0130.val.d_pmax,&ta[9],&sta,9,true))
            ispmaxo=true;
        else
            ispmaxo=false;
        
        if((delaycheck(tx0101.val.fac,tx0130.val.floww,2000,&tw[10],&stw,10,false))||
                (delaycheck(tx0101.val.fac,tx0130.val.fhighw,2000,&tw[11],&stw,11,true)))
            SETBIT(MSGWRDL,1);
        else
            CLEARBIT(MSGWRDL,1);
        
        leg123=(delaycheck(tx0101.val.iac1,tx0130.val.acihighw,(tx0130.val.d_iwarn*1000),&ta[12],&sta,12,true))||
                (delaycheck(tx0101.val.iac2,tx0130.val.acihighw,(tx0130.val.d_iwarn*1000),&ta[13],&sta,13,true));  
        if(TESTBIT(GENCFGL,0))
            leg123|=delaycheck(tx0101.val.iac3,tx0130.val.acihighw,(tx0130.val.d_iwarn*1000),&ta[14],&sta,14,true);
        if(leg123)
            isimaxw=true;
        else
            isimaxw=false;
        
        if(delaycheck(tx0101.val.pac,tx0130.val.acphighw,(tx0130.val.d_pwarn*1000),&ta[15],&sta,15,true))
            ispmaxw=true;
        else 
            ispmaxw=false;

        if(isimaxw||isimaxo)
            SETBIT(C_ALM,2);
        else if(!AL_PENDING)
            CLEARBIT(C_ALM,2);
        
        if(ispmaxw||ispmaxo)
            SETBIT(C_ALM,3);
        else if(!AL_PENDING)
            CLEARBIT(C_ALM,3);
        
        if(tx0101.val.engspeed>MINSPEED){
            if(TESTBIT(MSGWRDH,5)) //still got junk data from being reprogrammed. Will now assume no data from panels is available and overwrite everything
                stdefaultdata();
            if((t_1s-runts)>=60){
                runts=t_1s;
                tx013A.v32.optime++;
                writeeeprom(0x01A4,&tx013A.val.runtl,2);
                if((t_1s-hysts)>=600){//every 10 minutes
                    savhist();
                    hysts=t_1s;
                }
                tx0101.val.runlast++;
            }
            //calculate total anergy since last start and cumulated. 
            if((t_1ms-enerclock)>3600){//integrate total energy with a time resolution if 3.6sec.
                enerclock=t_1ms;
                if(tx0101.val.pac>0){
                    enrem+=tx0101.val.pac;       //PAC is in units of 100W. Integrated over 3.6 seconds yields units of 0.1wh.
                    if(enrem>10000){             //every 10000 increments of enrem yields 1kwh
                        tx013A.v32.energy+=enrem/10000;
                        tx0101.val.enlast+=enrem/10000;
                        enrem=enrem%10000;
                        wrx[0]=tx013A.val.totalenl;
                        wrx[1]=tx013A.val.totalenh;
                        wrx[2]=enrem;
                        wrx[3]=enrem>>16;
                        writeeeprom(0x01D4,wrx,4);
                        storcntr=0;
                    }
                    else if(++storcntr>50){ //save energy reminder every 50x3.6 sec
                        storcntr=0;
                        wrx[0]=enrem;
                        wrx[1]=enrem>>16;
                        writeeeprom(0x01D8,wrx,2);
                    }
                } 
            }
        }
    } 
}


//function delaycheck will perform a delay check for all kind of alarms and will return true if that alarm
//needs to be set. It requires as an input the actual value of the variable to be monitored,the threshold,
//the required delay in ms, a pointer to the timer variable assigned to that delay, a pointer to a 16bit status
//variable holding a status bit of whether that alarm was already pending or not, a bit counter showing which
//particular bit in that status variable is assigned to this alarm and input of whether high or low limit is monitored
bool delaycheck(uint16_t val, uint16_t limval, uint32_t delay, uint32_t *tmr,
    uint16_t *stat,uint8_t bitno,bool highlim){
    bool retval=false,isalarm=false;
    if(highlim){//monitor for above high limit
        if(val>limval)
            isalarm=true;
    }
    else{
        if(val<limval)
            isalarm=true;
    }
    if(isalarm){
        if(TESTBIT(*stat,bitno)){
            if((t_1ms-*tmr)>delay)
                retval=true;        //delay expired.
        }
        else{
            SETBIT(*stat,bitno);
            *tmr=t_1ms;
        }
    }
    else
        CLEARBIT(*stat,bitno);
    return(retval);
}

//will read and check non-mirrored data 
void checknonmirrored(void){
    uint8_t i;
    bool ok=true;
    int16_t *ibp;
    uint16_t *ubp;
    if(readeeprom(0x01AC,tx0131.wrds,20)){
        if((ALPTR&0xFF)>79){
            ALPTR=0;
            ok=false;
        }
        if((EVPTR&0xFF)>79){
            EVPTR=0;
            ok=false;
        }
        ibp=&tx0131.val.ofssu1;
        for(i=0;i!=9;i++){
            if((*ibp<-500)||(*ibp>500)){
                *ibp=0;
                ok=false;
            }
            ibp++;
        }
        ubp=&tx0131.val.factu1;
        for(i=0;i!=9;i++){
            if((*ubp<750)||(*ubp>1250)){
                *ubp=1000;
                ok=false;
            }
            ubp++;
        }

        if(!ok)
            writeeeprom(0x01AC,tx0131.wrds,20);
    }
}



//check data as per transfer 0x0138. If data is out of range than it will be set to all zero. Message word 5 bit will be set to indicate this
//operating time will be placed in tx0101 as well
void checkinfo(void){   
    bool cont;
    uint16_t rwbuf[16];
    uint8_t i,k=0;
    if(readeeprom(0x0180,rwbuf,16)){  //read gen. serial No. This is an ASCII string, first byte is first in memory. 
        cont=true;
        i=0;
        while(cont&&(i<16)){
            if(((rwbuf[i]>>8)>=0x20)&&((rwbuf[i]>>8)<=0x7F)){
                tx0138.val.gensn[2*i]=rwbuf[i]>>8;
                k++;
            }
            else
                cont=false;   //break the loop!
            if((cont)&&((rwbuf[i]&0xFF)>=0x20)&&((rwbuf[i]&0xFF)<=0x7F)){
                tx0138.val.gensn[2*i+1]=rwbuf[i]&0xFF;
                k++;
            }
            else{
                cont=false;
                tx0138.val.gensn[2*i+1]=0;
            }
            i++;
        }
        if(k<4)     //any valid serial No. needs to exceed 4 digits!         
            serinval(tx0138.val.gensn);
        else while(i<16){
            tx0138.val.gensn[2*i]=0;   //if serial No is less than 32 bytes fill up with all zero's
            tx0138.val.gensn[2*i+1]=0;
            i++;
        }
    }
    readeeprom(0x01A4,tx013A.trans,4);
    readeeprom(0x01D4,rwbuf,4);
    tx013A.val.totalenl=rwbuf[0]; 
    tx013A.val.totalenh=rwbuf[1];
    enrem=rwbuf[2]|(rwbuf[3]<<16);
    if(enrem>10000)
        enrem=0;
    if(tx013A.v32.optime>9999998){
        SETBIT(MSGWRDH,5);
        for(i=0;i!=6;i++)
            tx013A.trans[i]=0;
        enrem=0;
    }
    else
        CLEARBIT(MSGWRDH,5);
}

void readextadc(bool init){//will process external ADC reading
    static uint16_t s4fails;
    uint32_t rxclc;
    uint16_t uhv;
    uint8_t i;
    if(init){
        if(!TESTBIT(tx0101.val.stwordlow,3)){
            resetacchans();
            s4fails=uhv=tx0101.val.tcomp=0;
            UAC1=UAC2=UAC3=IAC1=IAC2=IAC3=0;
        }
    }
    else if((TESTBIT(tx0101.val.stwordlow,3))||isrcom.Bits.spi4fail){//ADC sensing is disabled because of voltage problems. Routine readintad will fix this
        resetacchans();
        UAC1=UAC2=UAC3=IAC1=IAC2=IAC3=0;
        tx0101.val.tcomp=uhv=0;
//        if(isrcom.Bits.spi4fail)
//            if(++s4fails>10)
//                SETBIT(STATWRDL,14);
    }
    else{
//if any ADC channel is found erratic then that channel will be cleared, reading will be set to zero and messageword bit 18 will be set       
        CLEARBIT(MSGWRDH,2);
        s4fails=0;
        for(i=0;i!=8;i++){
            if(adcch[i].stat==EXT_ADC_TIMEOUT){
                SETBIT(MSGWRDH,2);
                asm("di");
                adcch[i].adccntr=0;
                adcch[i].adcdata=0;
                adcch[i].stat=EXT_ADC_WAIT4ZERO;
                asm("ei");
                chan2zero(false,i);   //immediately make that channel 0 
            }
            else if(adcch[i].stat==EXT_ADC_READZERO){
                asm("di");
                adcch[i].adccntr=0;
                adcch[i].adcdata=0;
                adcch[i].stat=EXT_ADC_WAIT4ZERO;
                asm("ei");
                switch(i){
                    case 0:
                        tx0101.val.uac1=0;
                        break;
                    case 1:
                        tx0101.val.uac2=0;
                        break;
                    case 2:
                        tx0101.val.uac3=0;
                        break;
                    case 3:
                        tx0101.val.iac1=0;
                        break;
                    case 4:
                        tx0101.val.iac2=0;
                        break;
                    case 5:
                        tx0101.val.iac3=0;
                        break;
                    default:
                        break;
                }
            }
//if any channel is found with data available then data will be processed.
            else if(adcch[i].stat==EXT_ADC_DTAAVAIL){
                if(i<6){        //some AC reading, calculate value
                    calcval(i);
                    adcch[i].adccntr=0;
                    adcch[i].adcdata=0;
                    asm("di");
                    adcch[i].stat=EXT_ADC_WAIT4ZERO;
                    asm("ei");
                }
                else if(i==6){  //high voltage reading
#ifdef UONLY        
                    uhv=15000;                    
#else
                    if(adcch[6].adccntr==64){
                        uhv=(((adcch[6].adcdata>>6)*VREFH)>>10)*377/47; //this includes resistive divider 33kOhm versus 4K7. Unit is mV
                        adcch[6].adccntr=adcch[6].adcdata=0;
                        if((uhv<HVDCMIN)||(uhv>HVDCMAX)){ //turn off DC-DC converter and set status bit
                            LATCSET=0x2000;
                            SETBIT(STATWRDL,11);     //no way to reset this, other than power-cycle
                        }
                    }
                        asm("di");
                        adcch[6].stat=EXT_ADC_WAIT4ZERO;
                        asm("ei");
#endif
                }
                else if(i==7){
#ifdef  UONLY
                    tx0101.val.tcomp=80;
#else
                    if(adcch[7].adccntr==64){
                        rxclc=(uint16_t)(adcch[7].adcdata>>6);
                        tx0101.val.tcomp=gettemp(rxclc,tch_ehs);
                        if(tx0101.val.tcomp)
                            tx0101.val.tcomp+=36;       //for valid data add 36 to meet offset of -20. Lowest readable comp. temp. is 16 deg. C
                        adcch[7].adccntr=adcch[7].adcdata=0;
                    }
                        asm("di");
                        adcch[7].stat=EXT_ADC_WAIT4ZERO;
                        asm("ei");
                  
#endif
                }
            }
        }//for.. loop, checking all 8 channels
    }
}


//will calculate current or voltage, as per data in channel chan
void calcval(uint8_t chan){
    uint16_t *resptr;
    uint32_t quot;
    switch(chan){
        case 0://voltage U1
            resptr=&tx0101.val.uac1;
            break;
        case 1://voltage U2
            resptr=&tx0101.val.uac2;
            break;
        case 2://voltage U3
            resptr=&tx0101.val.uac3;
            break;
        case 3://current  I1
            resptr=&tx0101.val.iac1;
            break;
        case 4://current  I2
            resptr=&tx0101.val.iac2;
            break;
        case 5://current  I3
            resptr=&tx0101.val.iac3;
            break;
        default:
            break;       
    }
    if(adcch[chan].adcdata<100) //lower readings may be ignored
        *resptr=0;   
    else if(chan<3){ //voltage channels
        gotvoltage=true;
        if(adcch[chan].adccntr>96){//must read minimum 96 values (would be closed to 100 Hz!)
            quot=adcch[chan].adcdata/adcch[chan].adccntr;
            if(quot>20000) //this would correspond to approx. 350mV effective voltage on ADC, or 46V on gen. output 
                *resptr=((rootx(quot)*VREFH)>>10)*36456/45600;                             //multiply and shift calculates effective voltage in mV
            //Resistive divider is (180K+180K+4K7):4K7; the filter input on the VCS adds 3 further parallel paths with (complex) impedance of about 435kOhm each. This will cause the effective low side of the res. divider to appear as 4.5602kOhm at 60Hz
            else
                *resptr=0;
        }
    }
    else if (chan<6){//current channel
        if(adcch[chan].adccntr>48){
            quot=adcch[chan].adcdata/adcch[chan].adccntr;
            if(quot>0x7F)   //this would correspond to approx. 27mV on ADC input or about 1.2A current flow on regular CT board with 22Ohm resistor
                *resptr=((rootx(quot)*VREFH)>>10)*20/22;  //*fact/1000+ofs;    //multiply and shift calculates effective voltage in mV  
                                                                            //constant 20/22 covers sensing resistor (22Ohm) and ratio of CT                                                                //(2000) but divided by 100 to get result in units of 10mV
            else
                *resptr=0;
        }
    } 
}

//will return the square root of val using gauss algorithm. Will calculate to an error less than 1.55%. Input data needs to be >8 bit
uint32_t rootx(uint32_t val){
    uint16_t xroot,yroot,retval=0;
    uint32_t valx;
    valx=val;
    xroot=1;
    while(valx){    //find start value
        valx>>=2;
        if(valx)
            xroot<<=1;
    }
    while((!retval)&&(xroot>0)){
        yroot=val/xroot;
        valx=yroot*yroot;
        if(valx>val){
            if(((valx-val)<=(val>>6))||((yroot-xroot)<2))
                retval=yroot;
            else   
                xroot+=(yroot-xroot)>>1;
        }
        else{
            if(((val-valx)<=(val>>6))||((xroot-yroot)<2))
                retval=yroot;
            else
                xroot-=(xroot-yroot)>>1;
        }
    }
    return(retval);
}


//this routine will reset the entire AC sensing system and re-initialize SPI 4
void resetacchans(void){
    uint32_t i;
    SPI4CON=0;
    for(i=0;i!=8;i++){
        adcch[i].stat=EXT_ADC_WAIT4ZERO;
        adcch[i].adccntr=adcch[i].adcdata=0;
    }
    CLEARBIT(STATWRDL,14);
    isrcom.Bits.spi4fail=false;

       //SPI4, ADC access. Master mode, 32 bits
    SPI4CON=0;                  //will default to transmit interrupting when TSR is empty, 8 bit mode
    SPI4CONbits.MSTEN=true;    
    SPI4BRG=19;                 //select 1MHz communication speed
    SPI4CONbits.MODE32=true;
    SPI4CONbits.CKE=true;
    SPI4CONbits.ON=true;
    LATCCLR=0x4000;
    i=SPI4BUF;
#ifdef  UONLY
    adcind=0;
#else
    adcind=7;
#endif
    SPI4BUF=0x001F000;
    IFS1bits.SPI4EIF=IFS1bits.SPI4RXIF=false;
}

//this routine will also handle the radiator fan
void compcalc(bool init){
    static uint8_t compstat[4],fstat,nnoi,numax;        //11: off and ready, 01: on, 00: discharging. 
                                            //fan: 00 off, not required. 01: in low speed; 02: in high speed
                                            //bit 7 indicates that transition is pending
    static uint32_t tcomp[4],fantmr,uptmr;
    static uint8_t stat,prevcomp;                   //0 out of POR. 1: ready to boost. 2: in cut-off mode
    uint8_t i,txfan,compact;
    bool updout=false;
    if(init){   
        for(i=0;i!=4;i++)
            compstat[i]=0b11;
        LATECLR=0x01F8;         //all banks and fan off
        tx0101.val.compon=prevcomp=0;
        fstat=nnoi=numax=0;
    }
    else{
        //update status of discharging capacitors
        if((compstat[0]==0)&&((t_1ms-tcomp[0])>=tdisch))  //bank 0 compare against tdisch
            compstat[0]=0b11;
        
        if((compstat[1]==0)&&((t_1ms-tcomp[1])>=(2*tdisch)))  //bank 1 compare against 2x tdisch
            compstat[1]=0b11;
        
         if((compstat[2]==0)&&((t_1ms-tcomp[2])>=(4*tdisch)))  //bank 2 compare against 4x tdisch
            compstat[2]=0b11;
        
         if((compstat[3]==0)&&((t_1ms-tcomp[3])>=(4*tdisch)))  //bank 3 compare against 4x tdisch
            compstat[3]=0b11;       
        
        switch(stat){
            case 0:
                if(tx0101.val.uac1>tx0130.val.vcompen)
                    stat=1;
                LATECLR=0x01E0;
                tx0101.val.compon=prevcomp=0;
                for(i=0;i!=4;i++)
                    if(compstat[i]==1){
                        compstat[i]=0;
                        tcomp[i]=t_1ms;
                    }    
                break;
            case 1:
                if(tx0101.val.uac1>tx0130.val.vcompmax){    //overvoltage, turn off
                    if(numax>3){
                        stat=2;
                        numax=0;
                        LATECLR=0x01E0;
                        tx0101.val.compon=prevcomp=0;
                        for(i=0;i!=4;i++)
                        if(compstat[i]==1){
                            compstat[i]=0;
                            tcomp[i]=t_1ms;
                        }
                    }
                    else
                        numax++;
                }
                else if(tx0101.val.uac1<tx0130.val.vcompen){
                    stat=0;
                    numax=0;
                }
                else{
                    numax=0;
                    if((t_1ms-uptmr)>COMPCYC){
                        uptmr=t_1ms;
                        
                        //no compensation if current on leg 1 is less than 2x current per step or if pF is capacitive or above 0.97
                        if((tx0101.val.iac1<(istep<<1))||(tx0101.val.cosfi>97)){
                            if(nnoi>3){    
                                tx0101.val.compon=prevcomp=0;
                                updout=true;
                            }
                            else
                                nnoi++;
                        }
                        else if(istep>0){
                            nnoi=0;
                            compact=(tx0101.val.iac1*sinfi/500)/istep;
                            if(compact>30)
                                compact=30;                //this yields twice the number of steps. switch steps only 
                            if(prevcomp!=compact){          //if difference is 3 or more.
                                if(prevcomp>compact){
                                    if((prevcomp-compact)>2)
                                        updout=true;
                                }
                                else if((compact-prevcomp)>2)
                                    updout=true;
                                if(updout){
                                    prevcomp=compact;
                                    tx0101.val.compon=compact>>1;
                                }    
                            }
                        }
                    }
                }
                break;
                
            case 2:
                if(tx0101.val.uac1<tx0130.val.vcompreen)    //overvoltage, turn off
                    stat=1;
                break;
                
            default:
                stat=0;
                break;       
        }//switch statement.
        //set comp. status as calculated, if update is required
        if(updout){        //check for max. temperature. If warning is on, banks 4 will no longer be used
                            //if above shutdown, no compensation will take place at all
            if(TESTBIT(MSGWRDL,15))
                prevcomp=tx0101.val.compon=0;            //disable all compensation banks
            else if(TESTBIT(MSGWRDL,14)){
                CLEARBIT(tx0101.val.compon,3);  //disable bank 4
                prevcomp&=0x07;            //reflect same status in prevcomp
            }
            //first turn off all banks which are no longer required
            for(i=0;i!=4;i++){
                if(!TESTBIT(tx0101.val.compon,i)){
                    if(compstat[i]==0b01){
                        LATECLR=0x20<<i;
                        compstat[i]=0;
                        tcomp[i]=t_1ms;
                    }
                }
            }
            //now turn on required banks. If any bank is not available it then that status needs to be reflected in prevcomp!
            for(i=0;i!=4;i++){
                if(TESTBIT(tx0101.val.compon,i)){
                    if(compstat[i]==0b11){
                        LATESET=0x20<<i;
                        compstat[i]=0b01;
                    }
                    else if(compstat[i]==0)
                        CLEARBIT(tx0101.val.compon,i);      
                }
            }
            if(tx0101.val.compon!=(compact>>1)){   //could not use all banks
                prevcomp=tx0101.val.compon<<1;
                if(TESTBIT(compact,0))
                    SETBIT(prevcomp,0);
            }
        }
        //handle fan. First find out which temperature needs to be assigned
        if((TESTBIT(SENSCFGL,3))&&(tx0101.val.tcylhead>5))
            txfan=tx0101.val.tcylhead;
        else if((TESTBIT(SENSCFGL,1))&&(tx0101.val.tcoolout>5))
            txfan=tx0101.val.tcoolout;
        else if((TESTBIT(SENSCFGL,4))&&(tx0101.val.texhaust>5))
            txfan=tx0101.val.texhaust;
        else
            txfan=0xFF; //max speed is required, lost sensors
        switch(fstat){
            case 0:         //fan is not running
                if(txfan>tx0130.val.tfanlow){
                    fstat=3;            //transitioning status
                    fantmr=t_1ms;
                }
                break;
            case 1:         //fan is running in low speed
                if((txfan>tx0130.val.tfanhigh)||(txfan<tx0130.val.tfanlow)){//need to transfer either to higher speed or to stop
                    fstat=3;
                    fantmr=t_1ms;
                    PORTECLR=0x18;
                }
                break;
                
            case 2: //fan is running in high speed, check if that is OK
                if(txfan<tx0130.val.tfanhigh){
                    fstat=3;
                    fantmr=t_1ms;
                    PORTECLR=0x18;
                }
                break;
            case 3:
                if((t_1ms-fantmr)>FANDELAY){
                    if(txfan<=tx0130.val.tfanlow)
                        fstat=0;        //no fan required
                    else if((txfan>tx0130.val.tfanlow)&&(txfan<=tx0130.val.tfanhigh)){
                        PORTESET=0x08;
                        fstat=1;
                    }
                    else{
                        PORTESET=0x10;
                        fstat=2;
                    }
                }
                break;
            default:
                PORTECLR=0x18;
                fstat=0;
                break;   
        }   
    }
}

//will save an alarm log entry. will not check for alptr location, since it is assumed this was checked when powering up
void savalm(void){
    uint16_t savadr;
    if(ALPTR>0x14F)
        ALPTR=0;
    savadr=0x0A00+0x80*(ALPTR&0xFF);
    if(savbuf(savadr)){
        ALPTR+=1;
        if((ALPTR&0xFF)>79)
            ALPTR=0x0100;       //re-start from 0, set bit 8 to indicate an over-run
        writeeeprom(0x01AC,&ALPTR,1);
    }
}

//will save a history event
void savhist(void){
    uint16_t savadr;
    if(EVPTR>0x014F)
        EVPTR=0;
    savadr=0x3200+0x80*(EVPTR&0xFF);
    if(savbuf(savadr)){
        EVPTR+=1;
        if((EVPTR&0xFF)>79)
            EVPTR=0x0100;
        writeeeprom(0x01AE,&EVPTR,1);
    }
}

//will create the buffer to be saved from RegFrame. first 2 words are calendar date. Returns true if successull
bool savbuf(uint16_t adr){
    uint16_t sb[2],bf[35];
    uint8_t i,cntx=0;
    bool passed=false,retval=false;
    
    if(calok){
        sb[0]=caldate;
        sb[1]=caldate>>16;
    }
    else
        sb[0]=sb[1]=0xFFFF;     //write 0xFFFF if calendar date is unknown
    
    while(!passed && (cntx<3)){
        writeeeprom(adr,sb,2);
        cntx++;
        readeeprom(adr,bf,2);
        passed=(sb[0]==bf[0]) && (sb[1]==bf[1]);
    }
    
    if(passed && (cntx<3)){
        adr+=4;
        passed=false;
        cntx=0;
        tx0101.val.statvar=gencontrstate|(stfeedb.bts[0]<<8);
        while(!passed && (cntx<3)){
            writeeeprom(adr,tx0101.trans,35);
            cntx++;
            readeeprom(adr,bf,35);
            passed=true;
            for(i=0;i!=35;i++)
                if(tx0101.trans[i]!=bf[i])
                    passed=false;
        }
        if(passed && (cntx<3)){
            adr+=70;
            passed=false;
            cntx=0;
            while(!passed && (cntx<3)){
                writeeeprom(adr,tx013A.trans,6);
                cntx++;
                readeeprom(adr,bf,6);
                passed=true;
                for(i=0;i!=6;i++)
                    if(tx013A.trans[i]!=bf[i])
                        passed=false;
            }
            if(passed && (cntx<3))
                retval=true;
        }  
    }
    return(retval);
}


//used for test mode; will cycle through all digital outputs
void alltest(void){
    static uint32_t tmx,tmact;
    static uint8_t cyc;
    static bool up;
    if((!TESTBIT(STATWRDL,2))||(!TESTBIT(STATWRDL,3))){    //will not test actuator if it is not OK
        if(TESTBIT(C_ALM,13)||TESTBIT(C_ALM,15)||TESTBIT(STATWRDL,1) || linact)
            LATASET=0x0040;
        else{
            OC5CONbits.ON=true;         //need to keep motor on
            LATACLR=0x0040;
            if((t_1ms-tmact)>10){
                tmact=t_1ms;
                up=testservo(up);
            }
                
        }
        
        if((t_1ms-tmx)>1000){
            tmx=t_1ms;
            if(linact)
                up=testlinact(up);
            cyc++;
            if(cyc>13)
                cyc=0;
            if(TESTBIT(cyc,0))
                ledstat=BLKSYNC;
            else
                ledstat=BLKALT;

            LATACLR=0x3F;
            LATECLR=0x01FE;
            switch(cyc){
                case 0:
                    if(!linact)
                        LATDSET=0x02;   //stop solenoid
                    break;
                case 1:
                    if(!linact)
                        LATDCLR=0x02;   //stop solenoid
                    LATASET=0x02;   //fuel pump
                    break;
                case 2:
                    LATASET=0x04;   //preheat
                    break;
                case 3:
                    LATASET=0x08;   //cranking
                    break;
                case 4:
                    LATASET=0x10;   //flame start
                    break;
                case 5:
                    LATASET=0x0020; //GND breaker
                    break;
                case 6:    
                    LATESET=2;      //Relay  "Running"
                    break;
                case 7:             //Relay "Fault"
                    LATESET=4;
                    break;
                case 8:
                    LATESET=8;      //fan low speed    
                    break;
                case 9:
                    LATESET=0x10;   //fan high speed
                    break;
                case 10:
                    LATESET=0x20;   //comp. step 1
                    break;
                case 11:
                    LATESET=0x40;   //comp. step 2
                    break;
                case 12:
                    LATESET=0x80;   //comp. step 3
                    break;
                case 13:
                    LATESET=0x0100; //comp. step 4
                    break;
                default:
                    cyc=0;
                    break;
            }
        }
//check for battle short
        if(PORTEbits.RE9 || pan_bs)
            SETBIT(tx0101.val.mwordlow,10);
        else 
            CLEARBIT(tx0101.val.mwordlow,10);
//check for oil pressure switch
        if(PORTGbits.RG0)
            SETBIT(tx0101.val.stwordlow,4);
        else
            CLEARBIT(tx0101.val.stwordlow,4);
//check for air intake
        if(PORTAbits.RA7)
            SETBIT(tx0101.val.mwordlow,3);
        else 
            CLEARBIT(tx0101.val.mwordlow,3);
//check for radiator fan monitor
        if(PORTAbits.RA9)
            SETBIT(tx0101.val.stwordlow,5);
        else
            CLEARBIT(tx0101.val.stwordlow,5);
//evaluate remote power up
        if(PORTDbits.RD5 || pan_estop)
            SETBIT(STATWRDH,4);
        else
            CLEARBIT(STATWRDH,4);
//evaluate remote start switch
        if(PORTDbits.RD6)
            SETBIT(tx0101.val.mwordlow,5);
        else 
            CLEARBIT(tx0101.val.mwordlow,5);
    }
}

//this function will perform PID control either on speed or on voltage. Will be speed if isspeed!=0, otherwise voltage
//constants are allways assumed to be in %, so for a prop. constant of 2.1 the user enters 210.
//if newpar is set then accumulatet error and past error will be cleared.
void pidcontrol(uint16_t targval,bool isspeed,bool newpar){
    static uint32_t trg;
    static int16_t perr; //past error for differential 
    static bool lastspeed;                          //true if last regulation was speed as well
    uint16_t act,cp,cd;
    int32_t xsum,yout;
    int16_t err,margval;
    bool *in_ok;
    if(isspeed)
        in_ok=&gotspeed;
    else
        in_ok=&gotvoltage;
    
    if(((t_1ms-trg)>tx0130.val.treg) && *in_ok){
        trg=t_1ms;
        *in_ok=false;
        if(newpar || (lastspeed!=isspeed)){
            lastspeed=isspeed;
            perr=0;
        }
        if(isspeed){
            lastspeed=true;
            cp=tx0130.val.kp_speed;
            cd=tx0130.val.kd_speed;
            act=pwmspeed;
            margval=1;
        }
        else{
            lastspeed=false;
            cp=tx0130.val.kp;
            cd=tx0130.val.kd;
            if(TESTBIT(tx0130.val.genconfig_l,0)){//3~ generator
                act=(tx0101.val.uac1+tx0101.val.uac2+tx0101.val.uac3)/3;
            }
            else
                act=(tx0101.val.uac1+tx0101.val.uac2)>>1;
            margval=2;
        }
        if(bldc)
            err=act-targval;
        else
            err=targval-act;
        perr=err;          //save error for next loop
        if(linact){ //linear actuator
            xsum=((err*cp)/10)+((err-perr)*cd)/100;
             //xsum is not expected to be greater than 2bytes, positive or negative. If it is, actuator will go into saturation
            if(xsum>65535)
                OC2RS=PR2;
            else if(xsum<-65535)
                OC2RS=0;
            else{   //will scale total deviation in a way that would make give a full swing with 2 cycles
                    //meaning max. value below saturation will correspond PR2/2
                yout=xsum*PR2;
                yout>>=16;
                if((OC2RS+yout)<0)
                    OC2RS=0;
                else if((OC2RS+yout)>PR2)
                    OC2RS=PR2;
                else
                    OC2RS=OC2RS+yout;    
                if(OC2RS<tx0130.val.mindc_oc2)
                    OC2RS=tx0130.val.mindc_oc2;
            }
        }
        else{
            if((err>5)||(err<-5)){//speed /voltage needs to be off by more than 5rpm/0.5V
                xsum=(err*cp)/100+((err-perr)*cd)/100;
    //xsum is not expected to be greater than 2bytes, positive or negative. If it is, it will be limited to this maximum
                if(xsum>65535)
                    xsum=65535;
                else if(xsum<-65535)
                    xsum=-65535;
    //scale back onto total range of actuator which is actmax-actmin.
                yout=(abs(actmaxspeed-actminspeed)*xsum)>>16;
                if(yout==0){
                    if(xsum>0)
                        yout=-1;
                    else
                        yout=1;
                }
                yout=yout+OC5RS;
                if(bldc){
                    if(yout<(actmaxspeed+4*STEPMOVE))
                        yout=actmaxspeed+4*STEPMOVE;
                    else if(yout>(actminspeed-4*STEPMOVE))
                        yout=actminspeed-4*STEPMOVE; 
                }
                else{
                    if(yout>(actmaxspeed-4*STEPMOVE))
                        yout=actmaxspeed-4*STEPMOVE;
                    else if(yout<(actminspeed-4*STEPMOVE))
                        yout=actminspeed-4*STEPMOVE;
                }
                OC5RS=yout;
            }
        }
    }
}


//this function will perform engine preheat. It will return true when done.
bool preheat(bool init){
    static uint32_t tmrx,glowtime;
    static uint8_t stat;
    static bool flams;
    bool retval=false;
    uint8_t tguide;
    if(init){
        stat=0;
        flams=false;
        LATACLR=0x34;//clear all outputs, if already in process
    }
    else switch(stat){
        case 0://calculate preheating time
            if(tx0101.val.tvcs<=COLDVCS){//will engage flame start, 
                stat=1;     //
                glowtime=20000;
                tmrx=t_1ms;
                flams=true;
                LATASET=0x20;
            }
            else{
                flams=false;
                if(tx0101.val.tcylhead)     //faulty readings will be zero!
                    tguide=tx0101.val.tcylhead;
                else if(tx0101.val.tcoolout)
                    tguide=tx0101.val.tcoolout;
                else if(tx0101.val.tcoolin)
                    tguide=tx0101.val.tcoolin;
                else{
                    tguide=tx0101.val.tvcs;
                    if(tguide>55)
                        tguide-=30;
                    else
                        tguide=24;
                }
                if(tguide<24)
                    glowtime=10000;
                else if(tguide<60)
                    glowtime=(60-tguide)*7000/35+2000;		//decrease 7 seconds over 35 degree plus constant 2 seconds
                else if(tguide<70)
                    glowtime=2000;
                else//no preheating required
                    glowtime=0;
                if(glowtime){
                    stat=1;
                    tmrx=t_1ms;
                    LATASET=0x20;
                }
                else{
                    flams=false;
                    retval=true;
                }
            }
            break;
        case 1://this is the ground breaker closing cycle only!
            LATASET=0x0002; //Fuel pump
            if((t_1ms-tmrx)>70){
                if(glowtime){
                    tmrx=t_1ms;
                    LATASET=0x04;   //glow plugs 
                    if(flams)
                        LATASET=0x10;//flame starter
                    else
                        LATACLR=0x10;
                    stat=2;
                }
                else{
                    retval=true;
                    flams=false;
                    stat=0;
                }
            }
            break;
        case 2:
            if((t_1ms-tmrx)>=glowtime){
                LATACLR=0x14;
                stat=0;
                flams=false;
                retval=true;
            }
            break;
        default:
            LATACLR=0x35;
            flams=false;
            stat=0;
            break;
    }
    return(retval);
}  

bool engstart (bool init){
    static uint8_t stat,lowpwrcntr;
    static uint32_t tmrx,crktime,rntm;
    static bool advfuel;
    bool retval=false;
    if(init){
        stat=0;
        preheat(true);
        compcalc(true);
        LATACLR=0x3A;
        lowpwrcntr=0;
        advfuel=false;
    }
    else switch(stat){
        case 0://adjust actuator, energize gnd breaker and fuel pump and stop sol. if energized to run
            OC5CONbits.ON=1;
            LATASET=0x02;
            if(!TESTBIT(GENCFGL,5))
                LATASET=1;
            LATACLR=0x40;
            if(linact)
                OC2RS=PR2*(uint32_t)tx0130.val.actcrank/100;
            else if(bldc)
                OC5RS=actminspeed-(uint32_t)(actminspeed-actmaxspeed)*(tx0130.val.actcrank)/100;
            else
               OC5RS=actminspeed+(uint32_t)(actmaxspeed-actminspeed)*(tx0130.val.actcrank)/100;
            tx0101.val.runlast=tx0101.val.enlast=0;
            tmrx=t_1ms;
            stat=1;
            break;
        case 1:     //start cranking process
            if((t_1ms-tmrx)>200){//time for gnd breaker to close and actuator to move to center position
                LATASET=8;
                stat=2;
                tmrx=t_1ms;
                advfuel=false;
                crktime=t_1ms;
                tx013A.v32.starts++;
                writeeeprom(0x01A8,&tx013A.val.startcntl,2);
            }
            break;
        case 2://crank without advancing fuel rack. Will do this for max. 2 seconds, then advance 1 increment per 10ms
//            if(tx0101.val.engspeed>MINSPEED){ //engine started
            if(pwmspeed>MINSPEED){ //engine started
                retval=false;
                stat=3;
                rntm=t_1ms;
                advfuel=false;
            }
            else if((tx0101.val.ubatt<(tx0130.val.udcloww>>1))&&(lowpwrcntr<10)){
                lowpwrcntr++;
                inlowpower(false);
            }
            if(advfuel){
                if((t_1ms-tmrx)>50){
                    tmrx=t_1ms;
                }
                if(((tx0101.val.tvcs<COLDVCS)&&((t_1ms-crktime)>=(tx0130.val.tcranklow*1000)))||
                        ((tx0101.val.tvcs>=COLDVCS)&&((t_1ms-crktime)>=(tx0130.val.tcranknom*1000)))){
                    LATACLR=0x3F;
                    LATASET=0x40;
                    OC5CONbits.ON=0;
                    stat=0;
                    lowpwrcntr=0;
                    advfuel=false;
                    retval=true;
                    SETBIT(STATWRDL,7);      //start attempt failed
                }
            }
            else if((t_1ms-tmrx)>2000){
                tmrx=t_1ms;
                advfuel=true;
            }     
            break;
        case 3://engine is running, keep cranking for another MINCRNK ms.
//            if(tx0101.val.engspeed<MINSPEED)
            if(tx0101.val.engspeed<MINSPEED)
                stat=2;     //return to previous status, keep cranking 
            else if((t_1ms-rntm)>MINCRNK){
                retval=true;
                lowpwrcntr=stat=0;
                LATACLR=28;
            }
            break;
        default:
            break;
    }
    return(retval);
}


//this function will check generator status. It will return 1 if the generator may be started / continue to run
//It will check all status bits, but will not perform actual limit comparing. This is done by 
//functions slowreadcheck, actcheck, accheck...
//Battle short will cancel out:
//all temperature alarms, including missing exhaust elbow sensor
//oil pressure alarms (switch and sensor)), included not connected / shorted alarm
//All AC voltage and power end engine speed alarms
//DC voltage and all actuator alarms
//Fault on SPI or memory bus, DC/DC converter fault
//It will not cancel out demand mismatch, actuator not calibrated, vcs not programmed or calibrated, start attempt failed, cannot stop, 
//max No. of unexp. start/stops, unexpected stop, and HW problems with VCS (SPI buses and DC/DC))
bool checksys(void){
    bool retval=true;  
    if(PORTEbits.RE9 || pan_bs)
        SETBIT(tx0101.val.mwordlow,10);
    else
        CLEARBIT(tx0101.val.mwordlow,10);
    if(TESTBIT(MSGWRDL,10)){    //check for battle short
        if(((STATWRDL&0x7D8F)>0)||((STATWRDH&0x18))){  //these bits cannot be surpressed by battle short
            LATECLR=0x04;
            retval=false;
        }
        else{
            retval=true;    //any other alarm will be surpressed anyway
            if((C_ALM>0)||(STATWRDL>0)||(STATWRDH>0)){
                ledstat=RDGNBLK;
                LATECLR=0x04;
            }
            else{
                LATESET=0x04;
                ledstat=GNBLK;
            }
        }
    }
    else{
        if((C_ALM>0)||(STATWRDL>0)||STATWRDH>0){
            retval=false;
            LATECLR=0x0004;
            ledstat=RDBLK;
        }
        else{
            retval=true;
            LATESET=0x0004;
            ledstat=GNBLK;
        }
    }
    return(retval);
}

//will check whether someone asked to stop the generator. Will set demand mismatch if applicable. Will return true if stop is requested. 
//WIll be called only if gen, is in some running state. Will make gen. start to be "Remote" even if running, if at a later time remote start gets engaged
static bool checkstoprq(void){
    bool retval=false;
    if(stfeedb.fields.newin && !REQSTAT){
        stfeedb.fields.newin=false;
        retval=true;
        if(TESTBIT(MSGWRDH,8)){
            SETBIT(STATWRDL,8);    //this is a demand mismatch
            CLEARBIT(MSGWRDH,8);
        }
    }
    else
        retval=checkremof();
    return(retval);
}

void procu17(void){
    static uint16_t rxvcs,rxengoil;
    static uint8_t vcscntr,oilcntr;
    static bool isint,inof;
    static uint32_t tmrx;
    uint16_t adres;
    if(SPI2CONbits.ON){
        if(!SPI2STATbits.SPIBUSY){
            if(!inof){
                inof=true;
                tmrx=t_1ms;
            }
            else if(t_1ms-tmrx>1){
                LATGSET=0x0200; //pull CS high, 2ms after transition
                adres=SPI2BUF&0x03FF;
                if(isint){
                    rxvcs+=adres;
                    if(++vcscntr==64){
                        rxvcs>>=6;
                        tx0101.val.tvcs=gettemp(rxvcs,tch_i);
                        rxvcs=vcscntr=0;
                    }
                    isint=false;
                    LATGCLR=0x0200;
                    inof=false;
                    SPI2BUF=0x7800;     //read oil temperature  
                }
                else{
                    rxengoil+=adres;
                    if(++oilcntr==64){
                        rxengoil>>=6;
                        tx0101.val.tengoil=gettemp(rxengoil,tch_o);
                        rxengoil=oilcntr=0;
                    }
                    isint=true;
                    LATGCLR=0x0200;
                    inof=false;
                    SPI2BUF=0x6800;
                }    
            }     
        }
    }
    else{
        SPI2CON=0;              //SPI2, 2 ADC channels, oil temp. and VCS temp. 
        SPI2CONbits.MSTEN=true;    //Master mode
        SPI2CONbits.MODE16=1;   //run in 16 bit mode, one transaction per reading
        SPI2BRG=199;            //run with 100kHz
        SPI2CONbits.CKE=true;
        SPI2CONbits.ON=true;
        LATGCLR=0x0200;
        isint=true;
        inof=false;
        SPI2BUF=0x6800; //tx bit for VCS temperature
    }
}


//this function will try to get transfers 0x013A,0x013B and 0x013C from panels. will try max. every 250ms and up to panel address of max. 0x27
//or until it got the data it is waiting for. Will keep cycling through panels. 
void ask4transfer(uint16_t trx){
    static uint32_t tmrx;
    static uint8_t frompan;
    uint8_t buf[3];
    buf[1]=trx;
    buf[2]=trx>>8;;
    if((t_1ms-tmrx)>250){
        if((frompan<0x20)||(frompan>0x27))
            frompan=0x20;
        buf[0]=frompan;
        if(txtransfer(1,buf)){
            tmrx=t_1ms;
            frompan++;
            lasttrans=0;
        }
    }
}

//will check for power down request. Will set bit pwrdwn in variable stfeedb.fields for power down
void chkpwrdwn(void){
    static uint32_t tmrof;
    static bool isof;
    uint16_t incid;
    if(nocan>7){ //no communication on CAN bus for >7second
        if(!(PORTDbits.RD6||PORTDbits.RD7)){ //none of the static inputs are active, USB port is unplugged
            if(!isof){
                isof=true;
                tmrof=t_1ms;
            }
            else if((t_1ms-tmrof)>25){  //allow contact bouncing of 25ms
                stfeedb.fields.pwrdwn=true;
                if(tx0101.val.engspeed>MINSPEED){
                    SETBIT(tx0101.val.stwordhigh,3);
                    savalm();
                }
            }      
        }
        else
            isof=false;
    }
}

//this routine will be called from inside instop status. It is just used to turn off all digital outputs (including actuator supply)
//and clear the "did not build up voltage" bit
void alloff(void){
    CLEARBIT(MSGWRDH,0); 
    LATASET=0x0040;
    LATACLR=0x3F;
#ifndef TESTLINMOT
    OC2RS=0;
#endif    
    LATECLR=0x1A;       //will not clear alarm status output since this is unknown here
    chan2zero(true,0);
}

//this function will overwrite data that the VCS would retrieve from panels. 
//operating time, start counter and energy produced will be set to 0
//Service warning re-trigger will be calculated as if that service message was just resetted
void stdefaultdata(void){
    uint16_t bf[4];
    uint8_t i;
    for(i=0;i!=4;i++)
        bf[i]=0;
    writeeeprom(0x0180,bf,4); //clear operating time and start counter
    writeeeprom(0x01D4,bf,2);//clear energy counter
    for(i=0;i!=32;i++)
        if(TESTBIT(valsvn,i))
            ressrvnot(i);
    CLEARBIT(MSGWRDH,5);
}

void chan2zero(bool init,uint8_t chan){
    static uint8_t tmouts[6];
    uint8_t i;
    if(init)
        for(i=0;i!=6;i++)
            tmouts[i]=0;
    else{
        tmouts[chan]++;
        for(i=0;i!=6;i++){
            if(tmouts[i]>5){
                tmouts[i]=0;
                switch(i){
                    case 0:
                        tx0101.val.uac1=0;
                        break;
                    case 1:
                        tx0101.val.uac2=0;
                        break;
                    case 2:
                        tx0101.val.uac3=0;
                        break;
                    case 3:
                        tx0101.val.iac1=0;
                        break;
                    case 4:
                        tx0101.val.iac2=0;
                        break;
                    case 5:
                        tx0101.val.iac3=0;
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

//will check if remote stop (static)  was issued. will return true if so. 
bool checkremof(void){
    static bool lastof;
    static uint32_t tmrx;
    bool retval=false;
    if(TESTBIT(tx0101.val.mwordhigh,8)){    //remote start is not set
        if(!PORTDbits.RD6){
            if(lastof){
                if((t_1ms-tmrx)>100)       //debounce for 100ms
                    retval=true;
                    
            }
            else{
                lastof=true;
                tmrx=t_1ms;
            }
        }
        else{
            lastof=false;
            CLEARBIT(tx0101.val.stwordlow,8);
        }
    }
    else
        lastof=false;
    return(retval);
}

//this function will either allow the actuator to travel back (total time 2sec) or, if the stop sol. was energized to stop, it will do the entire stop cycle
bool stopcycle(uint32_t tmrx){
    static uint8_t stat;
    static uint32_t tx;
    bool retval=false;
    if(TESTBIT(tx0130.val.genconfig_l,5))
        switch(stat){
            case 0:
                if((t_1ms-tmrx)>100){   //this is the wait time to allow the ground breaker to pull in
                    tx=t_1ms;
                    LATDSET=0x0002;
                    stat=1;
                }
                break;
                
            case 1: //wait min 2 sec and then check for eng. speed<MINSPEED
                if((t_1ms-tx)>2000){
                    if(tx0101.val.engspeed<MINSPEED){
                        retval=true;
                        stat=0;
                        LATDCLR=0x0002;
                        LATACLR=0x0020;
                    }
                }
                break;
                
        
        }
    else if((t_1ms-tmrx)>2000)
        retval=true;
    if(retval)
        LATASET=0x0040; //turn off actuator supply.
    return(retval);
}

//will calibrate actuator. if init==true status variable will be initialized to 0
//will also check the reverse polarity MOSFET for correct operation
bool actcalib(bool init){
    static uint8_t calstate,golow,imaxcntr;
    static uint32_t tmx,traveltm;
    bool retval=false;     
    if((init)||(calstate>3)){
        calstate=0;
        golow=true;
        LATASET=0x0040;     //turn actuator off
        OC5CONbits.ON=0;
        imaxcntr=0;
    }
    else switch(calstate){
        case 0: //initialize calibration process
            LATACLR=0x0040;     //turn on 5V supply
            OC5CONbits.ON=1;
            OC5RS=3750;     //center position;
            tmx=t_1ms;
            calstate=1;
            break;

        case 1: //actuator traveling to center position, allow 1 s to stabilize
            if((t_1ms-tmx)>1000){
                traveltm=tmx=t_1ms;
                imaxcntr=0;
                calstate=2;
            }
    //actuator is not supposed to hit the limits while traveling to the middle. If it does, it is misaligned.           
    //also, the actuator voltage needs to be within shutdown limits           
            if(TESTBIT(tx0101.val.codeAlm,13))//bits 12, 13 and 15 need to be clear
                retval=actfailoff(&calstate);                   //will stop calibration process
            break;

            case 2: //actuator traveling towards limits with one decrement per 4ms
                if(((t_1ms-tmx)>4)&&(isrcom.Bits.actidone)){
                    isrcom.Bits.actidone=false;
                    tmx=t_1ms;
                    if(tx0101.val.acti<ACTIMAXW){
                        if(golow){
                            if(bldc){
                                if(OC5RS<=(limactlowspeed-STEPMOVE))
                                    OC5RS+=STEPMOVE;
                            }
                            else if(OC5RS>=(limactlowspeed+STEPMOVE))
                                OC5RS-=STEPMOVE;  
                        }
                        else{
                            if(bldc){
                                if(OC5RS>=(limacthighspeed+STEPMOVE))
                                    OC5RS-=STEPMOVE;
                            }
                            else if(OC5RS<=(limacthighspeed-STEPMOVE))
                                OC5RS+=STEPMOVE;
                        }
                        imaxcntr=0;
                    }
                    else
                        imaxcntr++; 
                }
                if(TESTBIT(tx0101.val.codeAlm,13) || ((t_1ms-traveltm)>15000))//again test for bits 12, 13 and 15 in codeAlm, but also for time-out
                    retval=actfailoff(&calstate);
                else if(bldc && ((golow && (OC5RS>=STEPVALMIN_BLDC)) || (!golow && (OC5RS<=STEPVALMAX_BLDC)))){       //if the actuator goes to the theoretical limits then the BLDC does not draw excessive current
                    if(golow){
                        actminspeed=OC5RS-ACTDELTA;
                        calstate=1;
                        golow=false;
                        tmx=t_1ms;
                        OC5RS=3750;
                    }
                    else{
                        actmaxspeed=OC5RS+ACTDELTA;
                        calstate=3;
                    }
                }
                else if(imaxcntr>20){//got the limit, either because time-out (9sec) or hitting current limit 20x in a row
                    if(golow){
                        actminspeed=OC5RS+ACTDELTA;  
                        golow=false;
                        calstate=1;
                        tmx=t_1ms;
                        OC5RS=3750;
                    }
                    else{
                        actmaxspeed=OC5RS-ACTDELTA;
                        calstate=3;
                    }
                }
                break;    
            case 3:     //calibration is over. CHeck results
                if(bldc){
                    if((actmaxspeed>=(limacthighspeed+ACTDELTA))&&(actminspeed<=(limactlowspeed-ACTDELTA)) &&
                        (actmaxspeed<actminspeed)&&((actminspeed-actmaxspeed)>=MINSTEPRANGE)){
                        retval=true;    //calibration is ready  
                        CLEARBIT(tx0101.val.mwordhigh,6);
                    }
                }
                else if((actmaxspeed<=(limacthighspeed-ACTDELTA))&&(actminspeed>=(limactlowspeed+ACTDELTA)) &&
                        (actminspeed<actmaxspeed)&&((actmaxspeed-actminspeed)>=MINSTEPRANGE)){
                    retval=true;    //calibration is ready  
                    CLEARBIT(tx0101.val.mwordhigh,6);
                }
                golow=true;
                calstate=0;
                LATASET=0x0040;
                OC5CONbits.ON=false;
                break;

            default:        //this case just for error handling. Should never execute
                golow=true;
                calstate=0;
                break;
        }
    return(retval);
}//actcalib


//this function just to turn off the actuator and report failed calibration. Needs to return true
bool actfailoff(uint8_t *calst){
    LATASET=0x0040;                                 //Turn off the actuator
    OC5CONbits.ON=false;
    SETBIT(tx0101.val.mwordhigh,6);                 //and mark actuator as not calibrated
    *calst=0;           
    return(true);
}

bool testlinact(bool up){
    if(up){
        if(OC2RS<7900)
                OC2RS+=100;
            else
                up=false;
        }
    else{
        if(OC2RS>100)
            OC2RS-=100;
        else
            up=true;           
    }
    return(up);
}


bool testservo(bool up){
    
    if(!PORTAbits.RA6){
        if(up){
            if(bldc){
                if(OC5RS>=(actmaxspeed+STEPMOVE))
                    OC5RS-=STEPMOVE;
                else
                    up=false;
            }
            else{
                if(OC5RS<=(actmaxspeed-STEPMOVE))
                    OC5RS+=STEPMOVE;
                else
                    up=false;
                }
        }
        else{
            if(bldc){
                if(OC5RS<=(actminspeed-STEPMOVE))
                    OC5RS+=STEPMOVE;
                else
                    up=true;
            }
            else{
                if(OC5RS>=(actminspeed+STEPMOVE))
                    OC5RS-=STEPMOVE;
                else
                    up=true;
            }
        }
        return(up);
    }
    else return(false);
}


//will calculate tank level. Will also set tank alarm level
//allow a hysteresis of 10% on scale ends, prior to indicating fault
void calctank(void){
    static uint32_t tklim;
    static bool tkal;
    CLEARBIT(tx0101.val.mwordhigh,2);
    if(tx0130.val.rtkempty==tx0130.val.rtkfull)
        tx0101.val.tank=255;       
    else if(tx0130.val.rtkfull<tx0130.val.rtkempty){        //res. at full tank is lowest
        if(tx0101.val.tankres<=tx0130.val.rtkfull)
            tx0101.val.tank=100;       //full tank
        else if(tx0101.val.tankres>=tx0130.val.rtkempty)
            tx0101.val.tank=0;
        else
            tx0101.val.tank=(uint32_t)(100)*(tx0130.val.rtkempty-tx0101.val.tankres)/(tx0130.val.rtkempty-tx0130.val.rtkfull);
    }
    else{   //res at empty tank is lowest
        if(tx0101.val.tankres<=tx0130.val.rtkempty)
            tx0101.val.tank=0;
        else if(tx0101.val.tankres>=tx0130.val.rtkfull)
            tx0101.val.tank=100;
        else
            tx0101.val.tank=(uint32_t)(100)*(tx0101.val.tankres-tx0130.val.rtkempty)/(tx0130.val.rtkfull-tx0130.val.rtkempty);
    }
    if(tx0101.val.tank<tx0130.val.tankw){
        if(tkal){
            if((t_1ms-tklim)>(tx0130.val.d_tankw*1000))
                SETBIT(tx0101.val.mwordlow,4);
        }
        else{
            tkal=true;
            tklim=t_1ms;
        }
    }
    else{
        CLEARBIT(tx0101.val.mwordlow,4);;
        tkal=false;
    }
}