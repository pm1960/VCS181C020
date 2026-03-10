//auxiliary functions used throughout all applications

#include <xc.h>
#include "auxfunc.h"
  

static uint8_t readsr(void);
static void setWRENlatch(void);
static void swosc(unsigned char osc);
static void erasmem(bool almem);
static uint32_t newcaldat(uint16_t days);
static inline uint16_t rwspi16(uint16_t sendx);

//will check whether the VCS has valid service notifications in memory. If so then bits in valsvn will be set. In order to be valid next operating time 
//and next cal. trigger time must not be 0xFFFFFFFF
void checksvn(void){
    uint8_t i;
    uint16_t rdx[4];
    valsvn=0;
    for(i=0;i!=32;i++){
        if(readeeprom(0x0228+i*0x40,rdx,4)){
            if(((rdx[0]|(rdx[1]<<16))==0xFFFFFFFF)||
               ((rdx[2]|(rdx[3]<<16))==0xFFFFFFFF))
                CLEARBIT(valsvn,i);
            else
                SETBIT(valsvn,i);
        }
    }
}

void clearenergy(void){
    tx013A.v32.energy=0;
    writeeeprom(0x01D4,&tx013A.val.totalenl,2);
}

//will retrieve 4 bytes from a string and place them in one 64bit word in little endian format
uint32_t get4bytes( uint8_t *strg){
    uint32_t retval=*strg++;
    retval|=*strg++<<8;
    retval|=*strg++<<16;
    retval|=*strg<<24;
    return(retval);
}

//will read from memory, specified address, specified number of uint16_t. Will not read from uneven address
//or from an address outside memory range. Will return true if read went well. Will not block for more than 5ms
bool readeeprom(uint16_t adr,uint16_t *buf,uint16_t len){
    bool wip=true;
    uint16_t bf;
    uint32_t tmrx=t_1ms;
    if(TESTBIT(adr,0)||(adr>(MEMSIZE*2-2))||(!len)||((adr/2+len)>MEMSIZE))
        return(false);	//address violation or length is zero or longer than EEPROM
    else{
        LATDSET=0x1000; //pull CS high
        while(((t_1ms-tmrx)<5)&&(wip))
            wip=TESTBIT(readsr(),0);
        if(wip)
            return(false);//timeout while reading SR
        else{   
            LATDCLR=0x1000;
            SPI1BUF=0x03;   //send read command
            while(SPI1STATbits.SPIBUSY);
            bf=SPI1BUF;                 //dummy read
            bf=rwspi16(adr);
//now finally do the entire read loop     
            while(len--){
                bf=rwspi16(bf);
                *buf++=bf;
            }
            LATDSET=0x1000;
            return(true);
        }
    }
}

//will write data to EEPROM. Will return true if write went OK. Requires even addresses
bool writeeeprom(uint16_t adr, uint16_t *buf, uint16_t len){
    uint32_t tmrx;
    uint16_t bf;
    bool inpage,wip=true;  
    LATDSET=0x1000;     //pull CS high
    if((!len)||(TESTBIT(adr,0))||(adr>(MEMSIZE*2-2))||((adr/2+len)>MEMSIZE))
        return(false);
    else{
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
                while(SPI1STATbits.SPIBUSY);
                bf=SPI1BUF;                //retrieve one dummy byte, then return to 16 bit width
                bf=rwspi16(adr);
                while(inpage&&len){
                    bf=*buf++;
                    bf=rwspi16(bf);
                    len--;
                    adr+=2;
                    if(!(adr&(MEMPAGE-1)))
                        inpage=false;
                }
                LATDSET=0x1000;
            }
        }     
    }
    return(true);
}       

//reads and returns the status register of the EEPROM
uint8_t readsr(void){
    uint8_t dm=0xFF;
    uint32_t tmrx=t_1ms;
    while((SPI1STATbits.SPIBUSY)&&((t_1ms-tmrx)<6));
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

//this function will read and write a 16bit word across the SPI1. It will NOT handle CS!. Data will be sent in big endian, as the SPI port would do if it was configured for >8bit
static inline uint16_t rwspi16(uint16_t sendx){
    uint16_t retval;
    SPI1BUF=sendx>>8;
    while(SPI1STATbits.SPIBUSY);
    retval=SPI1BUF<<8;
    SPI1BUF=sendx&0xFF;
    while(SPI1STATbits.SPIBUSY);
    retval |=SPI1BUF;
    return(retval);
}

//will write below  string into *chx. Will fill remaining space with blanks. String will not be zero-terminated
void serinval (uint8_t *chx){
    uint8_t i,cx[]="Generator serial No. is invalid";
    for(i=0;i!=31;i++)
        *chx++=cx[i];
    *chx=0;
}

//place controller in initial status, in preparation for SoftReset())
 void prepsoftres(void){
    uint32_t tmrx=t_1ms;
    if(C1CONbits.ON){
        C1CONbits.REQOP=0b001;  //disable CAN bus
        while(C1CONbits.OPMOD!=0b001);
        C1CONCLR=0x8000;
    }
    OC5CONbits.ON=0;
    U4MODEbits.ON=0;
    U3MODEbits.ON=0;
    SPI1CONbits.ON=0;
    SPI4CONbits.ON=0;
    SPI2CONbits.ON=0;
    IC1CONbits.ON=0;
    IC2CONbits.ON=0;
    AD1CON1bits.ON=0;
    LATBCLR=0xFFFF;
    LATACLR=0xFFFF; //all ports down
    LATCCLR=0xFFFF; //all ports down
    LATCSET=0x2000; //power down the DC/DC-converter
    LATFCLR=0xFFFF; //port down
    LATDCLR=0xFFFF; //port down
    LATECLR=0xFFFF; //Port down
    LATGCLR=0xFFFF; //port down
    
    OC5CONbits.ON=0;
    while((t_1ms-tmrx)<200);
}

void inlowpower(bool forever){//prepare for deep low power
    uint8_t ldtp;
    uint16_t pdtx,wbuf[4];
    uint32_t tmrx=t_1ms;
    if(stfeedb.fields.pwrdwn)
        forever=true;
    wbuf[0]=tx013A.val.totalenl;
    wbuf[1]=tx013A.val.totalenh;
    wbuf[2]=enrem;
    wbuf[3]=enrem>>16;
    writeeeprom(0x01D4,wbuf,4);
    
    if(!forever){//power down time is fixed 25ms; will turn off actuator and LEDS during that time (cranking issue))
        ldtp=ledstat;
        if(PORTAbits.RA6)
        ledstat=0;
        LATASET=0x40;
        LATGCLR=0x3000;
        LATCCLR=0x2000;
        while((t_1ms-tmrx)<25);
        ledstat=ldtp;
        LATACLR=0x40;
        LATCSET=0x2000;
    }
    else{  
        RCONbits.POR=RCONbits.BOR=false;
        ledstat=ALLOFF;
        while((t_1ms-tmrx)<500);//    Allow a CAN boradcast for power down request
        prepsoftres(); 
                
        asm("di");
        initcan();                    
        CNCONbits.ON=1;
        CNCONbits.SIDL=1;       //allow interrupt in idle mode
        CNENbits.CNEN16=1;      //this is for USB port
        CNENbits.CNEN15=1;      //this for the remote power up
        
        CNPUE=0;
        pdtx=PORTD;        //clear any mismatch on Port D7
        IFS1bits.CNIF=0;
        IEC1bits.CNIE=1;  

 //make sure device is ready for idle (not sleep)
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

        tmrx=t_1ms;
        while((t_1ms-tmrx)<5000);
        T2CONbits.ON=T3CONbits.ON=T4CONbits.ON=0;        
        
        C1CONbits.SIDL=1;
        C1CFGbits.WAKFIL=1;
        C1INTbits.WAKIF=0;
        C1INTbits.WAKIE=1;
        
        swosc(0b101);       //switch to LPRC
        TRISFSET=0x0008;    //turning this into input will let the pin float high, the buck converter will power down and enable the LDO
        asm volatile("wait");
        cpureset();
    }
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


//will append variable src in little endian format to byte string snk
void add4bytes(volatile uint32_t src, volatile uint8_t *snk){
    uint8_t i;
    for(i=0;i!=4;i++){
        *snk++=src;
        src>>=8;
    }
}

void wrincident(uint16_t adr, uint16_t nr){
    uint8_t i;
    uint16_t dx;
    dx=nr;
    if(writeeeprom(adr,&dx,1))
        i=3;
    else 
        i=4;
}

//will write calendar date as in caldate to specified location
void writecal(uint16_t adr){
    uint16_t wb[2];
    wb[0]=caldate;
    wb[1]=caldate>>16;
    writeeeprom(adr,wb,2);
    
}

//will overwrite  a total of 126x80 bytes with all 0xFFFF. Used to clear log memory
//argument is start address, either alarm or log memory
void clrmem(uint16_t adr){
    uint16_t cb[64];
    uint8_t i;
    for(i=0;i!=64;i++)
        cb[i]=0xFFFF;
    for(i=0;i!=126;i++)
        writeeeprom((adr+i*0x80),cb,64);
}


bool inlim(int32_t val,int32_t minval,int32_t maxval){
    return((val>=minval)&&(val<=maxval));
}


// willl check regular mirrored frame, either from normal memory (norm==true) or from default
//will not place this frame into tx0103 variables, calling routine needs to do so. 
bool checkmirrored(bool norm){
    bool retval=true;
    uint8_t i;
    uint16_t adr,buf[68];
    if(norm)
        adr=0;
    else
        adr=0x00C0;
    retval=true;
    if(readeeprom(adr,buf,68)){
        for(i=0;i!=3;i++)
            retval&=inlim(buf[i],800,4500);      //nidle,nnom,ACnom
        retval&=inlim(buf[3],1,800);            //treg;
        for(i=4;i!=8;i++)
            retval&=inlim(buf[i],0,10000);       //Control constants 
        for(i=8;i!=10;i++){
            retval&=inlim((buf[i]&0xFF),0,100);   //idling and cranking time, high and low temp. 
            retval&=inlim((buf[i]>>8),0,100);
        }
        
        for(i=10;i!=12;i++)
            retval&=inlim(buf[i],500,4500);     //Booster voltages
        retval&=inlim(buf[13],50,50000);       //temp. alarm suppress time (warm-up time)
        retval&=inlim(buf[14],5,50);             //Booster capacitors
        for(i=15;i!=19;i++)
            retval&=inlim(buf[i],500,5500);     //voltage limits
        for(i=19;i!=22;i++)
            retval&=inlim(buf[i],350,750);     //nominal frequency and frequency limits
        for(i=22;i!=27;i++)
            retval&=inlim(buf[i],40,2000);     //current and power limits, nominal power
        for(i=27;i!=30;i++)
            retval&=inlim(buf[i],400,6000);        //dc voltage limits
        retval&=inlim(buf[30],1500,5500);     //engine speed limits
        retval&=inlim(buf[31],1500,9999);     //engine speed limits
      
        for(i=32;i!=40;i++)
            retval&=inlim(buf[i],60,60000);      //shutdown delay limits, temp. not included
        for(i=40;i!=48;i++){
            retval&=inlim((buf[i]&0xFF),20,200);   //temperature warnings and shutdown
            retval&=inlim((buf[i]>>8),20,200);
            retval&=(buf[i]&0xFF)<=((buf[i]>>8)&0xFF);    //warning needs to be below shutdown
        }
        retval&=inlim((buf[48]&0xFF),0,250);   //shutdown delay for temperatures
        //upper byte of buf[48] as well as buffers 49 .. 51 are not used
        
        retval&=inlim((buf[52]&0xFF),0,150);    //oil pressure warning
        retval&=inlim((buf[52]>>8),0,150);  //oil pressure shutdown
        retval&=inlim((buf[53]&0xFF),10,250);     //oil pressure delay normal temp.
        retval&=inlim((buf[53]>>8),1,90);   //oil pressure delay low temp.
        retval&=inlim(buf[54],200,1024);      //water leak trigger
        retval&=inlim((buf[55]&0xFF),1,90);     //tank delay
        retval&=inlim((buf[55]>>8),0,180);      //tank level
        retval&=inlim((buf[56]&0xFF),50,200);        //fan low speed
        retval&=inlim((buf[56]>>8),50,200);        //fan high speed
        retval&=((buf[56]&0xFF)<((buf[56]>>8)&0xFF));    //low speed needs to be higher than high speed

        retval&=inlim(buf[57],10,5000);         //Resistance tank full
        retval&=inlim(buf[58],10,5000);         //Resistance tank empty
        if(buf[57]==buf[58])
            retval=false;                       //res. must not be the same
        retval&=inlim(buf[59],500,3000);        //min. duty cycle OC2
        
       
        retval&=inlim(buf[61]&0xFF,50,200);     //cool down temp
        retval&=inlim(buf[61]>>8,1,10);        //cool down time
        
        retval&=inlim(buf[62]&0xFF,10,100);      //act. pos. cranking
        retval&=inlim(buf[62]>>8,10,100);      //act. pos. idling
        
//will not check config bits other than actuator configuration
        if(TESTBIT(buf[63],10) && TESTBIT(buf[63],11))
            retval=false;      // has both, Hitec and BLDC programmed
        else if(TESTBIT(buf[63],11)){
            bldc=true;
            limacthighspeed=STEPVALMAX_BLDC;
            limactlowspeed=STEPVALMIN_BLDC;
        }
        else{ 
            bldc=false;
            if(TESTBIT(buf[63],10)){
                limacthighspeed=STEPVALMAX;
                limactlowspeed=STEPVALMIN;
            }
            else 
                linact=true;
                
        }   
        retval&=inlim(buf[67]&0xFF,30,250);     //max. oil pressure
        retval&=inlim(buf[67]>>8,2,48);         //poles
        if(TESTBIT((buf[67]>>8),0))
            retval=false;
    }
    return(retval);
}

//will perform reset operations as defined in device description
void rstrq(uint32_t rstfield){
    uint16_t field[32],i;
    bool rdok=true;
    field[0]=field[1]=0;
    if(TESTBIT(rstfield,4)){
        writeeeprom(0x01A8,field,2);        //start counter 
        tx013A.v32.starts=0;
    }
    if(TESTBIT(rstfield,1))            //erase alarms
        erasmem(true);
    
    if(TESTBIT(rstfield,2))            //erase history
       erasmem(false);
                
    if(TESTBIT(rstfield,3)){            //reset to default
        if(checkmirrored(false)){
            for(i=0;i<192;i+=0x40){
                rdok&=readeeprom((i+192),field,0x20);
                if(rdok)
                    rdok&=writeeeprom(i,field,0x20);
                if(!rdok){
                    i=181;
                }
            }
            if(rdok)
                writecal(0x01F4);
        }  
    }
    if(TESTBIT(rstfield,0)){
        tx013A.v32.optime=0;
        writeeeprom(0x01A4,&tx013A.val.runtl,2);
    }
    
    if(TESTBIT(rstfield,5)){        //make current data default
        for(i=0;i<192;i+=64){
            rdok&=readeeprom(i,field,0x20);
            if(rdok)
                rdok&=writeeeprom((i+192),field,0x20);
        }
        if(rdok)
            writecal(0x01FC);
    }
    if(TESTBIT(rstfield,6))
        tx0103.logfields.logcntr=0;
    if(TESTBIT(rstfield,7))        
        clearenergy();    
}

void progmirrored(uint8_t *dta){
    uint8_t i;
    for(i=0;i!=68;i++){
        tx0130.wrds[i]=*dta++;
        tx0130.wrds[i]|=*dta++ <<8;
    }
    writeeeprom(0,tx0130.wrds,68);
    writecal(0x01E8);
    if(!checkmirrored(true))
        SETBIT(tx0101.val.stwordlow,2);
}

void prognonmirrored(uint8_t *dta){
    uint8_t i;
    uint16_t dat[18];
    for(i=0;i!=18;i++){
        dat[i]=*dta++;
        dat[i]|=* dta++ <<8;
    }
    writeeeprom(0x01B0,dat,18);
    writecal(0x01EC);    
}

//will program regular info frame (tx0138) but without programing the date! Date is always assigned to tx0130!)
void proginfo(uint8_t *dta){
    uint8_t i;
    for(i=0;i!=16;i++){
        tx0138.trans[i]=*dta++ <<8;      //ASCII word, first character is first place
        tx0138.trans[i]|=*dta++;
    }
    writeeeprom(0x0180,tx0138.trans,8);
    //skip VCS serial No, move on with total op. time, start counter and energy
    dta+=4;
    for(i=0;i!=6;i++){
        tx013A.trans[i]=*dta++;          //remaining data in little endian format
        tx013A.trans[i]|=*dta++ <<8;
    }  
    writeeeprom(0x01A4,&tx013A.val.runtl,2);
    writeeeprom(0x01D4,&tx013A.val.totalenl,2);
}


//will erase log memory (Alarm or history) and set pointer to first address
void erasmem(bool almem){
    uint16_t clrbf[32],from,to,i;
    bool retval=true;
    for(i=0;i!=32;i++)
        clrbf[i]=0xFFFF;
    if(almem){
        ALPTR=i=0;
        from=0x0A00;
        to=0x5781;
        writeeeprom(0x01AC,&i,1);
    }
    else{
        EVPTR=i=0;
        from=0x5800;
        to=0x7F81;
        writeeeprom(0x01AE,&i,1);
    }
    for(i=from;i<to;i+=0x80){
        retval&=writeeeprom(i,clrbf,32);
        retval&=writeeeprom((i+0x40),clrbf,32);
        if(!retval){
            i=to+2; //break the loop if writing errors occur
        }
    }
}

//this function will load an amount of wrds words (not bytes!) from memory, starting at address adr, into target buffer *targ. 
//Format can be little endian (be==false) or big endian (be=true)
//the routine is made for data transfer, largest buffers will not exceed 256 bytes. wrds is always in words, i.e. to read 32 bytes wrds needs to be set to 16
void  mem2trans(uint8_t wrds, uint32_t adr,uint8_t *targ,bool be){
    uint16_t buf[128],i;
    if(readeeprom(adr,buf,wrds)){
        for(i=0;i!=wrds;i++){
             if(be){
                 *targ++=buf[i]>>8;
                 *targ++=buf[i];
            }   
             else{ 
                *targ++=buf[i];
                *targ++=buf[i]>>8;
             }
        }
    }
    else{
        for(i=0;i!=wrds;i++){
            *targ++=0xFF;
            *targ++=0xFF;
        }
    }
}


//this function will load data as per transfer 0x0100, into *dta.
//it will return length of data transfer. Length in bytes!
uint8_t ldid(uint8_t *dta){
    uint8_t *src,i,id[]="VCS181C004";
    uint32_t fxx;
    uint16_t rdx[2];
    fxx=SWVER;
    for(i=0;i!=4;i++){
        *dta++=fxx;
        fxx>>=8;
    }
    if(!readeeprom(0x01A0,rdx,2)){
        rdx[0]=rdx[1]=0xFFFF;
    }
    *dta++=rdx[0];
    *dta++=rdx[0]>>8;
    *dta++=rdx[1];
    *dta++=rdx[1]>>8;
    *dta++=PROTREL & 0xFF;
    *dta++=PROTREL >>8;
    *dta++=TX101LENGTH;
    *dta++=TX130LENGTH;
    i=12;
    src=id;
    while(*dta++=*src++)
        i++;
    return(i);
}

//will load a log data set as is in tx0130.logfields.logcntr. 0..155 is alarm memory, 156-235 is history mem.
//will return false if that log data set is not valid, i.e. voltages L1 and L2 must not be 0x>5000;
bool ldlog(void){
    uint16_t adx;
    if(tx0103.logfields.logcntr<160){   //range 0 ..79 covers alarm recors, 80 .. 159 periodic logs
        adx=(tx0103.logfields.logcntr*0x80)+0x0A00;
        readeeprom(adx,&tx0103.trans[1],43);
        if((tx0103.logfields.x101[2]<5000)&&(tx0103.logfields.x101[3]<5000))
            return(true);
        else
            return(false);
    }
    else
        return(false);
}

//will recalculate calendar date based on todays date plus days
uint32_t newcaldat(uint16_t days){
    uint8_t day,month,nyear,actyear,tileom;
    actyear=nyear=caldate>>25;
    while(days>365){
        nyear+=1;
        days-=365;
    }
    month=(caldate>>21)&0x0F;
    day=(caldate>>16)&0x1F;
    if(days){
        switch(month){
            case 2:
                if(actyear%4)
                    tileom=28-day;
                else
                    tileom=29-day;
                break;
            case 4:
            case 6:
            case 9:
            case 11:
                tileom=30-day;
                break;
            default:
                tileom=31-day;
                break;
        }
        if(tileom>=days)
            day+=days;
        else{
            days-=tileom;
            month++;
            day=1;
            if(month==13){
                month=1;
                nyear++;
            }
            while(days){
                switch(month){
                    case 2:
                        tileom=28;
                        break;
                    case 4:
                    case 6:
                    case 9:
                    case 11:
                        tileom=30;
                        break;
                    default:
                        tileom=31;
                        break;
                }
                if(days>tileom){
                    days-=tileom;
                    month++;
                    if(month==13){
                        month=1;
                        nyear+=1;
                    }
                }
                else{
                    days=0;
                    day=tileom;
                }
            }
        }
    }
    return((caldate&0xFFFF)|(day<<16)|(month<<21)|(nyear<<25));
}

//this will reset a service notification. will always use regular calendar time and operating time retrigger
void ressrvnot(uint8_t notno){
    uint16_t rvx[4];
    uint32_t newop,newcal;
    if(notno>15)
        CLEARBIT(tx0101.val.servwordhigh,notno-16); //clear the message, in case it was pending
    else
        CLEARBIT(tx0101.val.servwordlow,notno);
    readeeprom(0x0220+notno*0x40,rvx,2);
    if((rvx[0]>16383)||(rvx[1]>16383)){   //this reset affects a SVN that is within iniial time. Shall not be re-triggered
        newop=newcal=0xFFFFFFFF;
        CLEARBIT(valsvn,notno);     //this notification is no longer valid
    }
    else{
        newop=tx013A.v32.optime+(uint32_t)(rvx[0])*60;  //new op. time in minutes
        newcal=newcaldat(rvx[1]);
    }
    rvx[0]=newop;
    rvx[1]=newop>>16;
    rvx[2]=newcal;
    rvx[3]=newcal>>16;
    writeeeprom(0x0228+notno*0x40,rvx,4);
    
}

void progsvwarn(uint8_t notno,uint8_t *txt,uint16_t *tmr){
    uint16_t rvx[16];
    uint8_t ch,i;
    uint32_t newop,newcal;
    for(i=0;i!=16;i++){
        if((*txt>0x1F)&&(*txt<0x80))
            ch=*txt++;
        else
            ch=0;
        rvx[i]=ch<<8;                       //first byte first, this is an ASCII frame!
        if((*txt>0x1F)&&(*txt<0x80))
            ch=*txt++;
        else
            ch=0;
        rvx[i]|=ch; 
    }
    writeeeprom(0x0200+notno*0x40,rvx,16);   //store text;
//bytes in tmr are in below order:
    //2 bytes op. time regular retrigger
    //2 bytes cal. days regular retrigger
    //2 bytes op. time initial retrigger
    //2 bytes cal. days initial retrigger
    for(i=0;i!=4;i++)
        rvx[i]=*tmr++;
    writeeeprom(0x0220+notno*0x40,rvx,4);
    /* If actual operating time is past initial trigger time then the initial option is done,will re-trigger on revolving date
     * initial operating time is from zero, not from actual one. Time is from "today
     */
    
    if((rvx[2]*60)>=tx013A.v32.optime){        //is past initial, check whether revolving is enabled or not
        if((rvx[0]>16383)||(rvx[1]>16383)){ //never re-trigger, revolving is disabled
            newop=0xFFFFFFFF;
            newcal=0xFFFFFFFF;
        }
        else{   //use regular revolving to calculate next
            newop=(uint32_t)(rvx[0])*60+tx013A.v32.optime;
            newcal=newcaldat(rvx[1]);
        }
    }
    else{//recalculate inital from today and from operting time zero
        newop=(uint32_t)(rvx[2])*60; 
        newcal=newcaldat(rvx[3]);   //initial calendar retrigger
    }
    rvx[0]=newop;
    rvx[1]=newop>>16;
    rvx[2]=newcal;
    rvx[3]=newcal>>16;
    writeeeprom(0x0228+notno*0x40,rvx,4);
    if(notno>15)
        CLEARBIT(tx0101.val.servwordhigh,notno-16);
    else
        CLEARBIT(tx0101.val.servwordlow,notno);
    writecal(0x01F8);
}

void prog1block (uint8_t *bf){      //first 4 bytes are block base address, next 64 bytes are block data
    //This routine will never overwrite calibration data or VCS serial No but will overwrite gen. serial No.
    uint16_t blockadr;
    uint8_t i,k;
    uint16_t rvx[32],rdx;   
    blockadr=*bf++;
    blockadr|=*bf++ <<8;
    if(blockadr<=0x09C0){  //Will not program anything beyond 0x0A00
        for(i=0;i!=32;i++){
            if(((blockadr+2*i)>=VCSSNLOC)&&((blockadr+2*i)<(VCSSNLOC+2*VCSSNLEN))){//do not overwrite VCS serial number. instead read VCS SN from memory and write it to rvx so it will be programmed
                readeeprom(blockadr+2*i,&rdx,1);
                rvx[i]=rdx;
                bf+=2;
            }
            else if(((blockadr+2*i)>=CALDATALOC)&&((blockadr+2*i)<(CALDATALOC+2*CALDATALEN))){  //same procedure for calibration data
                readeeprom(blockadr+2*i,&rdx,1);
                rvx[i]=rdx;
                bf+=2;
            }
            
            else{
                rvx[i]=*bf++;
                rvx[i]|=*bf++ <<8;
            }
        }
        writeeeprom(blockadr,rvx,32);
    }
}

//will read one block and return upated block coutner. Block counter will roll over to 0 after last program operation. In such case entire frame is 0xFF
uint16_t read1block(uint16_t blockcntr, uint8_t *bf){// first 2 bytes need to be blockcounter    
    uint8_t i;
    uint16_t rvx[32];
    if(blockcntr<=0x09C0){      //Send all 0xFFFF so host SW will know this is the last transfer
        *bf++=blockcntr;
        *bf++=blockcntr>>8;
        readeeprom(blockcntr,rvx,32);
        for(i=0;i!=32;i++){
            *bf++=rvx[i];
            *bf++=rvx[i]>>8;
        }
        return(blockcntr+64);
    }
    else{
        for(i=0;i!=66;i++)
            *bf++=0xFF;
        return(blockcntr);
    }
}

void cpureset(void){
    asm("di");
    uint32_t rswrst;
    SYSKEY=0;
    SYSKEY=0xAA996655;
    SYSKEY=0x556699AA;
    RSWRSTbits.SWRST=true;
    rswrst=RSWRST;
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
}

void checkpwrup(void){
    bool cont=true;
    uint32_t tmrx=t_1ms;
    while(((t_1ms-tmrx)<2000)&&cont){
        if((!onpending)||PORTDbits.RD7||PORTDbits.RD6)
            cont=false;
        CANCOM_Tasks();                       
    }
    if(cont)
        inlowpower(true);
}

void writesn(void){
    writecal(0x01F0);
    writeeeprom(0x01A0,&tx000A.val.vcssnl,2);
}