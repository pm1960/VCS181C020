#include "cpusetup.h"

volatile uint32_t t_1ms=0;
volatile uint32_t v_period=0; 
volatile uint32_t i_period=0;
volatile uint32_t isr_flags=0;
volatile uint16_t nocan=0;
volatile uint16_t adchan[8]={0};
volatile uint16_t isoresptr=0xFF;
volatile uint8_t adccntr=0;
volatile uint8_t muxptr=0;
volatile uint16_t isoptrs=0;

volatile ACCHANNEL u1[2]={0};
volatile ACCHANNEL u2[2]={0};
volatile ACCHANNEL u3[2]={0};
volatile ACCHANNEL i1[2]={0};
volatile ACCHANNEL i2[2]={0};
volatile ACCHANNEL i3[2]={0};

volatile ACCHANNEL *acchans[12]={&u1[0],&u1[1],&u2[0],&u2[1],&u3[0],&u3[1],&i1[0],&i1[1],&i2[0],&i2[1],&i3[0],&i3[1]};
volatile DCCHANNEL pfctemp[2]={0};
volatile DCCHANNEL iso15[2]={0};

volatile ACCHANNEL *acchptr[2][6] = {
    {   // row 0 ? index 0 of each channel
        &u1[0], &u2[0], &u3[0],
        &i1[0], &i2[0], &i3[0]
    },
    {   // row 1 ? index 1 of each channel
        &u1[1], &u2[1], &u3[1],
        &i1[1], &i2[1], &i3[1]
    }
};

volatile DCCHANNEL *dcchptr[2][2]={
    {&pfctemp[0],&iso15[0]},
    {&pfctemp[1],&iso15[1]}
};

volatile SPI_MCP_STATE spistep =SPI_MCP_START;
volatile uint8_t spichannel =0;
volatile CANCOM_STATES cancomstat=CAN_STATE_INIT;
volatile CANRxMessageBuffer canrxbuf[256]={0};
volatile uint8_t canrxptr=0;

RTCX caldate={.trans=0};
HWTYPE hwtype=UNKNOWN;
TX0101 tx0101={ .trans_w ={0}};
TX0130 tx0130={.trans_w ={0}};
TX013A tx013A={.trans={0}};
TX000A tx000A ={.trans ={0}};
TX0003 tx0003 ={.trans={0}};
TX0138 tx0138 ={.trans ={0}};
TX01B6 tx01B6 ={.trans={0}};
TXB3232 tx01B0={.b32={0}};
TXB3232 tx01B1={.b32={0}};
TXB3232 tx01B2={.b32={0}};
TXB3232 tx01B3={.b32={0}};
TXB3232 tx01B4={.b32={0}};
TXB3232 tx01B5={.b32={0}};
RQHANDLER rq0100p={0};
RQHANDLER rq0100={0};
RQHANDLER rq0005={0};
RQHANDLER rq000B={0};
RQHANDLER rq0138={0};
RQHANDLER rq013A={0};
RQHANDLER rq01B0={0};
RQHANDLER rq01B1={0};
RQHANDLER rq01B2={0};
RQHANDLER rq01B3={0};
RQHANDLER rq01B4={0};
RQHANDLER rq01B5={0};
RQHANDLER rq01B6={0};
RQHANDLER* rqs[MAXRQS]={&rq0100,&rq0005,&rq000B,&rq0138,&rq013A,&rq01B0,&rq01B1,&rq01B2,&rq01B3,&rq01B4,&rq01B5,&rq01B6};
RQHANDLER x01B0={0};
RQHANDLER x01B1={0};
RQHANDLER x01B2={0};
RQHANDLER x01B3={0};
RQHANDLER x01B4={0};
RQHANDLER x01B5={0};
PANEL panels[9]={0};
CANNODE panelx20={0};
CANNODE iv217={0};
ST_FEEDB st_feedb={.trans={0}};
uint16_t tx0104[40][16]={0};
CANSFR sfr0 = {.trans={0}};
CANSFR sfr1 = {.trans={0}};
CANSFR sfr2 = {.trans={0}};
CANSFR sfr3 = {.trans={0}};
CANSFR sfr4 = {.trans={0}};
CANSFR sfr5 = {.trans={0}};
CANSFR sfr6 = {.trans={0}};
CANSFR sfr7 = {.trans={0}};
CANMFR mfr0 = {0};
CANMFR mfr1 = {0};
CANMFR mfr2 = {0};
CANMFR mfr3 = {0};
CANMFR mfr4 = {0};
CANMFR mfr5 = {0};
CANMFR mfr6 = {0};
CANMFR mfr7 = {0};
uint8_t mfdata[MFBUFS][223] ={0};

CANSFR *sfrx[SFBUFS]={&sfr0,&sfr1,&sfr2,&sfr3,&sfr4,&sfr5,&sfr6,&sfr7};
CANMFR *mfrx[MFBUFS]={&mfr0,&mfr1,&mfr2,&mfr3,&mfr4,&mfr5,&mfr6,&mfr7};


STAT_BITS stat_bits={0};
SYS_STAT sysstat=SYS_INIT;
GEN_APP_STATUS gcontstat=GENCONTR_STATE_INIT;


uint32_t valsn=0;
uint32_t perssn=0;
uint32_t lastuserint=0;
uint32_t lastrx0005=0;
uint32_t now=0;
uint32_t showstasto=0;

uint16_t enrem=0;
uint16_t alptr=0;
uint16_t evptr=0;
uint16_t lgreq=0;
uint16_t stastoptr=0;
uint16_t rtkmin=0;
uint16_t rtkmax=0;
uint16_t sinfi=0;
uint16_t actmax=0;
uint16_t actmin=0;
//transfer 0x0101
    uint16_t *statvar=&tx0101.trans_w[0]; 
    uint16_t *uac1=&tx0101.trans_w[1]; 
    uint16_t *uac2=&tx0101.trans_w[2];
    uint16_t *uac3=&tx0101.trans_w[3];
    uint16_t *iac1=&tx0101.trans_w[4];
    uint16_t *iac2=&tx0101.trans_w[5];
    uint16_t *iac3=&tx0101.trans_w[6];
    uint16_t *freq=&tx0101.trans_w[7];
    uint16_t *pabs=&tx0101.trans_w[8];
    uint16_t *prel=&tx0101.trans_w[9];
    int16_t  *cosfi=&tx0101.trans_w[10]; 
    uint16_t  *ubatt=&tx0101.trans_w[11];
    int16_t   *tcoolin=&tx0101.trans_w[12];   
    int16_t   *tcylh=&tx0101.trans_w[13];  
    int16_t   *texhm=&tx0101.trans_w[14];  
    int16_t   *tcoil=&tx0101.trans_w[15];  
    int16_t   *tvcs=&tx0101.trans_w[16];  
    int16_t   *altbear=&tx0101.trans_w[17];
    uint8_t   *poil=&tx0101.trans_b[36];
    uint8_t   *tank=&tx0101.trans_b[37];
    uint16_t  *engspeed=&tx0101.trans_w[19];
    uint16_t  *actu=&tx0101.trans_w[20];
    uint16_t  *acti=&tx0101.trans_w[21];
    uint16_t  *ifuelp=&tx0101.trans_w[22];
    uint16_t  *runlast=&tx0101.trans_w[23];
    uint16_t  *enlast=&tx0101.trans_w[24];
    uint16_t  *codeWarn=&tx0101.trans_w[25];
    uint16_t  *codeAlm=&tx0101.trans_w[26];
    uint16_t  *mwordlow=&tx0101.trans_w[27];
    uint16_t  *mwordhigh=&tx0101.trans_w[28];
    uint16_t  *stwordlow=&tx0101.trans_w[29];
    uint16_t  *stwordhigh=&tx0101.trans_w[30];
    uint16_t  *miscwordlow=&tx0101.trans_w[31];
    uint16_t  *miscwordhigh=&tx0101.trans_w[32];
    uint16_t  *pendsnl=&tx0101.trans_w[33];
    uint16_t  *pendsnh=&tx0101.trans_w[34];
    uint8_t   *comact=&tx0101.trans_b[70];
    uint8_t   *banks=&tx0101.trans_b[71];
    uint16_t  *tankres=&tx0101.trans_w[36];
    int16_t   *engoil=&tx0101.trans_w[37];
    int16_t   *coolout=&tx0101.trans_w[38];
    uint8_t   *comptemp=&tx0101.trans_b[78];
    uint8_t   *stastoreas=&tx0101.trans_b[79];
    
    
    uint16_t *idlespeed=&tx0130.trans_w[0];
    uint16_t *nomspeed=&tx0130.trans_w[1];
    uint16_t *uoutnom=&tx0130.trans_w[2];
    uint16_t *treg=&tx0130.trans_w[3];
    uint16_t *kp=&tx0130.trans_w[4];
    uint16_t *kd=&tx0130.trans_w[5];
    uint16_t *ki=&tx0130.trans_w[6];
    uint16_t *maxint=&tx0130.trans_w[7];
    uint16_t *uouth_w=&tx0130.trans_w[8];
    uint16_t *uouth_o=&tx0130.trans_w[9];
    uint16_t *uoutl_w=&tx0130.trans_w[10];
    uint16_t *uoutl_o=&tx0130.trans_w[11];
    uint16_t *fac_nom=&tx0130.trans_w[12];
    uint16_t *fac_h=&tx0130.trans_w[13];
    uint16_t *fac_l=&tx0130.trans_w[14];
    uint16_t *iouth_w=&tx0130.trans_w[15];
    uint16_t *iouth_o=&tx0130.trans_w[16];
    uint16_t *pouth_w=&tx0130.trans_w[17];
    uint16_t *pouth_o=&tx0130.trans_w[18];
    uint16_t *poutnom=&tx0130.trans_w[19];
    uint16_t *ubattl_w=&tx0130.trans_w[20];
    uint16_t *ubatth_w=&tx0130.trans_w[21];
    uint16_t *ubatth_o=&tx0130.trans_w[22];
    uint16_t *speedh_w=&tx0130.trans_w[23];
    uint16_t *speedh_o=&tx0130.trans_w[24];
    uint16_t *d_uouth=&tx0130.trans_w[25];
    uint16_t *d_uoutl=&tx0130.trans_w[26];
    uint16_t *d_iph_w=&tx0130.trans_w[27];
    uint16_t *d_iph_o=&tx0130.trans_w[28];
    uint16_t *d_ubatth=&tx0130.trans_w[29];
    uint16_t *d_speedh=&tx0130.trans_w[30];
    uint16_t *rtkfull=&tx0130.trans_w[31];
    uint16_t *rtkempty=&tx0130.trans_w[32];
    uint16_t *genconfl=&tx0130.trans_w[33];
    uint16_t *genconfh=&tx0130.trans_w[34];
    uint16_t *sensconfl=&tx0130.trans_w[35];
    uint16_t *sensconfh=&tx0130.trans_w[36];
    uint16_t *progsn_l=&tx0130.trans_w[37];
    uint16_t *progsn_h=&tx0130.trans_w[38];
    
    uint8_t  *t_idle=&tx0130.trans_b[78];
    uint8_t  *max_crank=&tx0130.trans_b[79];
    uint8_t *coolin_w=&tx0130.trans_b[80];
    uint8_t *altb_w=&tx0130.trans_b[81];
    uint8_t *cylh_w=&tx0130.trans_b[82];
    uint8_t *exhm_w=&tx0130.trans_b[83];
    uint8_t *coil_w=&tx0130.trans_b[84];
    uint8_t *poil_w=&tx0130.trans_b[85];
    uint8_t *coolin_o=&tx0130.trans_b[86];
    uint8_t *altb_o=&tx0130.trans_b[87];
    uint8_t *cylh_o=&tx0130.trans_b[88];
    uint8_t *exhm_o=&tx0130.trans_b[89];
    uint8_t *coil_o=&tx0130.trans_b[90];
    uint8_t *poil_o=&tx0130.trans_b[91];
    uint8_t *coolout_w=&tx0130.trans_b[92];
    uint8_t *coolout_o=&tx0130.trans_b[93];
    uint8_t *d_temps=&tx0130.trans_b[95];
    uint8_t *d_poil=&tx0130.trans_b[96];
    uint8_t *toil_w=&tx0130.trans_b[97];
    uint8_t *toil_o=&tx0130.trans_b[98];
    uint8_t *tk_low=&tx0130.trans_b[99];
    uint8_t *d_tank=&tx0130.trans_b[99];
    uint8_t *cooldown_temp=&tx0130.trans_b[100];
    uint8_t *cooldown_time=&tx0130.trans_b[101];
    uint8_t *act_crank=&tx0130.trans_b[102];
    uint8_t *act_idle=&tx0130.trans_b[103];
    uint8_t *maxoil=&tx0130.trans_b[104];
    uint8_t *poles=&tx0130.trans_b[105];
    uint8_t *abs_exhm=&tx0130.trans_b[106];
    uint8_t *abs_cylh=&tx0130.trans_b[107];
    uint8_t *priming=&tx0130.trans_b[108];
    uint8_t *compb0=&tx0130.trans_b[109];
    uint8_t *fan_low=&tx0130.trans_b[110];
    uint8_t *fan_high=&tx0130.trans_b[111];
    uint16_t *progdate_l=&tx0130.trans_w[56];
    uint16_t *progdateh=&tx0130.trans_w[57];
    
static void portsInit(void);
static void adc_init(void);
static void tmr_init(void);
static void spi_init(void);
static void isrconfig(void);
static void chkfastlog(uint8_t row);
    
void cpu_setup(void){
    DDPCONbits.JTAGEN = false;
    portsInit();
    adc_init();
    tmr_init();
    spi_init();
    isrconfig();
}



void portsInit(void){
    TRISA = 0xC681;
    ODCAbits.ODCA6=true;
    TRISA = 0x0040; //turn on actuator 

    TRISB = 0xFFFF;
    TRISC = 0x901E;
    LATC = 0x6000;      //de-assert CS, DC DC converter on
    
    TRISD = 0x836C;
    LATD = 0x1000;
            
    TRISE=0x0201;        
    LATE=0;         
    
    TRISF = 0x002D;
    LATF = 0;
    
    TRISG = 0xC080;      //all outputs
    LATG=0x0200;        //leave actuator off
}


void adc_init(void){
    AD1PCFG=0xF0C0;     
    AD1CON1=0;			//forces normal 16 bit output,
    AD1CON1bits.SSRC=0b111;	//set to auto convert
    AD1CON1bits.CLRASAM=1;	//clear sampling automatically
    AD1CON1bits.ASAM=1;     //prepare for auto-sampling
    AD1CON1bits.SAMP=1; 

    AD1CON2=0;			//Set buffer mode to 16 word buffer, forces usage of always MUX A
    AD1CON2bits.VCFG=1;		//Reference is AVSS and positive Vref
    AD1CON2bits.CSCNA=1;	//scan inputs;
    AD1CON2bits.SMPI=7;     //interrupt after 8 samples; During initalization these are all shifted up by 1 bit to catch hardware ID, basically skipping MUX input. The that will be modified after initialization
    
    AD1CON3=0;              //ADC clock derived from peripheral bus
    AD1CON3bits.SAMC=6;     //Auto Sample timer is set to 6 TAD; with TAD=200ns sampling time will be 1.2 us

    AD1CON3bits.ADCS=3;		//TAD will be 200ns; One conversion=12TAD=2.4us
    //with a total of 1.2us sampling plus 2.4us conversion time a total of 3.6us is required per channel and 28.8 us for 8 channels. TMR 1 will trigger every 50us. RB11 will be removed from Scan pattern after initialization

    AD1CHS=0;			//select VR- as negative reference for MUX A; MUX B is not used anyway
    AD1CSSL=0x0F3C;     //skip MUX for now
    muxptr=0;
}

void tmr_init(void){    
//Distribution of timer and PWM  resources:
//Timer 1 to generate an ADC interrupt every 50us.
//Timer 2 for general time keeping tasks (runs t_1ms))
//Timer 3 is the time base for the actuator control (20ms)
//Timer 4 and 5 are concatenated to form a free-running timer for frequency and phase shift calculation
    T1CON=0;
    TMR1=0;
    T1CONbits.TCKPS=0b01;       //Prescaling factor is 8:1 (200ns out) no gating, peripheral clock source
    PR1=250;                    //will cause interrupt every 50us
    T1CONbits.ON=true;
    
    T2CON=0;
    TMR2=0;
    T2CONbits.TCKPS=3;          //will prescale 1:8, prescaler output is 200ns
    PR2=5000;                   //will cause 1ms overflow rate
    T2CONbits.ON=true;
    
    //prepare timer 3 for servo motor. 
    TMR3=0;
    T3CON=0;				//see above
    T3CONbits.TCKPS=0b100;  //prescale 1:16, prescaler output is 400nS
    PR3=50000;	  			//50000*0.4us=20ms=50Hz.
    T3CONbits.ON=true;

    TMR4=TMR5=0;
    T4CON=T5CON=0;
    T4CONbits.T32=true;
    T4CONbits.TCKPS=0b010;      //prescale 1:4, 100ns out of the prescaler. 
    PR4=0xFFFF;
    PR5=0xFFFF;
    T4CONbits.ON=true;          //overflow time is 7 minutes and 9.5sec.
    
//PWM 1 is for actuator control    
    OC1CON=0;
    OC1CONbits.OCTSEL=1;        //select timer 3. Timer will be set after loading stop solenoid configuration
    OC1CONbits.OCM=6;           //regular PWM
//Actuator PWM will be enabled while performing calibration
}

void spi_init(void){
    //SPI port for Memory will operate @10MHz    
    SPI4CON=0;                     
    SPI4CONbits.MSTEN=1;	//master mode for this port. Will switch between 8 and 16 bit mode as required
    SPI4CONbits.CKE=1;
    SPI4BRG=1;              
    SPI4CONbits.ON=1;       //may run continuously
    
     //SPI1, ADC access. Master mode, 32 bits
    SPI3CON=0;                  //will default to transmit interrupting when TSR is empty,
    SPI3CONbits.MSTEN=true;    
    SPI3BRG=5;                 //select 3.333MHz communication speed and 8bit mode. Will use 3 bytes per each transfer
    SPI3CONbits.CKE=true;
}

void isrconfig(void){
    
//external interrupts, group 4    
    INTCONbits.INT1EP=INTCONbits.INT2EP=true;       //trigger on rising edge for both, current and voltage
    IFS0bits.INT1IF=IFS0bits.INT2IF=false;
    IEC0bits.INT1IE=IEC0bits.INT2IE=true;
    IPC1bits.INT1IP=IPC2bits.INT2IP=4;              
    IPC1bits.IC1IS=2;
    IPC2bits.INT2IS=3;       //voltage first if coincidental
    
//SPI3 ADC, group 5
    //SPI 4, ADC communication, will use SRS
    IPC6bits.SPI3IP=5;  
    IPC6bits.SPI3IS=3;  //one below timer within class
    //interrupts will be enabled when initializing the external ADC
    
//CAN, group 6
    IPC11bits.CAN1IP=6;     //CAN port
    IPC11bits.CAN1IS=3;	
    //will be enabled after fully initializing the CAN port
    
//timer 1 and 2, group 3
    IFS0bits.T1IF=IFS0bits.T2IF=false;
    IEC0bits.T1IE=IEC0bits.T2IE=true;
    IPC1bits.T1IP=IPC2bits.T2IP=3;
    IPC1bits.T1IS=3;
    IPC2bits.T2IP=2;
    INTCONbits.MVEC=true;
    __builtin_enable_interrupts();
}


bool cpu_init(void){
    uint32_t tmrx=t_1ms;
    uint16_t rdbf[2];
    uint8_t i;
    bool retval=false;
    LATGCLR=0x0200;     //this will turn on the DC DC converter as well
    AD1CON1bits.ADON=true;
 
    //turn on the actuator and check feedback. 
    while(!retval || ((t_1ms-tmrx)>500)){
        while(adccntr<64);
        *ubatt=((uint32_t)adchan[3]*VREF*17/10)>>16;             //divider ratio is 17, factor1/10 for unit
        *actu=((uint32_t)adchan[3]*VREF*27/(270+470))>>16;       // Actuator voltage; include resistive divider and factor 1/10 because unit is 10mV
        adchan[7]=((uint32_t)adchan[7]*VREF)>>16;
        if(adchan[7]<15)
            hwtype=ISC006;
        retval=(*actu<700) && (*actu>5500) && (*ubatt>VBATTMIN) && (*ubatt<VBATTMAX);
        for(i=0;i!=8;i++)
            adchan[i]=0;
        if(!retval)
            adccntr=0;  //start another round
    }
    LATGSET=0x0200;     //turn the actuator off again. Will only require it when running the generator
    if(retval){
        AD1CON1bits.ADON=false;     //switch back to 
        AD1CSSL=0x073E;
        AD1CON1bits.ADON=true;
        readeeprom(0,tx0130.trans_w,TR130LEN-2);
        if(!checkmirr(tx0130.trans_w)){
            SETBIT(*stwordlow,2);
        }
        readeeprom(0x01A0,progdate_l,2);
        readeeprom(0x01E8,tx013A.trans,6);
        if(tx013A.v32.optime>9999998)
            SETBIT(*miscwordhigh,4);
            
        readeeprom(0x01A4,&tx000A.trans[2],2);
        if(((tx000A.val.snh & 0xFF)==0) || ((tx000A.val.snh & 0xFF)>12) || ((tx000A.val.snh>>8) <26))
            SETBIT(*stwordlow,2);
        readeeprom(0x01A8,tx0138.trans,32);
            
        if(TESTBIT(*miscwordlow,4)){
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
        
        readeeprom(0x0200,rdbf,2);
        alptr=rdbf[0];
        evptr=rdbf[1];
        if(((alptr & 0xFF)>MAXLOG) || ((alptr>>8)>1) || ((evptr & 0xFF)>MAXLOG) || ((evptr>>8)>1))
            reslogmem();   //alarm or event pointers out of range, erase all log data
        for(i=0;i!=40;i++){
            readeeprom(0x0B00+i*32,tx0104[i],16);       //will load last set of fast shutdown
            chkfastlog(i);                              //will clear it to zero if not within reasonable range
        }
        
        rq0100.transfer=0x0100;
        rq0005.transfer=5;
        rq000B.transfer=0x0B;
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
        sysstat=SYS_INIT;
        gcontstat=GENCONTR_STATE_INIT;
        cancomstat=CAN_STATE_INIT;
        stat_bits.onpending=true;
    }
    return(retval);
}

//will check one row at a time. If the test fails the entire row will be zeroed
void chkfastlog(uint8_t row){
    uint8_t i;
    bool ok=true;   //voltage L1
    for(i=0;i!=6;i++)
        if(tx0104[row][i]>6000)     //that is exceeding 600V or 600A
            ok=false;
    if(tx0104[row][6]>700)      //frequency
        ok=false;
    if(abs(tx0104[row][7])<50)  //power factor
        ok=false;
    if((tx0104[row][8]<700) || (tx0104[row][8]>3700))   //battery
        ok=false;
    if((tx0104[row][9] & 0xFF)>200) //oil pressure. Do not check for actuator, it may be disabled
        ok=false;
    if(tx0104[row][10]>5000)      //eng. speed
        ok=false;
    //skip ctuator checking
     if(!ok)
        for(i=0;i!=16;i++)
            tx0104[row][i]=0;
}
