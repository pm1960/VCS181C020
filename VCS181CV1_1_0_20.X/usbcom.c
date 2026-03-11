#include "usbcom.h"

static uint16_t calcCRC(volatile uint8_t *data, volatile uint32_t len);
static void rxcheck (uint16_t transfer);
static void txtrans(uint16_t rqtransfer);
static void inittx(void);

void usbcont(void){
    static uint32_t tmrx;
    switch (usbstat){
        case USBCOM_STATE_INIT:
            if(PORTDbits.RD7){
                LATDSET=0x0800;
                tmrx=t_1ms;
                usbstat=USBCOM_STATE_RESET;
                usbreset();
            }
            else{
                U4MODE=0;
                IEC2bits.U4RXIE=false;
                IEC2bits.U4EIE=false;
            }
            break;
            
        case USBCOM_STATE_RESET:
            if(PORTDbits.RD7){
                if(t_1ms-tmrx>5){
                    LATDCLR=0x0800;
                    usbstat=USBCOM_STATE_READY;
                }
            }
            else
                usbstat=USBCOM_STATE_INIT;        
            break;
            
        case USBCOM_STATE_READY:
           if(!(PORTDbits.RD7))
                usbstat=USBCOM_STATE_INIT;
           else if(isrcom.Bits.rxon){ //check for time-out
               if((t_1ms-t_u4rx)>RXMAX){
                   asm("di");
                   u4rxptr=0;
                   isrcom.Bits.rxon=false;
                   asm("ei");
               }
            }
            else if((u4rxptr==u4rxbuf[4])&&(u4rxptr!=0)){  //got full data stream
                u4rxptr=0;
                if(calcCRC(u4rxbuf,(u4rxbuf[4]-2))==(u4rxbuf[u4rxbuf[4]-2]|(u4rxbuf[u4rxbuf[4]-1]<<8)))
                    rxcheck((u4rxbuf[2]|(u4rxbuf[3]<<8)));
                
            }
            break;
            
        default:
            usbstat=USBCOM_STATE_INIT;
            break;
    }
}


//reset all buffers, initialize port, enable reception   
//any error will immediately reset the receiver, data received will be lost
void usbreset(void){
    U4MODE=0;               //Reset port, no flow control, wake-up, loopback, or baud rate detection, 8N1
    U4STA=0;                //Keep in TX interrupt if at least one empty buffer space is available
    U4STAbits.URXISEL=0b10; // 6 bytes for RX. Will change in ISR
    U4STAbits.UTXEN=1;      //will always keep TX enabled, will switch interrupts on and off
    U4STAbits.URXEN=1;      //same here
    U4BRG=129;              //run @ 19200bit/s
    u4txbuf[0]=0xAA;
    u4txbuf[1]=0x55;
    u4rxptr=u4txptr=0;
    IFS2bits.U4EIF=IFS2bits.U4RXIF=IFS2bits.U4TXIF=false;
    IEC2bits.U4EIE=IEC2bits.U4RXIE=true;
    U4MODEbits.ON=true;
}


static const uint16_t crc_table[16] ={
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef
};


//Will calculate CRC16 using above table
uint16_t calcCRC(volatile uint8_t *data, volatile uint32_t len){
    uint16_t i,crc=0;
    while(len--){
        i = (crc >> 12) ^ (*data >> 4);
        crc = crc_table[i & 0x0F] ^ (crc << 4);
        i = (crc >> 12) ^ (*data >> 0);
        crc = crc_table[i & 0x0F] ^ (crc << 4);
        data++;
    }
    return (crc & 0xFFFF);
}


//this function will check if any action upon a complete RX is required and if so it will perform it. If a certain transfer was required, then 
//it will prepare and start that transfer. If any programming or calibration action was requested, then a flag will be returned. Next frame then 
//needs to be that requested programing or calibrating frame 
void rxcheck (uint16_t transfer){
    static GENCONTR_STATES lstb4tst;
    uint8_t i,txt[32];
    uint32_t vcsactbits;;
    uint16_t wrtm[64],adx;
    
    volatile uint8_t *src;

    if((transfer>0x013F)&&(transfer<0x0160)){
        src=&u4rxbuf[5];
        for(i=0;i!=32;i++)
            txt[i]=*src++;
        for(i=0;i!=4;i++){
            wrtm[i]=*src++;
            wrtm[i]|=*src++ <<8;
        }
        progsvwarn(transfer-0x0140,txt,wrtm);
    }
    
    else if((transfer>0x03FF) && (transfer<0x0600)){
        adx=(transfer-0x0400)<<7;
        for(i=0;i!=64;i++)
            wrtm[i]=u4rxbuf[5+2*i] | (u4rxbuf[6+2*i]<<8);
        writeeeprom(adx,wrtm,64);
    }
    
    
    switch(transfer){
        case 0x0001:        //this is a simple request.
            if(u4rxbuf[5]==0){     //reply if addressed to the VCS
                txtrans(u4rxbuf[6]|(u4rxbuf[7]<<8));
            }
            break;
        //case 0x0002 not defined for USB
        case 0x0003:
            vcsactbits=get4bytes(&u4rxbuf[5]);
            caldate=get4bytes(&u4rxbuf[9]);
            now=t_1ms;
            if(TESTBIT(vcsactbits,0)){ // bit 0 has highest priority; no other activity if any status change is required
                if(ACTSTAT){            //bit 0 will toggle actual status: off->on->cool down->off
                    REQSTAT=false;
                    stfeedb.fields.newin=true;     
                }
                else if(!stfeedb.fields.startinhib){    //will not allow start if some alarm is pending
                    REQSTAT=true;
                    stfeedb.fields.newin=true;
                }
            }
            else{ 
                if(vcsactbits==64){  //if in RUN mode then run test will be entered, otherwise dry test
                     if(gencontrstate==GENCONTR_STATE_DRYTEST)
                        gencontrstate=lstb4tst;
                    else{// if(tx0101.val.engspeed<MINSPEED){
                        lstb4tst=gencontrstate;
                        gencontrstate=GENCONTR_STATE_DRYTEST;       //anything else but stop mode test not enabled at this time
                    }
//                    switch(gencontrstate){
//                        case GENCONTR_STATE_ACTCALINICOM:
//                            SETBIT(STATWRDL,1);        //entered test mode with actuator disabled. May be required for compensation test
//                        case GENCONTR_STATE_INSTOP: //need to go to dry-test
//                            gencontrstate=GENCONTR_STATE_DRYTEST;
//                            break;
//                        case GENCONTR_STATE_DRYTEST: //need to return from drytest
//                            gencontrstate=GENCONTR_STATE_INSTOP;
//                            break;
//                        case GENCONTR_STATE_INRUN:
//                            gencontrstate=GENCONTR_STATE_RUNTEST;
//                            break;
//                        case GENCONTR_STATE_RUNTEST:
//                            gencontrstate=GENCONTR_STATE_INRUN;
//                            break;
//                        default:
//                            break;
//                    }
                }
            }
            break;
            
        case 0x0004:    //confirmation frame
            confword0=get4bytes(&u4rxbuf[5]);
            confword1=get4bytes(&u4rxbuf[9]);
            break;
            
        case 0x0005:    //Calendar time from panel. Panel configuration and panel status not defined for USB communication 
            caldate=get4bytes(&u4rxbuf[9]);
            now=t_1ms;
            break;
            
            
        case 0x0007:
            caldate=get4bytes(&u4rxbuf[9]);
            now=t_1ms;
            rstrq(get4bytes(&u4rxbuf[5]));
            break;
            
        case 0x0009:
            vcsactbits=get4bytes(&u4rxbuf[5]);
            caldate=get4bytes(&u4rxbuf[9]);
            for(i=0;i!=32;i++)
                if(TESTBIT(vcsactbits,i))
                    ressrvnot(i);
            break;
            
        case 0x000A:
            caldate=get4bytes(&u4rxbuf[5]);
            tx000A.val.vcssnl=u4rxbuf[9]|(u4rxbuf[10]<<8);
            tx000A.val.vcssnh=u4rxbuf[11]|(u4rxbuf[12]<<8);
            writesn();
            prepsoftres();
            cpureset();
            break;
            
        case 0x000B:
            tx0103.logfields.logcntr=u4rxbuf[9]|(u4rxbuf[10]<<8);
            break;
            
            
        case 0x0010:
            if(u4rxbuf[5]==0){
                prepsoftres();
                cpureset();
            }
            break;
            
        case 0x0130:
            progmirrored(&u4rxbuf[5]);
            break;
            
        case 0x0131:
            prognonmirrored(&u4rxbuf[9]);       //skip alarm pointer and memory pointer      
            break;
            
            
        case 0x0138:
            proginfo(&u4rxbuf[5]);
            break;
            
        case 0x013A:
           wrtm[0]=u4rxbuf[5]|(u4rxbuf[6]<<8);  //op. time low word
           wrtm[1]=u4rxbuf[7]|(u4rxbuf[8]<<8);  //op time high word
           wrtm[2]=u4rxbuf[9]|(u4rxbuf[10]<<8); //engine starts low word
           wrtm[3]=u4rxbuf[11]|(u4rxbuf[12]<<8);    //engine starts high byte
           writeeeprom(0x01A4,wrtm,4);
           wrtm[0]=u4rxbuf[13]|(u4rxbuf[14]<<8);    //energy low word
           wrtm[1]=u4rxbuf[15]|(u4rxbuf[16]<<8);    //energy high word
           writeeeprom(0x01D4,wrtm,2);
           break;
            
        default:
            break;       
    }
}

//this function will transfer the requested frame. It will only run if no transfer is ongoing while it is called, to avoid buffer overflow
void txtrans(uint16_t rqtransfer){
    uint16_t crc,wrtm[64],adx;
    uint8_t i;
    volatile uint8_t *snk;
    if(!IEC2bits.U4TXIE){   //check for no other transmission going on
        if((rqtransfer>0x013F)&&(rqtransfer<0x0160)){//these are service warnings. 
            u4txbuf[2]=rqtransfer;
            u4txbuf[3]=rqtransfer>>8;
            u4txbuf[4]=47;//length
            mem2trans(16,0x0200+(rqtransfer-0x0140)*0x40,&u4txbuf[5],true);      //32 bytes of text
            mem2trans(4,0x0220+(rqtransfer-0x140)*0x40,&u4txbuf[37],false);      //re-trigger values
        }
        else if((rqtransfer>0x03FF) && (rqtransfer<0x0600)){    //requesting memory data
            adx=(rqtransfer-0x400)<<7;  //address is memory block x128
            readeeprom(adx,wrtm,64);
            u4txbuf[2]=rqtransfer;
            u4txbuf[3]=rqtransfer>>8;
            u4txbuf[4]=135;  //total length is 128 data bytes and 7 bytes overhead
            snk=&u4txbuf[5];
            for(i=0;i!=64;i++){
                *snk++=wrtm[i];
                *snk++=wrtm[i]>>8;
            }
        }

        else{ 
            u4txbuf[2]=rqtransfer;
            u4txbuf[3]=rqtransfer>>8;
            switch(rqtransfer){

                case 0x0100:
                   u4txbuf[4]=ldid(&u4txbuf[5])+7;
                   break;
                    
                case 0x0101:
                    u4txbuf[5]=gencontrstate;
                    u4txbuf[6]=stfeedb.bts[0];
                    u4txbuf[7]=usbstat;
                    u4txbuf[8]=cancomstate;
                    snk=&u4txbuf[9];
                    for(i=2;i!=35;i++){
                        *snk++=tx0101.trans[i];
                        *snk++=tx0101.trans[i] >>8;
                    }
                    u4txbuf[4]=77;
                    break;
                    
                case 0x0103://retrieving log data. Check if that data set is available. Always read from actual address as is in tx0103.logcntr, then increment
                    if(ldlog()){
                        u4txbuf[4]=95;  //length of a log transfer:2 bytes log counter, 4 bytes cal. date, 70 bytes as in tx0101, 12 bytes as in tx013A,7 bytes header/trailer
                        snk=&u4txbuf[5];
                        for(i=0;i!=44;i++){
                            *snk++=tx0103.trans[i];
                            *snk++=tx0103.trans[i]>>8;
                        }
                    }
                    else{   //need to send NACK, no log data available
                        u4txbuf[4]=23;   //length, 16 x0xFF and 7 bytes header
                        for(i=0;i!=16;i++)
                        u4txbuf[5+i]=0xFF;
                    }
                    break;
                    
                case 0x0130:    //need to send a mirrored transfer
                    mem2trans(68,0,&u4txbuf[5],false);    //regular non-mirrored frame
                    u4txbuf[4]=143;
                    break;
                    
                case 0x0131:
                    mem2trans(20,0x01AC,&u4txbuf[5],false);
                    u4txbuf[4]=47;
                    break;
                    

                    
                case 0x0138:
                    for(i=0;i!=32;i++)
                        u4txbuf[5+i]=tx0138.val.gensn[i];      //will use this instead of direct memory transfer, since data may be invalid
                    u4txbuf[4]=39;
                    break;
                    
                case 0x0139:
                    u4txbuf[4]=31;
                    mem2trans(12,0x01E8,&u4txbuf[5],false);
                    break;
                    
                case 0x013A:        //this is a request transfer in USB communication
                    for(i=0;i!=6;i++){
                        u4txbuf[5+2*i]=tx013A.trans[i];
                        u4txbuf[6+2*i]=tx013A.trans[i]>>8;
                    }
                    u4txbuf[4]=19;
                    break;
                             

//no further transfers defined for the VCS                            
                default:
                    break;
                    
            }
        }
        crc=calcCRC(u4txbuf,u4txbuf[4]-2);
        u4txbuf[u4txbuf[4]-2]=crc;
        u4txbuf[u4txbuf[4]-1]=crc>>8;
        inittx();
    }
}


//inittx will just start transmission process. It assumes outbput buffer is fully loaded, including checksum
//It will not start if a transmission is still going on!
void inittx(void){
    u4txptr=0;
    if(!IEC2bits.U4TXIE){
        U4STAbits.UTXEN=1;
        IFS2bits.U4TXIF=0;
        IEC2bits.U4TXIE=1;
    }
}