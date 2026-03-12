#include "auxfunc.h"

static uint8_t readsr(void);
static void setWRENlatch(void);
static void lockunlock(bool lock);
static void writesr(uint8_t sreg);
static void swosc(unsigned char osc);
static bool actfailoff(uint8_t *calst);

void initextadc(void){
    uint8_t i,k;
    LATDSET=0x0010;     //bring CS high
    SPI3CONbits.ON=false;
    for(i=0;i!=2;i++){
        for(k=0;k!=6;k++){
            *acchptr[i][k]=(ACCHANNEL){0};
            acchptr[i][k]->tmrstart=((uint32_t)TMR5<<16) | (uint32_t)TMR4;      //start all AC channels with current timer content
        }
        for(k=0;k!=2;k++)
            *dcchptr[i][k]=(DCCHANNEL){0};
        
    }
    isoptrs=0x00FF;       //always start with row 0 for bot arrays
    spistep =SPI_MCP_START;
    spichannel =0;
    SPI3CONbits.ON=true;
    IFS0bits.SPI3EIF=IFS0bits.SPI3RXIF=false;
    IEC0bits.SPI3EIE=IEC0bits.SPI3RXIE=true;    //interrupt on overflow and buffer full
    LATDCLR=0x0010;
    SPI3BUF=1;          //this is the Init byte, ISR will take over after this  
}

//reads and returns the status register of the EEPROM
uint8_t readsr(void){
    uint8_t dm=0xFF;
    uint32_t tmrx=t_1ms;
    while(SPI1STATbits.SPIBUSY);
    SPI1STATbits.SPIROV=false;
    LATDCLR=0x1000;
    SPI1BUF=0x05;       //send command SPIREADSR. Second byte is to retrieve SRS
    while(SPI1STATbits.SPIBUSY);
    dm=SPI1BUF;
    SPI1BUF=dm;
    while(SPI1STATbits.SPIBUSY);
    dm=SPI1BUF;
    LATDSET=0x1000;
    return(dm);
}//readsr

void setWRENlatch(void){
    uint8_t dm;
    uint32_t tmrx=t_1ms;
    while((SPI1STATbits.SPIBUSY)&&((t_1ms-tmrx)<6));
    SPI1STATbits.SPIROV=false;
    LATDCLR=0x1000;
    SPI1BUF=0x06;       //send WREN command;
    while(SPI1STATbits.SPIBUSY);
    dm=SPI1BUF;
    LATDSET=0x1000;
}

//will read from memory, specified address, specified number of uint16_t. Will not read from uneven address
//or from an address outside memory range. Will return true if read went well. Will not block for more than 5ms
bool readeeprom(uint16_t adr,uint16_t *buf,uint16_t len){
    bool wip=true;
    uint8_t dm;
    uint16_t bf;
    uint32_t tmrx;
    
    tmrx=t_1ms;
    
    if(TESTBIT(adr,0)||(adr>(0xFFFE))||(!len))
        return(false);	//address violation or length is zero or longer than EEPROM
    else{
        LATDSET=0x1000;
        while(((t_1ms-tmrx)<2)&&(wip))
            wip=TESTBIT(readsr(),0);
        if(wip)
            return(false);//timeout while reading SR
        else{   //will require buffer mode to be set to 8 bit width to transmit read command
            LATDCLR=0x1000;
            SPI1BUF=0x03;   //send read command
            while(SPI1STATbits.SPIBUSY);
            dm=SPI1BUF;                 //dummy read
            SPI1BUF=adr>>8;
            while(!SPI1STATbits.SPIBUSY);
            dm=SPI1BUF;                //dummy read
            SPI1BUF=adr&0xFF;
            while(!SPI1STATbits.SPIBUSY);
            dm=SPI1BUF;                //dummy read
//now finally do the entire read loop     
            while(len--){
                SPI1BUF=dm;             //some dummy write to enforce reading
                while(!SPI1STATbits.SPIBUSY);
                bf=SPI1BUF<<8;
                SPI1BUF=dm;             //some dummy write to enforce reading
                while(!SPI1STATbits.SPIBUSY);
                bf|=SPI1BUF;
                *buf++=bf;
            }
            LATDSET=0x1000;
            return(true);
        }
    }
}

//will write data to EEPROM. Will return true if write went OK. Requires even addresses. Will not start write process if battery voltage is <10V
bool writeeeprom(uint16_t adr, uint16_t *buf, uint16_t len){
    uint32_t tmrx;
    uint8_t dm;
    uint16_t bf;
    bool inpage,wip=true;  
    if((adr>0xFFFE)||TESTBIT(adr,0)||(!len))
        return(false);
    else{
        LATDSET=0x1000;
        lockunlock(false);      //unlock chip
        while(len){
            tmrx=t_1ms;
            while(((t_1ms-tmrx)<5)&&(wip))
                wip=TESTBIT(readsr(),0);
            if(wip)
                return(false);//timeout while reading SR
            else{  
                wip=true;   //this is required for next cycle, if writing more than one page
                setWRENlatch();
                inpage=true;
                LATDCLR=0x1000;
                SPI1BUF=2;      //send write command
                while(!SPI1STATbits.SPIBUSY);
                dm=SPI1BUF;                //retrieve one dummy byte, then return to 16 bit width
                SPI1BUF=adr>>8;
                while(!SPI1STATbits.SPIBUSY);
                dm=SPI1BUF;                //retrieve one dummy byte
                SPI1BUF=adr&0xFF;
                while(!SPI1STATbits.SPIBUSY);
                dm=SPI1BUF;                //retrieve one dummy byte
                while(inpage&&len){
                    bf=*buf++;
                    len--;
                    adr+=2;
                    SPI1BUF=bf>>8;
                    while(!SPI1STATbits.SPIBUSY);
                    dm=SPI1BUF;
                    SPI1BUF=bf&0xFF;
                    while(!SPI1STATbits.SPIBUSY);
                    if(!(adr&(MEMPAGE-1)))
                        inpage=false;
                    while(!SPI1STATbits.SPIBUSY);
                    dm=SPI1BUF;
                }
                LATDSET=0x1000;
            }
        }     
    }
    lockunlock(true);   //lock eeprom
    return(true);
}    


//will lock or unlock eeprom.
void lockunlock(bool lock){
    uint8_t streg,bp01;
    if(lock)
        bp01=0x0C;
    else
        bp01=0;
    streg=readsr();
    while((streg&0x0C)!=bp01){
        while(TESTBIT(streg,0))
            streg=readsr();
        setWRENlatch();
        writesr(bp01);
        streg=readsr();  
    }
}

void writesr(uint8_t sreg){
    uint8_t dm;
    uint32_t tmrx;
    tmrx=t_1ms;
    while((SPI1STATbits.SPIBUSY)&&((t_1ms-tmrx)<3));
    SPI1STATbits.SPIROV=false;
    LATDCLR=0x1000;
    SPI1BUF=0x01;       //send command write sr
    while((!SPI1STATbits.SPIRBF)&&((t_1ms-tmrx)<3));
    dm=SPI1BUF; //dummy read
    SPI1BUF=sreg;
    while((!SPI1STATbits.SPIRBF)&&((t_1ms-tmrx)<3));
    dm=SPI1BUF; //dummy read
    LATDSET=0x1000; //cs high 
}//writesr

//this function will only perform a very rudimentary limit check for general plausibility. 16bitvalues must not exceed 65000 and 8bit values must not exceed 252
//it will further check logical implications
bool checkmirr(uint16_t *bf){
    uint8_t i;
    bool retval=true;
    uint16_t re,rf,gfl;
    for(i=0;i!=31;i++)
        retval &= (*bf++ <65000);       //everything up to tank res. 
    rf=*bf++;       //rtkfull
    re=*bf++;       //rtkempty
    if((rf>6500) || (re>6500) ||(rf==re))
        retval=false;
    if(retval){     //determine limits for tank sensor
    gfl=(rf < re) ? rf : re;
        if(gfl<10)
            rtkmin=6;  //6 Ohm
        else
            rtkmin=gfl>>1;  //or half of lower resistance
        gfl=(rf > re) ? rf : re;
        rtkmax=gfl+gfl>>2;
    }
    gfl=*bf++;   //genconfl
    if(TESTBIT(gfl,15) && ((gfl&3)!=2))
        retval=false;
    bf+=5;      //skip genconfh, sensor config and SN

    for(i=0;i!=17;i++){
        retval &=(*bf>>8)<251;
        retval &=(*bf++ & 0xFF)<251;
    }
    return(retval);
}

/*will check an ASCII string of max. len characters. will do this: 
 * Check for valid ASCII characters 90x20 to 0x79, boundaries included
 * Trim the string down from beginning to either first invalid character or to  len-1, whichever is shorter, and add append a trailing 0. Thus len is the length of the entire string, including terminating 0 
 * return false if the string is shorter than 3 characters or if it starts with a blank,
 * A string that does not meet check criteria will be replaced with "invalid"
 */
bool checkasc(uint8_t *str,uint8_t len){
    const uint8_t inval[]="invalid";
    uint8_t *ch,newln=0;
    bool cont=true;
    if(len<4)
        len=4;
    ch=str;
    while(cont){
        cont&=(*str <0x80) && (*str>0x1F);
        if(++newln>(len-1)){
            *str=0;
            cont=false;
        }
        else
            str++;
    }
    if((newln<4) || (*ch==' ')){
        str=ch;
        ch=(uint8_t *)inval;
        while(*str++ = *ch++);
        return(false);
    }
    else
        return(true);
}

void validatesvns(void){
    union{
        uint8_t bf8[128];
        uint16_t b16[64];
        uint32_t b32[32];   
    }bf;  
    uint32_t svx;
    uint8_t i; 
    bool pass;
    svx=*progsn_l | (*progsn_h<<16);
    if((rq01B0.status==0) && (rq01B1.status==0) && (rq01B2.status==0) && 
        (rq01B3.status==0) && (rq01B4.status==0) && (rq01B5.status==0)){ //must not validate SVN if still downloading data from panel
        for(i=0;i!=32;i++){
            CLEARBIT(valsn,i);
            if(TESTBIT(svx,i)){     //only check enabled SN
                if(i<16)
                    pass=(tx01B0.b32[2*i]>0)&& (tx01B0.b32[2*i]<0xFFFFFFFF);
                else
                    pass=(tx01B1.b32[2*i]>0)&& (tx01B1.b32[2*i]<0xFFFFFFFF);
                if(pass){
                    readeeprom(0x0300+i*0x40,bf.b16,0x40); 
                    if(checkasc(&bf.bf8[32],32)){
                        SETBIT(valsn,i);
                        if(bf.b16[0]<6)
                            SETBIT(perssn,i);
                        else
                            CLEARBIT(perssn,i);  
                    }
                } 
            }
        }
    }   
}

void inlowpwr(void){
uint32_t tmrx=t_1ms; 
    while((t_1ms-tmrx)>25) //Allow a CAN broadcast for power down request
//        CANCOM_Tasks();
    prepsoftres();  
//    initcan();
 //make sure device is ready for idle (not sleep)
    asm("di");
    SYSKEY=0;
    SYSKEY=0xAA996655;
    SYSKEY=0x556699AA;
    OSCCONCLR=0x0010;
    SYSKEY=0x33333333;
    asm("ei");    
    
    //power down the CAN bus:
    if(C1CONbits.ON){
        C1CONbits.REQOP=0b001;
        while(C1CONbits.OPMOD!=0b001);
    }
              
    while((t_1ms-tmrx)<2000);
    T1CONbits.ON=T2CONbits.ON=T3CONbits.ON=T4CONbits.ON=false;

    C1CONbits.SIDL=1;
    C1CFGbits.WAKFIL=1;
    C1INTbits.WAKIF=0;
    C1INTbits.WAKIE=1;
        
    swosc(0b101);       //switch to LPRC
       asm volatile("wait");
    cpureset();
}

void cpureset(void){
    uint32_t rswrst;
    asm("di");
    SYSKEY=0;
    SYSKEY=0xAA996655;
    SYSKEY=0x556699AA;
    RSWRSTbits.SWRST=true;
    rswrst=RSWRST;
    asm("nop");
    asm("nop");
}

//this function will switch the oscillator to run off SOSC if sosc=0b100,
//or off POSC if osc==0b011 or off LPRC if osc==0b101
//the function will not check the argument!
void swosc(unsigned char osc){
    asm("di");
    SYSKEY=0;
    SYSKEY=0xAA996655;
    SYSKEY=0x556699AA;
    OSCCONbits.NOSC=osc;
    OSCCONSET=1;
    SYSKEY=0x33333333;
    while(OSCCONbits.OSWEN);
    asm("ei");
}
//place controller in initial status, in preparation for SoftReset())
//will hold the USB port in reset for 2 seconds first. This is to enforce windows to re-enumerate
//will then place the port 
void prepsoftres(void){
    uint32_t tmrx=t_1ms;
    
    if(C1CONbits.ON){
        C1CONbits.REQOP=0b001;  //disable CAN bus
        while(C1CONbits.OPMOD!=0b001);
        C1CONCLR=0x8000;
    }
    SPI2CONbits.ON=SPI1CONbits.ON=false;
    AD1CON1bits.ON=0;
    OC1CON=0;    
    LATBCLR=0x800C0;
    LATCCLR=0x6000;
    LATDCLR=0x27;
    LATDSET=0x10;
    LATE=0;
    LATFSET=8;
    while((t_1ms-tmrx)<20);
}


void reslogmem(void){
    uint16_t i,bf[128];
    lgreq=alptr=evptr=0;
    for(i=0;i!=128;i++)
        bf[i]=0xFFFF;
    for(i=0;i!=MAXLOG<<1;i++)
        writeeeprom(0x1000+0x80*i,bf,64);
    bf[0]=bf[1]=0;
    writeeeprom(0x0200,bf,2);   //reset log pointers
}


//will retrieve 4 bytes from a string and place them in one 64bit word in little endian format. Will advance original pointer by 4
uint32_t get4bytes( uint8_t **strg){
    uint32_t retval=*((*strg)++);
    retval|=(*(*strg)++)<<8;
    retval|=(*(*strg)++)<<16;
    retval|=(*(*strg)++)<<24;
    return(retval);
}

void logstasto(uint8_t reason, uint8_t src){
    uint16_t bfx[3];
    bfx[0]=caldate.trans & 0xFFFF;
    bfx[1]=caldate.trans>>16;
    bfx[2]=reason | (src<<8);
    writeeeprom(0xB64A+(stastoptr & 0x1F)*6,bfx,3);
    stastoptr++;
    if((stastoptr & 0xFF)>0x1F)
        stastoptr=0x0100;
    writeeeprom(0x020E,&stastoptr,1);
}

//will check if cdt is a valid trans field of an rtcx structure. Will return true if so. WIll also check for 
bool chkvalrtc(RTCX *cdt){
    bool retval=true;          
    if((cdt->val.year>15)&&(cdt->val.year<99)&&(cdt->val.month>0)&&(cdt->val.month<13)&&(cdt->val.day_of_m>0)
            &&(cdt->val.hours<24)&&(cdt->val.minutes<60)&&(cdt->val.sec2<30)){
//check for erratic day of month/month combination
        if(cdt->val.month==2){   //february
            if(!(cdt->val.year%4)){//leap year
                if(cdt->val.day_of_m>29)
                    retval=false;
            }
            else if(cdt->val.day_of_m>28)
                retval=false;
        }
        else if((cdt->val.month==4)||(cdt->val.month==6)||(cdt->val.month==9)||(cdt->val.month==11)){
            if(cdt->val.day_of_m>30)
                retval=false;
        }
        else if(cdt->val.day_of_m>31)
            retval=false;
    }
    else
        retval=false;
    return(retval);
}

 //this will reset a service notification. will always use regular calendar time and operating time. Will never attempt to reset an invalid SVN. 
void ressrvnot(uint32_t notno){
    uint8_t i;
    uint16_t rvx[4],*snk;
    uint32_t newop,newcal;
    if(TESTBIT(valsn,notno)){
        if(notno<16)
            CLEARBIT(*pendsnl,notno);
        else 
            CLEARBIT(*pendsnh,notno-16);
        readeeprom(0x0300+notno*0x40,rvx,2); //read time interval to calculate next service request
//bit 15 of rvx[0] will indicate if this is a one-time message. If so the message will be invalidated, re-trigger date will be set to 0xFFFFFFFF        
        if(TESTBIT(rvx[0],15)){
            newop=newcal=0xFFFFFFFF;
//invalidate entire SVN by setting initial trigger and re-trigger to 32766. This number however will indicate this was a one-time SVN
            rvx[0]=rvx[1]=rvx[2]=rvx[3]=0x7FFE;
            while(!writeeeprom(0x0300+notno*0x40,rvx,4));
        }
        else{
            newop=(tx013A.v32.optime)+(uint32_t)(rvx[0]&0x7FFF)*60;  //new op. time in minutes
            newcal=newcaldat(rvx[1],caldate.trans);
        }
        rvx[0]=newop;
        rvx[1]=newop>>16;
        rvx[2]=newcal;
        rvx[3]=newcal>>16;
        //write next trigger time back
        
        while(!writeeeprom(0x0308+notno*0x40,rvx,4));   
//also update buffers 01B0 or 01B1 and flag them to be sent to update the panel
        
        if(notno<16){
            tx01B0.b32[2*notno]=newop;
            tx01B0.b32[2*notno+1]=newcal;
            x01B0.status=1;
        }
        else{
            tx01B1.b32[2*(notno-16)]=newop;
            tx01B1.b32[2*(notno-16)+1]=newcal;
            x01B1.status=1;
        }
//make "most recent reset" become the "previous to last" reset            
        readeeprom(0x0318+notno*0x40,rvx,4);   //read previous "most recent" from EEPROM 
        writeeeprom(0x0310+notno*0x40,rvx,4);   //write it back to prior to last

        if(notno<16){
            tx01B2.b32[2*notno]=rvx[0]|(rvx[1]<<16);
            tx01B2.b32[2*notno+1]=rvx[2]|(rvx[3]<<16);   //and send it to the panel
            x01B2.status=1;  
        }
        else{
            tx01B3.b32[2*(notno-16)]=rvx[0]|(rvx[1]<<16);
            tx01B3.b32[2*(notno-16)+1]=rvx[2]|(rvx[3]<<16);   //and send it to the panel
            x01B3.status=1;  
        }

        //now store the most recent reset
        rvx[0]=tx013A.val.runtl;
        rvx[1]=tx013A.val.runth;
        rvx[2]=caldate.trans&0xFFFF;
        rvx[3]=caldate.trans>>16;
        writeeeprom(0x0318+notno*0x40,rvx,4);   //log current date and op time as "last service reset
        if(notno<16){
            tx01B4.b32[2*notno]=rvx[0]|(rvx[1]<<16);
            tx01B4.b32[2*notno+1]=rvx[2]|(rvx[3]<<16);   //and send it to the panel
            x01B4.status=1;
        }
        else{
            tx01B4.b32[2*(notno-16)]=rvx[0]|(rvx[1]<<16);
            tx01B4.b32[2*(notno-16)+1]=rvx[2]|(rvx[3]<<16);   //and send it to the panel
            x01B5.status=1;
        }  
    }
}

//will initialize new service notifications after the warranty did start. The function will be called just once, when the warrant starts. 
void initsvn(void){
    uint8_t i,k;
    union{
        uint8_t bf8[128];
        uint16_t b16[64];
        uint32_t b32[32];   
    }bf;
    bool pass=true;
    for(k=0;k!=32;k++){
        readeeprom(0x0300+k*0x40, bf.b16,0x40); 
        for(i=0;i!=4;i++)
            pass &= ((bf.b16[i]&0x3FFF)<0x4000) && ((bf.b16[i]&0x3FFF)>0); //settings range for fields 1..4 is 1 to 16383; bit 15 shall be ignored
        pass &= checkasc(&bf.bf8[32],32);   //check for valid ASCII text
        if(pass){    
            //recalculate next events based on current op time and current date, using initial trigger intervals
            bf.b32[2]=tx013A.v32.optime+(uint32_t)bf.b16[2]*60;       //next trigger time, based on initial interval
            bf.b32[3]=newcaldat(bf.b16[3],tx01B6.val.shipdate_l | (tx01B6.val.shipdate_h <<16));
            bf.b32[4]=bf.b32[5]=bf.b32[6]=bf.b32[7]=0xFFFFFFFF; //invalidate history
            writeeeprom(0x0300+k*0x40, bf.b16,0x20);       // no need to write back the text
            SETBIT(valsn,k);
            if(k<16){
                tx01B0.b32[2*k]=bf.b32[2];     //prepare for double-buffering next trigger in the panel
                tx01B0.b32[2*k+1]=bf.b32[3];
                x01B0.status=1;
            }
            else{
                tx01B1.b32[2*(k-16)]=bf.b32[2];     //prepare for double-buffering next trigger in the panel
                tx01B1.b32[2*(k-16)+1]=bf.b32[3];
                x01B1.status=1;
            }
        }
        else{   //invalidate this message
            bf.b16[0]=bf.b16[1]=0xFFFF;
            writeeeprom(0x0300+k*0x40, bf.b16,2);
            CLEARBIT(valsn,k);
        }
    }
}

//will recalculate calendar date based on todays date plus days
uint32_t newcaldat(uint16_t days,uint32_t refdat){
    const uint8_t days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t daysleft,year,month,day;
    uint16_t cnt=days;
    bool isleap;
    RTCX datex;
    datex.trans=refdat;
    year=datex.val.year;      //starting from here, current date
    month=datex.val.month;
    day=datex.val.day_of_m;

    while(cnt){        //will iterate this loop for each day
        isleap=( (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0) );
            if(month==2){
                if(isleap)
                    daysleft=29;
                else
                    daysleft=28;
            }
            else daysleft=days_in_month[month-1];
        cnt--;
        day++;
        if(day>daysleft){//exceeded this month, go to first of next month
            month++;
            day=1;
            if(month>12){ //exceeded year
                year++;
                month=1;
            }
        }
    }
    return((year<<25) | (month<<21) | (day<<16));
}

void writecal(uint16_t adr){
    uint16_t bf[2];
    bf[0]=caldate.trans&0xFFFF;
    bf[1]=caldate.trans>>16;
    writeeeprom(adr,bf,2);
}

//actuator calibration will not support previous Hitec actuator, only BLDC servo. 
bool actcalib(bool init){
    static uint8_t calstate,phase1,imaxcntr;
    static uint32_t tmx,iminsum;
    bool retval=false;     
    if((init)||(calstate>3)){
        calstate=0;
        phase1=true;
        LATGSET=0x0200;     //turn actuator off
        OC1CONbits.ON=false;
        imaxcntr=0;
    }
    else switch(calstate){
        case 0: //initialize calibration process
            LATGCLR=0x0200;     //turn on actuator supply
            OC1CONbits.ON=1;
            OC1RS=STEPVALMAX+(STEPVALMIN-STEPVALMAX)/2;     //center position;
            tmx=t_1ms; 
            calstate=1;
            break;

        case 1: //actuator traveling to center position, allow 1.5 s to stabilize
            if((t_1ms-tmx)>1500){ 
                tmx=t_1ms;
                imaxcntr=iminsum=0;
                calstate=2;
            }
    //actuator is not supposed to hit the limits while traveling to the middle. If it does, it is misaligned.           
    //also, the actuator voltage needs to be within shutdown limits           
            if((*codeAlm&0xB000)!=0)//bits 12, 13 and 15 need to be clear
                retval=actfailoff(&calstate);                   //will stop calibration process
            break;

            case 2: //actuator traveling towards limits with one decrement per 4ms
                if(((t_1ms-tmx)>3)&&(stat_bits.gotacti)){
                    stat_bits.gotacti=false;
                    tmx=t_1ms;
                    if(*acti<(ACTIMAXC-20)){
                        if(phase1){ //traveling towards low throttle
                            if(OC1RS<=(STEPVALMIN-STEPMOVE))
                                OC1RS+=STEPMOVE;
                        }
                        else{
                            if(OC1RS>=(STEPVALMAX+STEPMOVE))
                                OC1RS-=STEPMOVE;
                        }
                        imaxcntr=0;
                        if(*acti<ACTIMINC)
                            iminsum++;
                    }
                    else{
                        imaxcntr++;    
                        iminsum=0;
                    }
                }
                if((*codeAlm&0xB000)!=0){//again test for bits 12, 13 and 15 in codeAlm, but also for time-out    
                    SETBIT(*mwordlow,12);
                    retval=actfailoff(&calstate);
                }
                else if((phase1 && (OC1RS>=STEPVALMIN)) || (!phase1 && (OC1RS<=STEPVALMAX))){       //if the actuator goes to the theoretical limits then the BLDC does not draw excessive current
                    if(phase1){
                        actmin=OC1RS-ACTDELTA;
                        calstate=1;
                        phase1=false;
                        tmx=t_1ms;
                        OC1RS=STEPVALMAX+(STEPVALMIN-STEPVALMAX)/2;
                    }
                    else{
                        actmax=OC1RS+ACTDELTA;
                        calstate=3;
                    }
                }
                else if(imaxcntr>20){
                    if(phase1){
                        actmin=OC1RS-ACTDELTA;
                        calstate=1;
                        phase1=false;
                        tmx=t_1ms;
                        OC1RS=STEPVALMAX+(STEPVALMIN-STEPVALMAX)/2;;
                    }
                    else{
                        actmax=OC1RS+ACTDELTA;
                        calstate=3;
                    }
                }
                break;    
            case 3:     //calibration is over, check results
                if((actmax>=STEPVALMAX)&&(actmin<=STEPVALMIN) && (actmin>actmax) && ((actmin-actmax)>=MINSTEPRANGE)){
                    if(iminsum>10)
                        retval=false;
                    else{
                        retval=true;    //calibration is ready  
                        CLEARBIT(*mwordlow,12);
                    }
                }
                phase1=true;
                calstate=0;
                LATGSET=0x0200; //supply off
                OC1CONbits.ON=false;
                if(!retval){//this cycle failed, report it as such
                    retval=true;
                    SETBIT(*mwordlow,12);
                }
                break;

            default:        //this case just for error handling. Should never execute
                phase1=true;
                calstate=0;
                OC1CONbits.ON=false;
                LATGSET=0x0200;
                break;
        }
    return(retval);
}//actcalib

//this function just to turn off the actuator and report failed calibration. Needs to return true
bool actfailoff(uint8_t *calst){
    LATGSET=0x0200;                                 //Turn off the actuator
    OC1CONbits.ON=false;
    SETBIT(*mwordlow,12);                 //and mark actuator as not calibrated
    *calst=0;           
    return(true);
}