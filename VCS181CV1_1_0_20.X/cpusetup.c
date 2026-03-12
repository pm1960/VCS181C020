#include "cpusetup.h"

volatile uint32_t t_1ms=0;
volatile uint32_t v_period=0; 
volatile uint32_t i_period=0;
volatile uint32_t isr_flags=0;
volatile uint16_t nocan=0;
volatile uint16_t adchan[14]={0};
volatile uint16_t isoresptr=0xFF;
volatile uint8_t adccntr=0;
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
    uint16_t *kpi=&tx0130.trans_w[5];   //use as integral as well
    uint16_t *kd=&tx0130.trans_w[6];    
    uint16_t *kdi=&tx0130.trans_w[7];       //use as max. integral as well
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
    uint8_t *threshlow=&tx0130.trans_b[112];
    uint8_t *cranklow=&tx0130.trans_b[113];
    uint8_t *idlelow=&tx0130.trans_b[114];
    uint8_t *d_poillow=&tx0130.trans_b[115];
    uint16_t *progdate_l=&tx0130.trans_w[58];
    uint16_t *progdateh=&tx0130.trans_w[59];


    
static void portsInit(void);
static void adc_init(void);
static void tmr_init(void);
static void spi_init(void);
static void isrconfig(void);
static void chkfastlog(uint8_t row);
static void initcap(void);
    
void cpu_setup(void){
    DDPCONbits.JTAGEN = false;
    portsInit();
    adc_init();
    tmr_init();
    initcap();
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
    //ADC: WIll use all 14 ports, external reference
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
}

void tmr_init(void){    
//Distribution of timer and PWM  resources:
//Timer 1 to generate an ADC interrupt every 100us.
//Timer 2 for actuator PWM
//Timer 3 time base for capture. No interrupt, free running
//Timer 4 general timekeeping, increments every 1ms
    
    T1CON=0;
    TMR1=0;
    T1CONbits.TCKPS=0b01;       //Prescaling factor is 8:1 (200ns out) no gating, peripheral clock source
    PR1=250;                    //will cause interrupt every 50us
    T1CONbits.ON=true;
    
    T4CON=0;
    T4CONbits.TCKPS=3;          //will prescale 1:8, prescaler output is 200ns
    PR4=5000;                   //will cause 1ms overflow rate
    T4CONbits.ON=1;
    
//timer 2 is used for PWM. will not need to interrupt.
//version 1.1.14 modified to drive stop solenoid as linear actuator. If configured for actuator then that will be fixed after loading conf. settings
    T2CON=0;			//see above
    T2CONbits.TCKPS=6;  //prescale 1:64, prescaler output is 1.6us
    PR2=12500;	  		//12500*1.6us=20ms=50 Hz
    T2CONbits.ON=1;
    
    //timer 3 is used for capture input. Will increment at with 1.6us, overflow time is about 9.5 Hz
    T3CON=0;			//see above
    T3CONbits.TCKPS=6;  //prescale 1:64, prescaler output is 1.6us
    PR3=65535;	  		//continuous overflow is required
    T3CONbits.ON=1;
    
//PWM 5 is for actuator control    
    OC5CON=0;
    OC5CONbits.OCTSEL=1;        //select timer 3. Timer will be set after loading stop solenoid configuration
    OC5CONbits.OCM=6;           //regular PWM
//Actuator PWM will be enabled while performing calibration
}

void spi_init(void){
    //SPI1 for Memory will operate @10MHz    
    SPI1CON=0;                     
    SPI1CONbits.MSTEN=1;	//master mode for this port. 
    SPI1CONbits.CKE=1;
    SPI1BRG=3;              //run at 5MHz. 
    SPI1CONbits.ON=1;       //may run continuously
    
     //SPI4, external ADC access. Master mode
    SPI4CON=0;                  //will default to transmit interrupting when TSR is empty,
    SPI4CONbits.MSTEN=true;    
    SPI4BRG=5;                 //select 3.333MHz communication speed and 8bit mode. Will use 3 bytes per each transfer
    SPI4CONbits.CKE=true;
    
    //SPI2 for VCS temperature and oil temperature, slow running
    SPI2CON=0;
    SPI4CONbits.MSTEN=true;
    SPI2CONbits.CKE=true;
    SPI2BRG=39;                 //run at 500kHz
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
}

void isrconfig(void){
//Timer interrupts: Timer 1 and timer 3. Both priority 3, with timer 1 higher secondary priority
    IFS0bits.T1IF=IFS0bits.T4IF=false;
    IEC0bits.T1IE=IEC0bits.T4IE=true;
    IPC1bits.T1IP=IPC4bits.T4IP=3;
    IPC1bits.T1IS=3;
    IPC4bits.T4IP=2;
//SPI2: Priority 2
    IPC7bits.SPI2IP=2;
    IPC7bits.SPI2IS=3;
    IEC1bits.SPI2RXIE=IEC1bits.SPI2EIE=true;
    IFS1bits.SPI2RXIF=IFS1bits.SPI2EIF=false;
    
//SPI4: Priority 5
    IPC8bits.SPI4IP=5;  //will use shadow register
    IPC8bits.SPI4IS=3;
    IEC1bits.SPI4RXIE=IEC1bits.SPI4EIE=true;
    IFS1bits.SPI4RXIF=IFS1bits.SPI4EIF=false;
    
//Input captures: Priority 7
    IPC1bits.IC1IP=IPC2bits.IC2IP=7;
    IPC1bits.IC1IS=3;
    IPC2bits.IC2IS=2;
    IEC0bits.IC1IE=true;
    IEC0bits.IC2IE=true;
    IFS0bits.IC1IF=IFS0bits.IC2IF=false;
    
//CAN, group 6
    IPC11bits.CAN1IP=6;     //CAN port
    IPC11bits.CAN1IS=3;	
    //will be enabled after fully initializing the CAN port

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
