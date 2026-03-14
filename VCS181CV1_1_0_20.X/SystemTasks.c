#include "SystemTasks.h"

static uint16_t wleakin,tdisch[3],istep;

static void checkextio(bool init);
static bool switchon(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon);
static bool switchoff(uint32_t *tmr, uint8_t *stats,uint8_t channel,bool commandon);
static void readintad(void);
static void initspi2(void);
static void calctank(void);
static int16_t gettemp(uint16_t *chart, uint16_t adx,int8_t ofsx);
static int16_t findhottest(int16_t *coils);
static void checktemps(void);
static void initcomp(void);
static void compcalc(bool init);
static void checkrqs(void);
static void chkpanel(void);
static void checkfrq(void);
static void maintainruntime(void);
static void maintainenergy(void);
static void handlewarranty(void);
static uint8_t updactcom(void);
static void checksvn(void);
static void wrtx0104(void);


//ext. temp sensor, NTC element FP
const uint16_t tch_e[]= {   80,1015,1014,1014,1013,1012,1012,1011,1010,1009,1009,1008,1007,1006,1005,1004,1002,1001,1000,999,997,	//end of range element and -20 to -1 deg C
							996,994,993,991,989,987,986,984,981,979,977,975,972,970,967,964,962,959,956,952,	// 0 to 19 deg C
							949,946,942,939,935,931,927,922,918,913,908,903,898,893,888,883,877,871,865,859,	// 20 to 39 deg C
							853,847,840,834,827,820,813,806,798,791,783,775,767,759,751,743,734,726,717,708,	// 40 to 59 deg C
                            699,690,681,672,663,654,645,636,627,617,608,599,590,581,571,561,552,543,533,524,    // 60 to 79 deg C
                            513,505,496,486,477,467,458,450,441,431,422,414,406,397,389,380,372,365,357,349,    // 80 to 99 deg C
                            340,333,326,319,311,304,297,291,284,277,270,264,258,252,246,239,234,229,223,217,    // 100 to 119 deg C
                            212,207,202,197,192,187,183,178,174,169,165,161,157,153,149,145,142,138,135,131,    // 120 to 139 deg C	
                            128,125,122,119,116,113,110,107,105,102,99,97,95,93,90,88,86,84,82,80,              // 140 to 159 deg C
                            0};  

//temp. chart for internal sensor, NTC, 22kOhm size 0402. Reads -20 to +100 deg. C
const uint16_t tch_i[]={    111,
                            989,987,985,982,979,976,973,970,967,964,960,956,952,948,944,939,934,929,924,919,913,    //-20 to -0 degC
                            907,901,895,889,882,875,868,860,853,845,837,829,820,812,803,794,784,775,765,755,    //1 to 20 degC
                            745,735,725,715,704,693,682,672,661,649,638,627,616,605,593,582,571,559,548,537,    //21 to 40 degC
                            526,515,504,493,482,471,460,450,439,429,419,409,399,389,379,370,361,352,343,334,    //41 to 60 degC
                            325,317,308,300,292,285,277,270,262,255,248,242,235,229,222,216,210,204,199,193,    //61 to 80 degC
                            188,183,178,173,168,163,159,155,150,146,142,138,134,131,127,124,120,117,114,111,    //81 to 100 degC
};

//cosine of phase shift multiplied by 1000
const uint16_t cosalfa[]={1000,1000,999,999,998,996,995,993,990,988,985,982,978,974,970,966,961,956,951,946,940,934,927,921,914,906,899,891,883,875,866, //0..30
                        857,848,839,829,819,809,799,788,777,766,755,743,731,719,707,695,682,669,656,643,629,616,602,588,574,559,545,530,515,500,        //31-60
                        485,469,454,438,423,407,391,375,358,342,326,309,292,276,259,242,225,208,191,174,156,139,122,105,87,70,52,35,17,0,              // 61-90
};

//temp. char for heat sink temperature sensor, based on supply voltage 5V and reference 2.5V, with biasing resistor 15kOhm
uint16_t tch_ehs[]={    89,1005,981,957,933,910,887,864,841,819,797,776,755,734,714,694,674,655,636,618,600,583,566,549,533,517,501,486,472,458,444,430,417,405,393, //17..50 deg.c
                        381,369,358,347,336,326,316,307,297,288,279,271,263,255,247,240,232,225,219,212,206,200,194,188,183,177,172,167,162,157,153,148,144,140,136,132,128,125,121,118,114,111,108,105,102,99,97,94,92,89,0};//51..100C
 

void SysTasks(void){
    static uint32_t tmrx;
    switch(sysstat){
        case SYS_INIT:
            rq0100.status=true;       
            tmrx=t_1ms;
            sysstat=SYS_CONNECT;
            checkextio(true);
            readintad();
            initspi2();
            initcomp();
            compcalc(true);
//            checkextad_ac(true);
            break;
            
        case SYS_CONNECT:        //if upload is not over within 3 sec. the device will enter power down
            checkrqs();
            readintad();
            checkextio(false);
            chkpanel();
            checkfrq();
            compcalc(false);
            if(stat_bits.gotgp203){
                sysstat=SYS_ON;
                stat_bits.actcalini=true;
                tmrx=t_1ms;
            }
            else if((t_1ms-tmrx)>10000) 
                inlowpwr();
            break;
            
        case SYS_ON:            //system is on but generator is not running
//            checkrqs();
            readintad();
//            checkextio(false);
//            chkpanel();
//            checkfrq();
//            compcalc(false);
//            checkextad_dc();
//            checkextad_ac(false);
//            if(TESTBIT(*miscwordlow,4) && !stat_bits.nocmdata){
//                rq0138.status=rq013A.status=rq01B0.status=rq01B1.status=rq01B2.status=rq01B3.status=rq01B4.status=rq01B5.status=rq01B6.status=1;
//                CLEARBIT(*miscwordlow,4);
//            }
//            if(stat_bits.pwrdwn || (nocan>MAXNOCAN)){
//                sysstat=SYS_WAITSTOP;
//                stat_bits.pwrdwn=true;
//            }
//            maintainruntime();
//            maintainenergy();
//            if(!TESTBIT(rq01B6.status,0))
//                handlewarranty();
//            if((t_1ms-showstasto)>20000)
//                *stastoreas&=0x7F;
//            if((gcontstat>GENCONTR_STATE_CRANKING) && (gcontstat<GENCONTR_STATE_WAITUAC))
//                wrtx0104();
//            checksvn();
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
        *miscwordlow &= 0x9FFF;
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
                    SETBIT(*miscwordlow,13);
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
                    SETBIT(*miscwordlow,14);
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
    static uint32_t tmrw[3],tmra[3];
    static uint16_t stw,sta;
//monitoring assignment:
//bit 0:    Oil pressure
//bit 1:    Battery voltage low (warning only)
//bit 2:    Battery voltage high    
    
    
    uint16_t adin[14];
    int16_t coilx[3];
    uint8_t i;
    if(adccntr==64){
        for(i=0;i!=14;i++){
            adin[i]=adchan[i]>>6;
            adchan[i]=0;
        }
        adccntr=0;   
        
        //oil pressure
        if(TESTBIT(*sensconfl,5)){
            adin[0]=(adin[0]*VREF>>9)*1250/(5000);   //entire calculation in units of mV. Shift by 9 because voltage divider of 2x33kOhm
            if(adin[0]>125)
                *poil=(adin[0]-125)/10;
            else
                *poil=0;    
            if(*poil>*maxoil){
                CLEARBIT(*codeWarn,10);
                SETBIT(*stwordhigh,7);
                if(!st_feedb.val.alpending)
                    CLEARBIT(*codeAlm,10);
            }
            else if(stat_bits.ch_oil){
                if(delaycheck(*poil,*poil_w,200,&tmrw[0],&stw,0,false))
                    SETBIT(*codeWarn,10);
                else CLEARBIT(*codeWarn,10);
                if(delaycheck(*poil,*poil_o,*d_poil*100,&tmra[0],&sta,0,false))
                    SETBIT(*codeAlm,10);
                else if(!st_feedb.val.alpending)
                    CLEARBIT(*codeAlm,10);
            }
            else{
                CLEARBIT(stw,0);
                CLEARBIT(sta,0);         
                CLEARBIT(*codeWarn,10);
                if(!st_feedb.val.alpending)
                    CLEARBIT(*codeAlm,10);
            }
        }
        else{
            *poil=255;   //this sensor is disabled
            CLEARBIT(*codeWarn,10);
            CLEARBIT(stw,0);
            CLEARBIT(sta,0);
            if(!st_feedb.val.alpending){
                CLEARBIT(*codeAlm,10);
                CLEARBIT(*stwordhigh,7);
            }
        }
        
        //battery
        batvolt=*ubatt=((uint32_t)adin[1]*VREF*17/10)>>10;       //factor is 17, but unit is 10mV
        if(delaycheck(*ubatt,*ubattl_w,200,&tmrw[1],&stw,1,false))
            SETBIT(*mwordlow,0);
        else
            CLEARBIT(*mwordlow,0);
//no shdn on battery low
        if(delaycheck(*ubatt,*ubatth_w,200,&tmrw[2],&stw,2,true))
            SETBIT(*codeWarn,4);
        else
            CLEARBIT(*codeWarn,4);
        if(delaycheck(*ubatt,*ubatth_o,200,&tmra[2],&sta,2,true))
            SETBIT(*codeAlm,4);
        else if(!st_feedb.val.alpending)
            CLEARBIT(*codeWarn,4);
        
        //fuel tank
        if(TESTBIT(*sensconfl,6)){
            adin[2]>>=6;
            *tankres=300*(uint32_t)adin[2]/(1024-adin[2]);        //will not break this down to mV but stick with 10bit reference values
            if((*tankres<rtkmin) || (*tankres>rtkmax)){
                *tankres=9999;
                *tank=250;      //faulty tank
                SETBIT(*mwordhigh,4);
            }
            else if(!TESTBIT(*stwordlow,2))
                calctank();
        }
        else
            *tank=255;
        stat_bits.gottank=true;
        
//water leak is in fan breaker. Will be evaluated in   checkextio      
        wleakin=adin[3];        //will be processed along with digital inputs
        
        *actu=((uint32_t)adin[4]*VREF*249/(249+604))>>16;    //actuator voltage in multiples of 10mV
        if(adin[5]>adin[4]){
            *acti=((((uint32_t)adin[5]*VREF*249/(249+604))>>16)-(uint32_t)*actu)*454/100;        //dividing units of10mV by 0.22Ohm equals multiplying with 4.54
        }
        else
            *acti=0;
        stat_bits.gotacti=true;
        if((*actu<ACTUMINW) || (*actu>ACTUMAXW) || (*acti>ACTIMAXW))
            SETBIT(*codeWarn,12);
        else
            CLEARBIT(*codeWarn,12);
            
        if((*actu<ACTUMINO) || (*actu>ACTUMAXO) || (*acti>ACTIMAXO))
            SETBIT(*codeAlm,12);
        else if(!st_feedb.val.alpending)
            CLEARBIT(*codeWarn,12);
        
        if(!TESTBIT(*sensconfl,2)){
            *tcylh=255;
            CLEARBIT(*mwordhigh,4);
        }
        else{    
            *tcylh=gettemp((uint16_t *)tch_e,adin[6],-20);
            if(*tcylh == 999){
                SETBIT(*mwordhigh,4);
                *tcylh=255;
            }
        }
        
        if(!TESTBIT(*sensconfl,1)){
            *altbear=255;
            CLEARBIT(*mwordhigh,1);
        }
        else{
            *altbear=gettemp((uint16_t *)tch_e,adin[7],-20);
            if(*altbear == 999){
                SETBIT(*mwordhigh,1);
                *altbear=255;
            }
        }
        
        if(!TESTBIT(*sensconfl,3)){
            *texhm=255;
            CLEARBIT(*stwordlow,13);
        }
        else{
            *texhm=gettemp((uint16_t *)tch_e,adin[8],-20);
            if(*texhm == 999){
                SETBIT(*stwordlow,13);
                *texhm=255;
            }
        }
        
         if(!TESTBIT(*sensconfl,0)){
            *tcoolin=255;
            CLEARBIT(*mwordlow,14);
        }
        else{
            *tcoolin=gettemp((uint16_t *)tch_e,adin[9],-20);
            if(*tcoolin == 999){
                SETBIT(*mwordlow,14);
                *tcoolin=255;
            }
        }
        
        if(!TESTBIT(*sensconfl,4)){
            *tcoil=coilx[0]=coilx[1]=coilx[2]=255;
            CLEARBIT(*mwordlow,11);
        }
        else{
            for(i=0;i!=3;i++)
                coilx[i]=gettemp((uint16_t *)tch_e,adin[10+i],-20);
            *tcoil=findhottest(coilx);      
            if(*tcoil == 999)
                SETBIT(*mwordlow,11);
        }
        
        if(!TESTBIT(*sensconfl,9)){
            *coolout=255;
            CLEARBIT(*mwordhigh,9);
        }
        else{
            *coolout = gettemp((uint16_t *)tch_e,adin[13],-20);
            if(*coolout==999){
                SETBIT(*mwordhigh,9);
                *coolout=255;
            }
        }
        if(!SPI2CONbits.ON)
            initspi2();
        else if(u17cntr>64){
            *tvcs=gettemp((uint16_t *)tch_i,vcstemp>>6,-20);
            if(TESTBIT(*sensconfl,12)){
                *engoil=gettemp((uint16_t *)tch_e,oiltemp>>6,-20);
                if(*engoil==999){
                    *engoil=255;
                    SETBIT(*mwordhigh,10);
                }
                    
            }
            else
                *engoil=255;
            vcstemp=oiltemp=u17cntr=0;
            LATGCLR=0x0200;
            isr_flags |= RDVCSTEMP;
            SPI2BUF=0x6800; //tx bit for VCS temperature
        }
        checktemps();
    }
}

void checktemps(void){
    static uint16_t stw,sta;
    static uint32_t tmrw[14],tmra[14];
 //bit assignment   
    //bit 0: engine oil
    //bit 1: exhaust
    //bit 2: cyl. head
    //bit 3: coolant in. head
    //bit 4: alt. bearing
    //bit 5: coolant out
    //bit 6: winding
    
    //check for abs. max first
    if(!TESTBIT(*stwordlow,13))
        if(*texhm>*abs_exhm)
            SETBIT(*codeAlm,8);     //will only reset below shdn warning
    if(!TESTBIT(*mwordhigh,4))
        if(*tcylh > *abs_cylh)
            SETBIT(*codeAlm,7);
    if(TESTBIT(*mwordhigh,4) && TESTBIT(*mwordlow,14))
        SETBIT(*stwordlow,14);
    else if(!st_feedb.val.alpending)
        CLEARBIT(*stwordlow,14);
    if(*tvcs>MAXTEMPVCS)
        SETBIT(*mwordlow,2);

    
   if(TESTBIT(*sensconfl,1) && !TESTBIT(*mwordhigh,1)){     //alt. bearing
        if(delaycheck(*altbear,*altb_w,200,&tmrw[4],&stw,4,true))
            SETBIT(*codeWarn,11);
        else CLEARBIT(*codeWarn,11);  
        if(delaycheck(*altbear,*altb_o,*d_temps*100,&tmra[4],&sta,4,true))
            SETBIT(*codeAlm,11);
        else if(!st_feedb.val.alpending)
            CLEARBIT(*codeWarn,11);  
    }
    else{
        CLEARBIT(stw,4);
        CLEARBIT(sta,4);
        CLEARBIT(*codeWarn,11);  
        CLEARBIT(*codeAlm,11);
    }
    
    if(!TESTBIT(*mwordhigh,10) && TESTBIT(*sensconfl,12)){        //oil temp
        if(delaycheck(*engoil,*toil_w,200,&tmrw[0],&stw,0,true))
            SETBIT(*codeWarn,13);
        else CLEARBIT(*codeWarn,13);  
        if(delaycheck(*engoil,*toil_o,*d_temps*100,&tmra[0],&sta,0,true))
            SETBIT(*codeAlm,13);
        else if(!st_feedb.val.alpending)
            CLEARBIT(*codeWarn,13);  
    }
    else{
        CLEARBIT(stw,0);
        CLEARBIT(sta,0);
        CLEARBIT(*codeWarn,13);  
        CLEARBIT(*codeAlm,13);
    }

    if(stat_bits.checkexhm){
        if(!TESTBIT(*stwordlow,13) && TESTBIT(*sensconfl,3)){       //exhaust manifold
            if(delaycheck(*texhm,*exhm_w,200,&tmrw[1],&stw,1,true))
                SETBIT(*codeWarn,8);
            else CLEARBIT(*codeWarn,8);  
            if(delaycheck(*texhm,*exhm_o,*d_temps*100,&tmra[1],&sta,1,true))
                SETBIT(*codeAlm,8);
            else if(!st_feedb.val.alpending)
                CLEARBIT(*codeWarn,8);  
        }
        else{
            CLEARBIT(stw,1);
            CLEARBIT(sta,1);
            CLEARBIT(*codeWarn,8);  
            CLEARBIT(*codeAlm,8);
        }
    }

    //all other temperatures only after water flow is good
    if(stat_bits.ch_water){
        if(!TESTBIT(*mwordhigh,4) && TESTBIT(*sensconfl,2)){     //cyl. head 
            if(delaycheck(*tcylh,*cylh_w,200,&tmrw[2],&stw,2,true))
                SETBIT(*codeWarn,7);
            else CLEARBIT(*codeWarn,7);  
            if(delaycheck(*tcylh,*cylh_o,*d_temps*100,&tmra[2],&sta,2,true))
                SETBIT(*codeAlm,7);
            else if(!st_feedb.val.alpending)
                CLEARBIT(*codeWarn,7);  
        }
        else{
            CLEARBIT(stw,2);
            CLEARBIT(sta,2);
            CLEARBIT(*codeWarn,7);  
            CLEARBIT(*codeAlm,7);
        }
        
        if(!TESTBIT(*mwordhigh,14) && TESTBIT(*sensconfl,0)){        //coolant in 
            if(delaycheck(*tcoolin,*coolin_w,200,&tmrw[3],&stw,3,true))
                SETBIT(*codeWarn,5);
            else CLEARBIT(*codeWarn,5);  
            if(delaycheck(*tcoolin,*coolin_o,*d_temps*100,&tmra[3],&sta,3,true))
                SETBIT(*codeAlm,5);
            else if(!st_feedb.val.alpending)
                CLEARBIT(*codeWarn,5);  
        }
        else{
            CLEARBIT(stw,3);
            CLEARBIT(sta,3);
            CLEARBIT(*codeWarn,5);  
            CLEARBIT(*codeAlm,5);
        }
        
        if(!TESTBIT(*mwordhigh,9) && TESTBIT(*sensconfl,9)){        //coolant out 
            if(delaycheck(*coolout,*coolout_w,200,&tmrw[5],&stw,5,true))
                SETBIT(*codeWarn,6);
            else CLEARBIT(*codeWarn,6);  
            if(delaycheck(*coolout,*coolout_o,*d_temps*100,&tmra[5],&sta,5,true))
                SETBIT(*codeAlm,6);
            else if(!st_feedb.val.alpending)
                CLEARBIT(*codeWarn,6);  
        }
        else{
            CLEARBIT(stw,5);
            CLEARBIT(sta,5);
            CLEARBIT(*codeWarn,6);  
            CLEARBIT(*codeAlm,6);
        }
        
        if(!TESTBIT(*mwordlow,11) && TESTBIT(*sensconfl,4)){        //coil 
            if(delaycheck(*tcoil,*coil_w,200,&tmrw[6],&stw,6,true))
                SETBIT(*codeWarn,9);
            else CLEARBIT(*codeWarn,9);  
            if(delaycheck(*tcoil,*coil_o,*d_temps*100,&tmra[6],&sta,6,true))
                SETBIT(*codeAlm,9);
            else if(!st_feedb.val.alpending)
                CLEARBIT(*codeWarn,9);  
        }
        else{
            CLEARBIT(stw,6);
            CLEARBIT(sta,6);
            CLEARBIT(*codeWarn,9);  
            CLEARBIT(*codeAlm,9);
        }
    }
}


void initspi2(void){
    SPI2CONbits.ON=false;
    IEC1bits.SPI2RXIE=IEC1bits.SPI2EIE=true;
    IFS1bits.SPI2RXIF=IFS1bits.SPI2EIF=false;
    vcstemp=oiltemp=u17cntr=0;
    LATGCLR=0x0200;
    isr_flags |= RDVCSTEMP;
    SPI2BUF=0x6800; //tx bit for VCS temperature
}

//will calculate tank level. Will also set tank alarm level
//allow a hysteresis of 10% on scale ends, prior to indicating fault
void calctank(void){
    static uint32_t tklim;
    static bool tkal;
    CLEARBIT(*mwordhigh,2);
    if(*rtkfull<*rtkempty){        //res. at full tank is lowest
        if(*tankres<=*rtkfull)
            *tank=100;       //full tank
        else if(*tankres>=*rtkempty)
            *tank=0;
        else
            *tank=(uint32_t)(100)*(*rtkempty-*tankres)/(*rtkempty-*rtkfull);
    }
    else{   //res at empty tank is lowest
        if(*tankres<=*rtkempty)
            *tank=0;
        else if(*tankres>=*rtkfull)
            *tank=100;
        else
            *tank=(uint32_t)(100)*(*tankres-*rtkempty)/(*rtkfull-*rtkempty);
    }
    if(*tank<*tk_low){
        if(tkal){
            if((t_1ms-tklim)>(*d_tank*1000))
                SETBIT(*mwordlow,4);
        }
        else{
            tkal=true;
            tklim=t_1ms;
        }
    }
    else{
        CLEARBIT(*mwordlow,4);
        tkal=false;
    }
}

//will calculate temperature based off a certain chart. First item in that chart is the lowest permissible ADC value, next one is the highest. Will add offset ofsx to the result
int16_t gettemp(uint16_t *chart, uint16_t adx,int8_t ofsx){
    int16_t i=0;
    if(adx<*chart++)
        return(999);
    else if(adx>*chart)
        return(999);
    else while(adx<*chart++)
        i++;
    return(i+ofsx);
}

int16_t findhottest(int16_t *coils){
    int16_t coil1,coil2,coil3,coilmax;
    coil1=*coils++;
    coil2=*coils++;
    coil3=*coils;
    if(coil1>249)
        coil1=-40;  //invalidate this
    if(coil2>249)
        coil2=-40;
    if(coil3>249)
        coil3=-40;
    if(coil1>coil2)
        coilmax=coil1;
    else
        coilmax=coil2;
    if(coil3>coilmax)
        coilmax=coil3;
    if(coilmax==-40)
        return(255);
    else
        return(coilmax);
}

//this function will calculate compensation current and discharge time 
void initcomp(void){
//calculate discharging time. Resistors are 220kOhm. Assume the charge needs to decrease down to 1/e (app. 36%) of initial charge
//in 3~ systems one resistor is across one capacitor. Solving V=V0*e^-(t/R*C) and V/V0=e yields t=R*C. With R=.22*10^-6Ohm and C in uF (10^-6)
//the time t in ms may be calculated as 220*C, with C in uF. This is for bank 0; will be double for bank 1 and 4 times this time for banks 2..3                                
    tdisch[0]=220*(*compb0);
    tdisch[1]=tdisch[0]<<1;
    tdisch[2]=tdisch[3]=tdisch[1]<<1;   //bank 3 and bank 4 have the same discharge time
    
    if(TESTBIT(*genconfl,15))      //on 3~ generators caps are across line- to line-voltage but nominal voltage is line to neutral
//                                 //this introduces a factor of sqr(3) for voltage and again sqr(3) for caps assembly. Thus factor of 3 is good
        istep=(uint32_t)189*((*fac_nom)/10)*(*uoutnom/10)*(*compb0)/1000000; //2pi and factor of 3 are rounded to 189/10

    else if((*genconfl & 3)==0)    //on 120/240 generators the voltage again is line to N but caps are across the 240 line, so factor is 2
        istep=(uint32_t)126*((*fac_nom)/10)*(*uoutnom/10)*(*compb0)/1000000; //2pi and factor of 2 are rounded 126/10
    else istep=0;   //no compensation for 120V generators, not supported. 
}

//this routine will also handle the radiator fan
void compcalc(bool init){
#define BANK_DISCH  0   //comp. bank is discharging
#define BANK_ON     1   //comp. bank is connecting
#define BANK_READY  3   //comp. bank ready to connect

#define STAT_POR    0   //compensation initialized
#define STAT_READY  1   //can be used
#define STAT_UPDOUT 2   //update outputs    
    
#define MAX_SPIKES  4   //will enter cutoff mode if the voltage did over-spike fot this amount of sinewaves
    
    static uint8_t compstat[4],nnoi,numax;        //11: off and ready, 01: on, 00: discharging. 
                                                        //fan: 00 off, not required. 01: in low speed; 02: in high speed                                                        //bit 7 indicates that transition is pending
    static uint32_t tcomp[4],updtmr;
    static uint8_t stat;                   
    uint8_t i,compact;
    if(init){   
        for(i=0;i!=4;i++)
            compstat[i]=BANK_READY;
        LATECLR=0x001E0;         //all banks and all relays off
        *banks=0;
        nnoi=numax=0;
        updtmr=t_1ms;
        stat=STAT_POR;
    }
    else{
        //update status of discharging capacitors
        for(i=0;i!=4;i++){
            if((compstat[i]==BANK_DISCH)&&((t_1ms-tcomp[i])>tdisch[i]))  //bank 0 compare against tdisch
                compstat[i]=BANK_READY;
        }
        switch(stat){
            case STAT_POR:
                if(*uac1>(*uoutnom -20))
                    stat=STAT_READY;
                LATECLR=0x001E0;
                *banks=0;
                for(i=0;i!=4;i++)
                    if(compstat[i]==BANK_ON ){
                        compstat[i]=BANK_DISCH;
                        tcomp[i]=t_1ms;
                    }    
                break;
            case STAT_READY:
                if(*uac1>((*uoutnom) *11/10)){    //overvoltage, turn off
                    if(numax>MAX_SPIKES){
                        numax=0;
                        compact=0;      //required banks 
                    }
                    else
                        numax++;
                }
                else if(*uac1<((*uoutnom) *9/10))
                    compact=numax=0;
                else{
                    numax=0;
                    if((t_1ms-updtmr)>COMPCYC){
                        updtmr=t_1ms;
                        
                        //no compensation if current on leg 1 is less than 2x current per step or if pF is capacitive or above 0.97
                        if((*cosfi >970) || (*cosfi<0) || (*iac1<(istep<<1))){
                            if(nnoi>MAX_SPIKES)    //if reading PF not requiring PFC then update. 
                                *banks=0;
                            else
                                nnoi++;
                        }
                        else if(istep>0){
                            nnoi=0;
                            compact=(2*(*iac1)*sinfi)/istep;        //add a factor of 2 to compensate for rounding errors.
                            compact>>=1;        //eliminate rounding factor
                        }
                    }
                }
                stat=STAT_UPDOUT;
                break;
                
            case STAT_UPDOUT:
                if(TESTBIT(*mwordlow,15))    
                    compact=0;
                else if(TESTBIT(*miscwordlow,10))
                    compact&=0x07;
                //first turn off all banks that are no longer required
                for(i=0;i!=4;i++){
                    if(!TESTBIT(compact,i)){
                        if(compstat[i]==BANK_ON){
                            LATECLR=0x20<<i;
                            compstat[i]=BANK_DISCH;
                            tcomp[i]=t_1ms;
                        }
                    }
                }
                //now turn on required banks. 
                for(i=0;i!=4;i++){
                    if(TESTBIT(compact,i)){
                        if(compstat[i]==BANK_READY){
                            LATESET=0x20<<i;
                            compstat[i]=BANK_ON;
                            SETBIT(*banks,i);
                        }
                    }
                }
                break;
                
            default:
                stat=STAT_POR;
                break;       
        }
    }
}

//will send pending requests right away or repeat requests. Will also monitor CAN bus and set an alarm if it times out
void checkrqs(void){
    uint8_t i;
    for(i=0;i!=MAXRQS;i++)
        if(rqs[i]->status==1)
            sendrq(rqs[i]);
        else if(rqs[i]->status==2)
            if((t_1ms-rqs[i]->lastissued)>RQUPD)
                sendrq(rqs[i]);       
}

void chkpanel(void){
    uint8_t i=0;
    bool isfb,battles;
    isfb=false;
    battles=stat_bits.battles;  //include the local battle short
    if((t_1ms-lastrx0005)>2500)     
        SETBIT(*mwordhigh,5);
    else{
        CLEARBIT(*mwordhigh,5);
        for(i=0;i!=8;i++){
            if(panels[i].sourceadr){     //this makes sure not to read from panels which are already disabled
                if(t_1ms-(panels[i].lastcom)>5000){
                    panels[i].sourceadr=0;
                    panels[i].pansw=0xFFFFFFFF;
                }
                else{
                        //check if any fire boy input is pending 
                    if(TESTBIT(panels[i].ext_contr,4) && TESTBIT(*genconfl,9))
                        isfb=true;
                    if(TESTBIT(panels[i].ext_contr,5))
                        battles=true;
                    if(TESTBIT(*genconfl,8) && (panels[i].sourceadr>0x20)) //static inputs requested, no more than one panel allowed
                        SETBIT(*stwordhigh,8);
                    if((panels[i].pansw==0) || (panels[i].pansw==0xFFFFFFFF))
                        SETBIT(*mwordhigh,0);
                    else
                        CLEARBIT(*mwordhigh,0);     //if the panel did send a transfer 5 before it ID'd itself then a fals "not programmed" message may be triggered
                }     
            }
        }
        
        if(isfb && TESTBIT(*genconfl,9))
            SETBIT(*stwordhigh,1);        //fire boy
        else if(!st_feedb.val.alpending)
            CLEARBIT(*stwordhigh,1);

        if(battles)
            SETBIT(*miscwordlow,3);
        else
            CLEARBIT(*miscwordlow,3); 
    }
}

void checkfrq(void){
    uint32_t flags;
    uint32_t per_v;
    uint32_t per_i;
    uint32_t sumx;
    uint8_t i;
    bool iscap=false;
    
    static uint16_t fac[8];
    static uint16_t fcntr;
    static uint32_t lastf;
    
    __builtin_disable_interrupts();
    flags=isr_flags;
    per_v=v_period;
    per_i=i_period;
    isr_flags &= ~flags;
    __builtin_enable_interrupts();
    
    if(flags & GOTUPULS){   //per_v is in units of 1.6us
        if((per_v<MAXPER) && (per_v>MINPER)){
            fac[fcntr]=6250000/per_v;
            fcntr=(fcntr+1)%8;
            lastf=t_1ms;
            sumx=fac[0];
            for(i=1;i!=8;i++)
                sumx+=(uint32_t)fac[i];
            *fac=sumx>>3;
            *engspeed=(*fac)*12/((*poles) *10);
            stat_bits.gotspeed=true;
        }
    }
    //the period reported from timer 1 interrupt is always the delta between the last voltage pulse and the current pulse. In case of capacitive loading this could be reflect as a very long period, theoretically almost as long as the 
    //period of the voltage signal. In this case the time sensed from last voltage pulse - which in fact was the voltage pulse before this current pulse- and the current pulse needs to be decreased by 1 voltage period.
    //the (negative) difference between the two then is the capacitive power factor. Practical limits will be set to 5*PI/12 (75deg) phase shift either way. Larger phase angle will be considered to be an error 

    if(flags & GOTIPULS){   //the period reported from timer 1 interrupt is always the delta between the last voltage pulse and the current pulse. In case of capacitive loading this could be reflect as a very long 
        if((per_i)>(per_v)){
            iscap=true;
            per_i=per_i-per_v;
        }
        if(per_i>(per_v*5/24)){        //max period shift, anything else will trigger phase shift 9999, meaning invalid
            *cosfi=9999;
            sinfi=0;
        }
        else{
            sumx=90*per_i/(per_v>>2);           //calculate phase shift in degrees. 
            if(sumx>89){
                *cosfi=0;
                sinfi=1;
            }
            else{
                *cosfi=cosalfa[sumx];
                sinfi=cosalfa[90-sumx];
                if(iscap)
                    (*cosfi) *= -1;
            }
        }
        stat_bits.gotsinfi=true;
    }
    if((t_1ms-lastf)>100){
        lastf=t_1ms;
        for(i=0;i!=8;i++)
            fac[i]=0;
        fcntr=*fac=*engspeed=0;
        *cosfi=1000;
    }
}


void maintainruntime(void){
    static uint32_t lastlogged;
    if(*engspeed<MINSPEED)
        lastlogged=t_1ms;
    
    else if((t_1ms-lastlogged)>60000){
        lastlogged=t_1ms;
        tx013A.v32.optime++;
        (*runlast)++;
        writeeeprom(0x1E8,tx013A.trans,2);
    }
}

//if the control system is connected to a communication interface then the "generate" command can be issued by that interface by using transfer 3 / bit 31
void maintainenergy(void){
    static uint32_t storetmr;
    static uint32_t entmr;
    if((t_1ms-entmr)>3600){
        entmr=t_1ms;
        if(*pabs>0){
            enrem+=*pabs;       //Pabs is in units of 100W. Integrated over 3.6 seconds yields units of 0.1wh.
            if(enrem>10000){             //every 10000 increments of en yields 1kwh
                tx013A.v32.energy=tx013A.v32.energy+(((uint32_t)enrem/10000)&0xFFFF);
                *enlast+=enrem/10000;
                enrem=enrem%10000;
                writeeeprom(0x01F0,&tx013A.val.totalenl,2);
                writeeeprom(0x01F4,&enrem,1);
                storetmr=t_1ms;
            }
            else if((t_1ms-storetmr)>36000){ //store energy remainder every 36 seconds, even if a full kwH did not add up
                storetmr=t_1ms;
                writeeeprom(0x01F4,&enrem,1);
            }
        } 
    }
}

//this function will handle the warranty. It will only be called if warranty tracking is enabled. It will run once a minute
void handlewarranty(void){
    static uint32_t tmrx;
    uint32_t u32x;
    if(((t_1ms-tmrx)>60000) && ((t_1ms-lastrx0005)<10000)){
        tmrx=t_1ms;
    //    first find out if warranty did start
        if(TESTBIT(tx01B6.val.status,2)){//warranty did start
            if(!TESTBIT(tx01B6.val.status,3)){  //check if warranty did expire
                if((caldate.trans>(tx01B6.val.warrantydate_l | (tx01B6.val.warrantydate_h <<16))) || (tx013A.v32.optime>WARRANTYMINUTES)){                
                    SETBIT(tx01B6.val.status,3);
                    tx01B6.val.warrantydate_l = caldate.trans & 0xFFFF;
                    tx01B6.val.warrantydate_h = caldate.trans >>16;
                    writeeeprom(0x02B0,tx01B6.trans,5);
                }
            }
        }
        else{   //check if warranty should be started
            if(caldate.trans-(tx01B6.val.shipdate_l | (tx01B6.val.shipdate_h <<16))>0x02000000){        //this would be 1 year
                SETBIT(tx01B6.val.status,2);    //trigger based on time
                u32x=(tx01B6.val.shipdate_l | (tx01B6.val.shipdate_h <<16))+0x0C000000;        //adding 6 years to shipping date, 
                tx01B6.val.warrantydate_l=u32x & 0xFFFF;
                tx01B6.val.warrantydate_h=u32x >>16;
                writeeeprom(0x02B0,tx01B6.trans,5);
                initsvn();
            }
            else if(tx013A.v32.optime>WARRANTYSTART){           //this would be 5 hours of operating time
                SETBIT(tx01B6.val.status,2);       //trigger from today, 5 years
                u32x=caldate.trans + 0x0A000000;      //add 5 years
                tx01B6.val.warrantydate_l=u32x & 0xFFFF;
                tx01B6.val.warrantydate_h=u32x >>16;
                writeeeprom(0x02B0,tx01B6.trans,5);
                initsvn();
            }
        }
    }
}

void wrtx0104(void){ //will first shift down all rows by 1 then overwrite row 39 with new data 
    static uint32_t tmr;
    uint8_t i,k;
    if((t_1ms-tmr)>=250){
        tmr=t_1ms;
        *comact=updactcom();
        for(i=1;i!=40;i++)
            for(k=0;k!=16;k++)
                tx0104[i-1][k]=tx0104[i][k];    //shift down by one row
        if(actmax>=actmin)
            k=0;        //act. position, calculate for later
        else
            k=(uint16_t)(OC3RS-actmax)*200/(actmin-actmax);
        tx0104[39][0]=*uac1;   //ac voltage 1
        tx0104[39][1]=*uac2;   //ac voltage 2
        tx0104[39][2]=*uac3;   //ac voltage 3
        tx0104[39][3]=*iac1;   //ac current 1
        tx0104[39][4]=*iac2;    //ac current 2
        tx0104[39][5]=*iac3;   //ac current 3
        tx0104[39][6]=*freq;             
        tx0104[39][7]=*cosfi;    
        tx0104[39][8]=*ubatt;
        tx0104[39][9]=*poil|(k<<8) | (*comact <<9); //shift by 9 because data i sent in units of 0.5%
        tx0104[39][10]=*engspeed;
        tx0104[39][11]=*actu;
        tx0104[39][12]=*acti;
        tx0104[39][13]=*mwordlow;
        tx0104[39][14]=*mwordhigh;
        tx0104[39][15]=*codeWarn;    
    }
}

uint8_t updactcom(void){
    uint8_t retval=0;
    if(TESTBIT(*mwordlow,12))
        retval=0xFF;
    else if(OC5CONbits.ON && !PORTAbits.RA6){
        if(OC5RS>=actmin)
            retval=0;
        else if(OC5RS<=actmax)
            retval=100;
        else
            retval=(uint32_t)(actmin-OC5RS)*100/(actmin-actmax);
    }
    return(retval);
}

//this routine will check if any service message became pending. If that is the case it will set the corresponding bit in servword low or servword high 
//No further action required here; resetting a service notification will update transfers 01B0 and 01B1 which are the basis for this function
//Will execute once per 2 seconds
void checksvn(void){
    static uint32_t tmrx;
    uint8_t i;
    if(((t_1ms-tmrx)>2000) && (gcontstat>GENCONTR_STATE_CALPROGFAIL) && (panelx20.nodesw>0x0201002C)){ 
        tmrx=t_1ms;
        for(i=0;i!=16;i++)  
            if(TESTBIT(valsn,i))
                if((tx01B0.b32[2*i]<tx013A.v32.optime) || (tx01B0.b32[2*i+1]<caldate.trans))
                    SETBIT(*pendsnl,i);
        for(i=0;i!=16;i++) 
            if(TESTBIT(valsn,i+16))
                if((tx01B1.b32[2*(i+16)]<tx013A.v32.optime) || (tx01B1.b32[2*(i+16)+1]<caldate.trans))
                    SETBIT(*pendsnh,i);    
    }
}