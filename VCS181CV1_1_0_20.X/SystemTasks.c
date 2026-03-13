#include "SystemTasks.h"

static void checkextio(bool init);
static bool switchon(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon);
static bool switchoff(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon);

void SysTasks(void){
    static uint32_t tmrx;
    switch(sysstat){
        case SYS_INIT:
            rq0100.status=true;       
            tmrx=t_1ms;
            sysstat=SYS_CONNECT;
            checkextio(true);
            initcomp();
            compcalc(true);
            checkextad_ac(true);
            break;
    }
}








void checkextio(bool init){
#define FANSW       0
#define AIRINSW     1
#define LOCALBS     2   //battle short local input
#define OILPRSW     3

    static uint8_t digstats;  //bits set in this variable will indicate an active input
    static uint32_t tmrins[4];
    
    if(init)
        digstats=0xF0;      //inidicates that all signals are off
    
    else{
        if(!TESTBIT(*stwordhigh,FANTEMPSWITCH)){    //switch is inactive
            if(switchon(&tmrins[FANSW],&digstats,FANSW,!PORTBbits.RB12))
                SETBIT(*stwordhigh,FANTEMPSWITCH);
        }
        else if(switchoff(&tmrins[FANSW],&digstats,FANSW,PORTBbits.RB12))
            CLEARBIT(*stwordhigh,FANTEMPSWITCH);
        if(!TESTBIT(*miscwordlow,AIRINTAKE)){
            if(switchon(&tmrins[AIRINSW],&digstats,AIRINSW,!PORTBbits.RB13))
                SETBIT(*miscwordlow,AIRINTAKE);
        }
        else if(switchoff(&tmrins[AIRINSW],&digstats,AIRINSW,PORTBbits.RB13))
            CLEARBIT(*miscwordlow,AIRINTAKE);
        if(!stat_bits.battles){
            if(switchon(&tmrins[LOCALBS],&digstats,LOCALBS,PORTDbits.RD6))      //battle short triggers on closing contact
                stat_bits.battles=true;
        }
        else if(switchoff(&tmrins[LOCALBS],&digstats,LOCALBS,!PORTDbits.RD6))      //battle short triggers on closing contact
            stat_bits.battles=false;
        if(gcontstat>GENCONTR_STATE_CRANKING){                                      //will not trigger oil pressure switch if generator is not running
            if(!TESTBIT(*stwordlow,POILSWITCH)){
                if(switchon(&tmrins[OILPRSW],&digstats,OILPRSW,!PORTDbits.RD7))
                    SETBIT(*stwordlow,POILSWITCH);
            }
            else if(switchoff(&tmrins[OILPRSW],&digstats,OILPRSW,PORTDbits.RD7))
                CLEARBIT(*stwordlow,POILSWITCH);
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