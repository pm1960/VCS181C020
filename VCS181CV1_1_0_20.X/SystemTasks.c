#include "SystemTasks.h"

static uint16_t wleakin;

static void checkextio(bool init);
static bool switchon(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon);
static bool switchoff(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon);
static void readintad(void);

void SysTasks(void){
    static uint32_t tmrx;
    switch(sysstat){
        case SYS_INIT:
            rq0100.status=true;       
            tmrx=t_1ms;
            sysstat=SYS_CONNECT;
            checkextio(true);
            readintad();
//            initcomp();
//            compcalc(true);
//            checkextad_ac(true);
            break;
    }
}








void checkextio(bool init){
#define FANSW       0
#define AIRINSW     1
#define LOCALBS     2   //battle short local input
#define OILPRSW     3
#define FANOFF      0
#define FANLOW      1
#define FANTRANS    2
#define FANHIGH     3
    
    static uint8_t digstats;  //bits set in this variable will indicate an active input
    static uint8_t fanstat;     
    static uint32_t tmrins[5];  //tmrins[4] contrls pause when fan is transitioning 
    int16_t fantemp;
    
    if(init)
        digstats=0xF0;      //inidicates that all signals are off
    
    else{
        if(TESTBIT(*genconfh,5)){
            if(!TESTBIT(*stwordhigh,11)){    //switch is inactive
                if(switchon(&tmrins[FANSW],&digstats,FANSW,!PORTAbits.RA7))
                    SETBIT(*stwordhigh,11);
            }
            else if(switchoff(&tmrins[FANSW],&digstats,FANSW,PORTAbits.RA7))
                CLEARBIT(*stwordhigh,11);
        }
        else 
            CLEARBIT(*stwordhigh,11);

        if(TESTBIT(*genconfh,2)){
            if(!TESTBIT(*miscwordlow,9)){
                if(switchon(&tmrins[AIRINSW],&digstats,AIRINSW,!PORTAbits.RA9))
                    SETBIT(*miscwordlow,9);
            }
            else if(switchoff(&tmrins[AIRINSW],&digstats,AIRINSW,PORTAbits.RA9))
                CLEARBIT(*miscwordlow,9);
            }
        else
            CLEARBIT(*miscwordlow,9);

        if(!stat_bits.battles){
            if(switchon(&tmrins[LOCALBS],&digstats,LOCALBS,PORTEbits.RE9))      //battle short triggers on closing contact
                stat_bits.battles=true;
        }
        else if(switchoff(&tmrins[LOCALBS],&digstats,LOCALBS,!PORTEbits.RE9))      //battle short triggers on closing contact
            stat_bits.battles=false;

        if(gcontstat>GENCONTR_STATE_CRANKING){                                      //will not trigger oil pressure switch if generator is not running
            if(!TESTBIT(*stwordhigh,9)){
                if(switchon(&tmrins[OILPRSW],&digstats,OILPRSW,!PORTGbits.RG0))
                    SETBIT(*stwordhigh,9);
            }
            else if(switchoff(&tmrins[OILPRSW],&digstats,OILPRSW,PORTGbits.RG0))
                CLEARBIT(*stwordhigh,9);
        }
        
        if(TESTBIT(*genconfh,3)){    //Fan breaker is monitored on analog input, will compare to fix thresholds 
            if(wleakin>((VREF-(VREF>>2))))
                SETBIT(*stwordhigh,10);
            else if(wleakin<(VREF>>2))
                CLEARBIT(*stwordhigh,10);
        }
        else
            CLEARBIT(*stwordhigh,10);
    
        if(gcontstat>GENCONTR_STATE_CRANKING){
            LATESET=1;
            if(!TESTBIT(*mwordhigh,4) && TESTBIT(*sensconfl,2))
                fantemp=*tcylh;
            else if(!TESTBIT(*mwordlow,14)&& TESTBIT(*sensconfl,0))
                fantemp=*tcoolin;
            else
                fantemp=255;
            switch(fanstat){
                case FANOFF:
                    if(fantemp>*fan_low){
                        tmrins[4]=t_1ms;
                        fanstat=FANTRANS;
                        LATECLR=0x00024;
                    }
                    break;
                    
                case FANTRANS:
                    if(fantemp<*fan_low){
                        fanstat=FANOFF;
                        tmrins[4]=t_1ms;
                    }
                    else if((t_1ms-tmrins[4])>FANDELAY){
                        if(fantemp<*fan_high){
                            LATESET=0x0008;
                            fanstat=FANLOW;
                        }
                        else{
                            LATESET=0x0010;
                            LATESET=0x0010;
                            fanstat=FANHIGH;
                        }
                    }
                    break;
                    
                case FANLOW:
                    if(fantemp<(*fan_low-FANHYST)){
                        LATECLR=024;
                        fanstat=FANOFF;
                    }
                    else if(fantemp>*fan_high){
                        LATECLR=024;
                        tmrins[4]=t_1ms;
                        fanstat=FANTRANS;
                    }
                    break;
                    
                case FANHIGH:
                    if(fantemp<(*fan_high-FANHYST)){
                        LATECLR=024;
                        fanstat=FANTRANS;
                        tmrins[4]=t_1ms;
                    }
                    break;
                    
                default:
                    LATECLR=024;
                    fanstat=FANOFF;
                    break;
            }
        }
        else {
            fanstat=FANOFF;
            LATECLR=0x00025;
        }
    }
}

//this function will be called if a digital input is in inactive state. The bit commandon will be true if the input is currently requesting the ON status. The function will 
//debounce the input and return true if debouncing time did elapse and the input still is required to be ON. Channel is the channel No. per routine checkextio, stats 
//is a static varibale from the calling function to manage debounce time
bool switchon(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon){
    bool retval=false;
    if(commandon){
        if(TESTBIT(*stats,channel)){
            if(t_1ms-*tmr>DIGDEBOUNCE)
                retval=true;   
        }
        else{
            SETBIT(*stats,channel);
            CLEARBIT(*stats,channel+4);
            *tmr=t_1ms;
        }
    }
    else{
        SETBIT(*stats,channel+4);
        CLEARBIT(*stats,channel);
    }
    return(retval);
}

//this function works similar to switchon but handles switching an active input off. Debounce time is the same. If comandon is false then the function will check for debouncing towards off state
bool switchoff(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon){
    bool retval=false;
    if(!commandon){
        if(TESTBIT(*stats,channel+4)){
            if(t_1ms-*tmr>DIGDEBOUNCE)
                retval=true;   
        }
        else{
            SETBIT(*stats,channel+4);
            CLEARBIT(*stats,channel);
            *tmr=t_1ms;
        }
    }
    else{
        SETBIT(*stats,channel);
        CLEARBIT(*stats,channel+4);
    }
    return(retval);
}

void readintad(void){
    uint16_t adin[14];
    uint8_t i;
    if(adccntr>64){
        for(i=0;i!=16;i++)
            adin[i]=adchan[i]>>6;
        restart_adc();
        wleakin=adin[3];        //will be processed along with digital inputs
    }
}