#include "SYSTasks.h"
static void remstart(void);

//manage system tasks, mainly service notification. runs once every 20 seconds
void SYS_Tasks(void){
    static uint32_t tsystasks;
    uint8_t i;
    uint16_t rvx[4];
    remstart();

    if((t_1ms-tsystasks)>20000){    //re-test every 20 seconds
        checksvn();
        tsystasks=t_1ms;
        if(valsvn){
            for(i=0;i!=32;i++){
                if(TESTBIT(valsvn,i)){
                    readeeprom(0x0228+i*0x40,rvx,4);
                    if(tx013A.v32.optime>=(rvx[0]|(rvx[1]<<16))){//check for op. time
                        if(i>15)
                            SETBIT(tx0101.val.servwordhigh,i-16);
                        else
                            SETBIT(tx0101.val.servwordlow,i);
                    }
                    else{
                        if(i>15)
                            CLEARBIT(tx0101.val.servwordhigh,i-16);
                        else
                            CLEARBIT(tx0101.val.servwordlow,i);
                        
                    }
                    if(calok){  //will not check on CAL time if CAL time is invalid
                        if(caldate>=(rvx[2]|(rvx[3]<<16))){
                            if(i>15)
                                SETBIT(tx0101.val.servwordhigh,i-16);
                            else
                                SETBIT(tx0101.val.servwordlow,i);
                        }
                        else{
                            if(i>15)
                                CLEARBIT(tx0101.val.servwordhigh,i-16);
                            else
                                CLEARBIT(tx0101.val.servwordlow,i);
                        }
                    }
                }
            }
        }
    }
}


//will check if remote start (static)  was issued. Will manipulate newin if required. Will trigger demand mismatch if applicable
void remstart(void){
    static bool laston;
    static uint32_t tmrx;
    if(!TESTBIT(tx0101.val.mwordhigh,8) && (gencontrstate>GENCONTR_STATE_ACTCALINICOM)){    //remote start is not set
        if(PORTDbits.RD6){
            if(laston){
                if((t_1ms-tmrx)>100){       //debounce for 100ms
                    SETBIT(tx0101.val.mwordhigh,8);
                    if(!stfeedb.fields.actstat && !stfeedb.fields.startinhib){
                        stfeedb.fields.newin=true;      //trigger engine start    
                        stfeedb.fields.req_in=true; 
                    }
                }
            }
            else{
                laston=true;
                tmrx=t_1ms;
            }
        }
        else
            laston=false;
    }
    else
        laston=false;
}