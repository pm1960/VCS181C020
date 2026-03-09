#include "CanCom.h"

//list of valid transfers. Needs to end with 0
//service warnings (0x0140...0x015F) are not required to be integrated in this array, will always check for those
uint16_t vcstrans[]={1,2,3,4,5,7,9,10,11,16,0x0100,0x0130,0x0138,0x013A,0x013B,0x013C,0};

static bool canoff(void);
static bool append2fifo0(CANTxMessageBuffer *msg);
static bool append2fifo1(CANTxMessageBuffer *msg);
static void readeidbuf(void);
static void makeeid(CANTxMessageBuffer *msg,uint16_t transferid,uint8_t prio);
static bool txsfr(uint8_t sfrno);
static void txmfr(uint8_t mfrno, bool *pend);
static bool valtrans(uint16_t transfer);
static void proctrans(uint16_t transferid,uint8_t src, uint8_t *msg);
static void checkmfr(uint16_t transferid,uint8_t src,volatile CANRxMessageBuffer *msg);
static bool inarray(uint8_t idbyte, uint8_t *bp);
static bool getemptysf(uint8_t *k);
static bool getemptymf(uint8_t *k);
static void chkpansvn(uint16_t adr,uint8_t svn,uint16_t *bf);
static uint32_t evalcal(uint8_t *bf);
static void forcepwrdwn(void);
static void sendmfr(void);
static void sendsfr(void);

void CANCOM_Tasks (void){
    static uint32_t tbus,tx013A,tmtx0101;
    uint8_t i,bf[4];
    bool cont;
    switch (cancomstate){
        case CAN_STATE_INIT:
            if((t_1ms-tbusfail)>100){
                if(!canoff()){
                    cancomstate=CAN_STATE_HWIRESP;
                    tbusfail=t_1ms;
                    SETBIT(MSGWRDH,4);
                }
                else if(!initcan()){
                    tbusfail=t_1ms;
                    cancomstate=CAN_STATE_HWIRESP;
                    SETBIT(MSGWRDH,4);
                }
                else{
                    cancomstate=CAN_STATE_BUSOFF;
                    tbus=t_1ms;
                }
            }
            break;
            
        case CAN_STATE_BUSOFF:
            if(((t_1ms-tbus)>CANOFF)&&(!stfeedb.fields.pwrdwn))
                if(PORTDbits.RD7){
                    cancomstate=CAN_STATE_TURNON;
                    tbus=t_1ms;
                    bf[0]=0x33;
                    bf[1]=0xBB;
                    bf[2]=0x55;
                    bf[3]=0xAA;
                    txtrans(2,bf);
                    onpending=false;
                }
                else{
                    if(!stfeedb.fields.pwrdwn)
                        cancomstate=CAN_STATE_BUSON;
                    else if((gencontrstate==GENCONTR_STATE_INIT)||(gencontrstate==GENCONTR_STATE_INITEND))
                        SETBIT(MSGWRDL,6);
                }
            break;
            
        case CAN_STATE_TURNON:
            if((t_1ms-tbus)>250){
                tbus=t_1ms;
                bf[0]=0x33;
                bf[1]=0xBB;
                bf[2]=0x55;
                bf[3]=0xAA;
                txtrans(2,bf);
                cancomstate=CAN_STATE_BUSON;
            }
            break;
                
        case CAN_STATE_BUSON:   //will also take care about transmissions; will send max one SFR and one MFR per cycle
            for(i=0;i!=MFBUFS;i++)
            if((mfx[i]->status==BUSY) && ((t_1ms-mfx[i]->rxstart)>RXMAX))       //set orphaned multiple transfers to idle
                mfx[i]->status=IDLE;    
            readeidbuf();   
            for(i=0;i!=MFBUFS;i++)
                if(mfr[i].status==DTA_AVAIL){
                    proctrans(mfr[i].idword,mfr[i].source,mfdata[i]);
                    mfr[i].status=IDLE;
                }
            if((!TESTBIT(tx0101.val.miscwordlow,4))&&((t_1ms-tx013A)>5000)){
                if(txtrans(0x013A,NULL))
                    tx013A=t_1ms;
            } 
            if((t_1ms-tmtx0101)>500){
                if(txtrans(0x0101,NULL))
                    tmtx0101=t_1ms;  
            }
            if(!stfeedb.fields.pwrdwn){
                sendsfr();
                sendmfr();
            }
            break;
            
        case CAN_STATE_HWIRESP: //try cycle the bs after 500ms.
            if((t_1ms-tbusfail)>500){
                if(canoff())
                    if(initcan()){
                        CLEARBIT(tx0101.val.mwordhigh,4);
                        cancomstate=CAN_STATE_BUSOFF;
                        tbus=t_1ms;
                    }
            }
            break;
            
        default:
            cancomstate=CAN_STATE_INIT;
            break;
        
    }
}

//this function will SFR's if required and as long as the buffer is available
void sendsfr(void){
    uint8_t i=0;
    static uint32_t tmrx;
    while(i<SFBUFS){
        if(sfr[i].status==TX_REQ){
            if(txsfr(i))
                sfr[i].status=IDLE;
            else
                i=SFBUFS;  //break this loop, FIFO is full
        }
        i++; 
    }
}

//this function will send one MFR transfer if available. Transfer always through FIFO 0. If the FIFO is not empty the routine will end and try again on next call. 
//if the FIFO becomes clogged up the routine will attempt later or, if no success for a certain time, will give up
void sendmfr(void){
    static uint32_t tmrx;
    static bool pending;
    uint8_t i=0;
    if(stfeedb.fields.pwrdwn){
        pending=false;
        for(i=0;i!=MFBUFS;i++)
            mfr[i].status=IDLE;
    }
    else{
        if(pending){    //previous MFR was not fully sent, try again
            if((t_1ms-tmrx)>MAXFIFOFULL)
                cancomstate=CAN_STATE_INIT;      //transfer needs to be done in 500ms or a bus error will be triggered
            else
                txmfr(0,&pending);      //buffer No is irrelevant if last transfer was pending
        }
        while((i<MFBUFS) && !pending){
            if(mfr[i].status==TX_REQ){
                txmfr(i,&pending);
                if(pending)
                    tmrx=t_1ms;
                else
                    mfr[i].status=IDLE;
            }
            i++;
        }
    }
}



//this global function may be called from outside CAN operating algorithm. It will transmit the transfer ID handed over.
// Frame length and frame specific sequences need to be known by this routine / will be handled as static variables in this routine
//if any generic data is required it needs to be available from *msg on. The routine must not be used for regular CAN communication but shall
//be limited to functions from outside this file attempting to access the CAN bus. Any return data will be handled by the regular CAN reception anyway
//will return true if data was sent
bool txtransfer(uint16_t transferid, uint8_t *msg){
    uint8_t i,k;
    bool retval=false;
    switch(transferid){
        case 0x0001:    //outside routine asking for a specific request. 
            if(getemptysf(&k)){
                sfr[k].idword=1;
                sfr[k].buf[0]=*msg++;   //this is ecpected to be the address of intended receiver
                sfr[k].buf[1]=*msg++;   //this is the transfer ID that is requested
                sfr[k].buf[2]=*msg++;   //high byte of transfer ID
                for(i=3;i!=8;i++)
                    sfr[k].buf[i]=0xFF;
                sfr[k].status=TX_REQ;
                retval=true;
            }
            break;
            
            
            
        default:
            break;
    }
    return(retval);
}


//this routine will turn off the CAN bus if it was on. If it is not successful it will return false
bool canoff(void){
    uint32_t tmrx;
    if(C1CONbits.ON){
        tmrx=t_1ms;
        C1CONbits.REQOP=4;
        while(((t_1ms-tmrx)<=10)&&(C1CONbits.OPMOD!=C1CONbits.REQOP));
        if(C1CONbits.OPMOD!=C1CONbits.REQOP)
            return(false);
        else
            return(true);
    }
    else
        return(true);
}

//will initialize CAN 1 as internal CAN bus, standard protocol. Will return true if successful
bool initcan(void){
    uint32_t tmrx;
    uint8_t i;
   //will now turn CAN module 1 back on and then prepare interrupts
    C1CONSET=0x0010A000;	//Enable time-stamp capture and turn on module and set SIDLE bit
    C1INTCLR=0xF80FF80F;	//clear all interrupts
    C1INTSET=0x70020000;	//enable receive(RBIE), System err.(SERRIE) CAN bus err. (CERRIE) and wake up interrupts

    //enable RX not empty IE for FIFO 2 (RX FIFO)
    C1FIFOINT2SET=0x10000;
   

    //set CAN interrupt priority and enable it
    IPC11bits.CAN1IP=7;	//assign highest priority
    IPC11bits.CAN1IS=3;	//and highest subpriority
    IFS1bits.CAN1IF=0;
    IEC1bits.CAN1IE=1;	//enable CAN interrupts;
//now CAN mode is required to be configuration mode:
    C1CONbits.REQOP=0b100;
    while(C1CONbits.OPMOD!=0b100);
     //set up for correct bit timing: 250kbit/s, with 10 time quanta per bit
    C1CFG=0;			//reinit to zero, 1 sample point only
    C1CFGbits.SEG2PHTS=1;	//segment 2 freely programmable
    C1CFGbits.SEG2PH=2;		//phase segment 2 is 3 TQ
    C1CFGbits.SEG1PH=2;		//same as phase segment 1
    C1CFGbits.PRSEG=2;		//propagation delay ist 3 TQ
    C1CFGbits.SJW=1;		//SJW=2 TQ
    C1CFGbits.BRP=15;		//Prescale FSYS by 16, with an additional prescaler of 2 this results in 400ns per TQ
    C1FIFOBA=KVA_TO_PA(C1FiFoBuff);         //assign physical address to buffer base address

    C1TMRbits.CANTSPRE=40000;   //will capture can frames with resolution of 500us
    C1CONbits.CANCAP=1;

    //now initialize all buffers
    C1FIFOCON0=C1FIFOCON1=0x001F0080;        //All TX FIFO set for max. size (32 messages)
    C1FIFOCON0bits.TXPRI=0b11;               //FIFO 0 has highest priority
    C1FIFOCON1bits.TXPRI=0b10;               //FIFO 1 has lower priority
    C1FIFOCON2=0x001F2000;                   //RX FIFO set for max. size, priority 0, DONLY cleard
 
//message filtering: This CAN bus is set up for FP CAN bus communication using extended data format.
//the filter will pass through all messages since only 2 messages are of no concern to the VCS (address gathering). 
//thus the SW will only filter for 
//No specific filtering will occur on inbound messages since panel addresses are arbitrary and the VCS needs to handle all panel messages
    C1RXM0=0x00080000;              //Filter for MIDE bit only
    C1RXF0=0x00080000;              //trace for MIDE bit set (extended frame messages) only

//Select mask registers and assign message FIFO buffers to filters
    C1FLTCON0=0;                    //disable all filters
    C1FLTCON0bits.MSEL0=0;          //use acceptance mask 0
    C1FLTCON0bits.FSEL0=2;          //use FIFO 2, unspecified, all messages are panel messages
    C1FLTCON0bits.FLTEN0=1;         //enable filter

//no other filters enabled
    
//Operation mode now needs to be normal
    C1CONbits.REQOP=0;
    tmrx=t_1ms;
    while((C1CONbits.OPMOD!=C1CONbits.REQOP)&&((t_1ms-tmrx)<10));	//wait for the module to get into normal mode
    if(C1CONbits.REQOP!=C1CONbits.OPMOD)
        return(false);
    else{
        canrxptr=canprocptr=0; //initialize buffer pointers
        for(i=0;i!=MFBUFS;i++){
            mfx[i]=&mfr[i];
            mfx[i]->status=IDLE;
        }
        for(i=0;i!=SFBUFS;i++){
            sfx[i]=&sfr[i];
            sfx[i]->status=IDLE;
        }
        return(true);
    }
}
//will place a CAN message into fifo 1 if it's not full. Returns true if so.
bool  append2fifo1(CANTxMessageBuffer *msg){
    CANTxMessageBuffer *buffapend;
    if(C1FIFOINT1bits.TXNFULLIF){
        buffapend=PA_TO_KVA1(C1FIFOUA1);
        *buffapend=*msg;
        C1FIFOCON1SET=0x2000;   //set UINC
        C1FIFOCON1SET=0x08;     //set transmit request
        return(true);               
    }
    else
        return(false);
}

//same as above but for FIFO 0, high priority
bool append2fifo0(CANTxMessageBuffer *msg){
    CANTxMessageBuffer *buffapend;
    if(C1FIFOINT0bits.TXNFULLIF){
        buffapend=PA_TO_KVA1(C1FIFOUA0);
        *buffapend=*msg;
        C1FIFOCON0SET=0x2000;   //set UINC
        C1FIFOCON0SET=0x08;     //set transmit request
        return(true);
    }
    else
        return(false);
}

//this function will transmit sfrout. It assumes the buffer is ready for transmission, particularly unused bytes need to be set to 0xFF
//and IDword to be set to actual transfer ID. Source will be default set to zero, since this is the VCS
bool txsfr(uint8_t sbufno){
    CANTxMessageBuffer outbf;
    cansfr *sf;
    bool sendok=true;
    sf=sfx[sbufno];
    makeeid(&outbf,sf->idword,3);
    outbf.messageWord[2]=sf->buf[0]|(sf->buf[1]<<8)|(sf->buf[2]<<16)|(sf->buf[3]<<24);
    outbf.messageWord[3]=sf->buf[4]|(sf->buf[5]<<8)|(sf->buf[6]<<16)|(sf->buf[7]<<24);
    if(stfeedb.fields.pwrdwn)
        sendok=outbf.messageWord[2]==0xCC44DD22;
    if(sendok)
        return(append2fifo0(&outbf));
    else
        return(true);
}

//this function will read and process data from EID FIFO (FIF). Inbound data transfers will be assembled 
//into either a multi-frame input buffer or into a single input frame buffer, depending on the ID word. If a new message is detected
//and no buffer is free, then that message will be lost. 
void readeidbuf(void){
    uint8_t src;
    uint16_t transferid;
    while(canprocptr!=canrxptr){  //will stop processing if some data becomes available. In that case, that data needs to be processed first
        src=canrxbuf[++canprocptr].msgEID.EID&0xFF;   //source of this frame
        transferid=((canrxbuf[canprocptr].msgEID.EID>>8)&0x03FF)|((canrxbuf[canprocptr].msgSID.SID&0x3F)<<10);
        if(valtrans(transferid)){ 
            if(transferid<0x0100)
                proctrans(transferid,src,(uint8_t *)canrxbuf[canprocptr].data);
            else//will need to check for multiple frame transmission(s) and call for processing of such transmission(s))
                checkmfr(transferid,src,&canrxbuf[canprocptr]);
        }
    }//while, canprocptr!=canrxptr;
}


//this function will process a valid transfer, similar to USB functions
//*msg is the start of the message, which could be a single frame transfer or a multiple frame transfer
void proctrans(uint16_t transferid,uint8_t src, uint8_t *msg){
    static bool latchbs;
    uint32_t vcsact;
    uint16_t rqtrans,indata[2],wrtm[4],adrx;
    uint8_t i,txt[32],k;
    nocan=0;
    if((transferid>0x013F)&&(transferid<0x0160)){//got a service notification
        for(i=0;i!=32;i++)
            txt[i]=*msg++;
        for(i=0;i!=4;i++){
            wrtm[i]=*msg++;
            wrtm[i]|=*msg++ <<8;
        }
        progsvwarn(transferid-0x0140,txt,wrtm);
    }
    else
        switch(transferid){
            case 0x0001:    //general frame request
                if(*msg++ ==0){//check if VCS is addressed
                    rqtrans=*msg++;
                    rqtrans|=(*msg++)<<8;
                    txtrans(rqtrans,NULL);
                }
                break;

            case 0x0002:
                vcsact=get4bytes(msg);
                if(vcsact==0xCC44DD22)
                    stfeedb.fields.pwrdwn=true;          //power down requested by via CAN bus
                
                else if(vcsact==0xAA55BB33)
                    onpending=false;
                break;

            case 0x0003:    //VCS activity request. All bits are mutually exclusive
                vcsact=get4bytes(msg);
                msg+=4;
                caldate=evalcal(msg);
                if(TESTBIT(vcsact,0)){ // bit 0 has highest priority; no other activity if any status change is required
                    if(ACTSTAT){            //bit 0 will toggle actual status: off->on->cool down->off
                        REQSTAT=false;
                        stfeedb.fields.newin=true;
                    }
                    else if(!stfeedb.fields.startinhib){    //will not allow start if some alarm is pending
                        REQSTAT=true;
                        stfeedb.fields.newin=true;
                    }
                }
//                else if(TESTBIT(vcsact,6)){  //will only enter test mode from stop and regular run, nothing else
//                    switch(gencontrstate){
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
//                }
//                else if(TESTBIT(vcsact,8)&&(!latchbs)){//may toggle combat override, if it is not latched by some static input
//                    if(TESTBIT(MSGWRDL,10))
//                        CLEARBIT(MSGWRDL,10);
//                    else
//                        SETBIT(MSGWRDL,10);
//                }
                break;

            case 0x0004:    //confirmation frame
                confword0=get4bytes(msg);
                msg+=4;
                confword1=get4bytes(msg);
                break;
                
            case 0x0005: //Panel sending status bits, update remote start, battle short and calendar time
                onpending=false;
                i=*msg;
                msg+=4;
                if(TESTBIT(i,4))
                    pan_estop=true;     
                else
                    pan_estop=false;
                if(TESTBIT(i,5))
                    pan_bs=true;
                else
                    pan_bs=false;
                if(!TESTBIT(i,1))    //update calendar frame if panel RTC is not out of sync
                    caldate=evalcal(msg);              //will not cause programing routine to abandon!
                break;  
                
            case 0x0007:
                vcsact=get4bytes(msg);
                msg+=4;
                caldate=evalcal(msg);
                rstrq(vcsact);
                forcepwrdwn();
                break;  

            case 0x0009:
                vcsact=get4bytes(msg);
                msg+=4;
                for(i=0;i!=32;i++)
                    if(TESTBIT(vcsact,i))
                        ressrvnot(i);
                break;
                
            case 0x000A:
                if(get4bytes(msg)==0){
                    msg+=4;
                    tx000A.val.vcssnl=*msg++;
                    tx000A.val.vcssnl|=*msg++ <<8;
                    tx000A.val.vcssnh=*msg++;
                    tx000A.val.vcssnh|=*msg++ <<8;
                    writeeeprom(0x01A0,&tx000A.val.vcssnl,2);
                }
                break;
                
            case 0x000B:    //special request, panel asking for log data as in bytes 4 and 5 of input string
                msg+=4;
                tx0103.logfields.logcntr=*msg++;
                tx0103.logfields.logcntr|=*msg++ <<8;
                break;
                
                
            case 0x0010:
            if(*msg++ ==0){//check if VCS is addressed{
                prepsoftres();
                cpureset();
            }
            break;

            case 0x0130:
                progmirrored(msg);
                forcepwrdwn();
                break;

            case 0x0131:
                msg+=4;                 //skip alarm pointer and memory pointer
                prognonmirrored(msg);      
                forcepwrdwn();       
                break;
 

            case 0x0138:
                proginfo(msg);
                break;              
                
            case 0x013A:    //Data from panel, flash to VS memory if data is valid.
                indata[0]=*msg++;
                indata[0]|=*msg++ <<8;      
                indata[1]=*msg++;
                indata[1]|=*msg++ <<8;
                if((indata[0]|(indata[1]<<16))<9999999){
                    tx013A.val.runtl=indata[0];
                    tx013A.val.runth=indata[1];
                    writeeeprom(0x01A4,indata,2);

                    indata[0]=*msg++;
                    indata[0]|=*msg++ <<8;
                    indata[1]=*msg++;
                    indata[1]|=*msg++ <<8;
                    tx013A.val.startcntl=indata[0];
                    tx013A.val.startcnth=indata[1];
                    writeeeprom(0x01A8,indata,2);

                    indata[0]=*msg++;
                    indata[0]|=*msg++ <<8;
                    indata[1]=*msg++;
                    indata[1]|=*msg++ <<8;
                    tx013A.val.totalenl=indata[0];
                    tx013A.val.totalenh=indata[1];
                    writeeeprom(0x01D4,indata,2);
                    CLEARBIT(tx0101.val.mwordhigh,5);
                    lasttrans=0x013A;
                }
                break;  
                
            case 0x013B:    //Data from panel, flash to VS memory if data is valid.
            case 0x013C:
                lasttrans=transferid;   //store to file as received from panel
                if(transferid==0x013B)
                    adrx=0x0200;
                else
                    adrx=0x0600;
                for(i=0;i!=16;i++){
                    for(k=0;k!=4;k++){
                        wrtm[k]=*msg++;
                        wrtm[k]|=*msg++ <<8;
                    }
                    if(transferid==0x013B)
                        k=i;
                    else
                        k=i+16;
                    chkpansvn(adrx,k,wrtm);
                    adrx+=64;
                }
                break;
                
            default:
                break; 
        }
}


//this function is static to procmsg, particularly transfers 0x013B and 0x013C. It will read from adrx and compare with string handed over
//if data is the same, there is no need to write anything. If it is not, a validity check will occur:
//if either re-triger time, calendar or operating time, is 0xFFFFFFFF but the notification it is concerning is valid, then that notification will be reset
//adr will always point to the beginning of a service message, so read and write requires an offset of 40 bytes
void chkpansvn(uint16_t adr,uint8_t svn,uint16_t *bf){
    uint16_t wrb[4],rdb[4];
    uint8_t i;
    bool same=true;
    uint32_t opt,calt;
    for(i=0;i!=4;i++)
        wrb[i]=*bf++;
    if(readeeprom(adr+40,rdb,4)){
        for(i=0;i!=4;i++)
            if(rdb[i]!=wrb[i])
                same=false;
        opt=wrb[0]|(wrb[1]<<16);
        calt=wrb[2]|(wrb[3]<<16);
        if(TESTBIT(valsvn,svn)&&((opt==0xFFFFFFFF)||(calt==0xFFFFFFFF))){
            ressrvnot(svn);
            same =true;     //reset for logic inconsistency. do not overwrite! This is e new svn that was not enabled in previous VCS
        }
        else if(!TESTBIT(valsvn,svn)&&((opt!=0xFFFFFFFF)||(calt!=0xFFFFFFFF))){
            same=false;     //requires reset. Logic inconsistent, this svn was enabled in previous VCS
            wrb[0]=wrb[1]=wrb[2]=wrb[3]=0xFFFF;
        }
        if(same)
            writeeeprom(adr+40,wrb,4);
    }
}










//this function will transmit the data frame requested. works similar to the USB. Will return OK if data frame was placed into outbound buffer
//if any additional data is required it needs to be in buffer *bf. Specific to particual transfers
bool txtrans(uint16_t rqtrans, uint8_t *bf){
    static uint8_t seq013f[32],seq0100,seq0101,seq0103,seq0130,seq0131,seq0132,
            seq0138,seq0139,seq013A,seq013B,seq013C;     //sequence counter for service warnings and other MFR transfers
    uint8_t *snk,i,k,h;
    uint16_t adrx,rd[4];
    bool retval=false;

    if((rqtrans>0x013F)&&(rqtrans<0x0160)){  //these are service warnings. 
        if(getemptymf(&k)){    //if busy with other transactions then skip
            mfr[i].dset=seq013f[rqtrans-0x0140]=(seq013f[rqtrans-0x0140]+0x20)&0xE0;
            mfr[i].idword=rqtrans;
            mfr[i].length=40;
            mem2trans(16,(0x0200+(rqtrans-0x0140)*0x40),&mfdata[i][0],true);
            mem2trans(4,(0x0220+(rqtrans-0x0140)*0x40),&mfdata[i][32],false);
            mfr[i].status=TX_REQ;
            retval=true;
        }
    }
  
    else switch(rqtrans){
        case 0x0002:    //for transfer 2 code word is mandatory!
            if(getemptysf(&k)){
                sfr[k].idword=2;
                sfr[k].status=TX_REQ;
                for(i=0;i!=4;i++)
                    sfr[k].buf[i]=*bf++;
                for(i=4;i!=8;i++)
                    sfr[k].buf[i]=0xFF;
                retval=true;
            }
            break;
            
        case 0x000A:    //regular calibration transfer
            if(getemptysf(&k)){
                readeeprom(0x01A0,rd,2);
                sfr[k].idword=2;
                sfr[k].status=TX_REQ;
                for(i=0;i!=4;i++)
                    sfr[k].buf[i]=0;
                sfr[k].buf[4]=rd[0];
                sfr[k].buf[5]=rd[0]>>8;
                sfr[k].buf[6]=rd[1];
                sfr[k].buf[7]=rd[1]>>8;
                retval=true;
            }
            break;
            
        case 0x000B:
            if(getemptysf(&k)){
                sfr[k].idword=0x0B;
                sfr[k].status=TX_REQ;
                rd[0]=ALPTR;
                rd[1]=EVPTR;
                rd[2]=rd[3]=0xFFFF;
                snk=&sfr[k].buf[0];
                for(i=0;i!=4;i++){
                    *snk++=rd[i];
                    *snk++=rd[i]>>8;
                }
            }
            break;
            
        case 0x0100:    //general ID frame
            onpending=false;
            if(getemptymf(&k)){
                mfr[i].dset=seq0100=(seq0100+0x20)&0xE0;
                mfr[i].idword=0x0100;
                mfr[i].length=ldid(&mfdata[i][0]);
                mfr[i].status=TX_REQ;
                retval=true;
            }
            break;
                          
        case 0x0101:    //regular data frame, for transmission on request
            if(getemptymf(&k)){
                mfr[k].dset=seq0101=(seq0101+0x20)&0xE0;
                mfdata[k][0]=gencontrstate;
                mfdata[k][1]=stfeedb.bts[0];
                mfdata[k][2]=usbstat;
                mfdata[k][3]=cancomstate;
                snk=&mfdata[k][4];
                for(i=2;i!=35;i++){
                    *snk++=tx0101.trans[i];
                    *snk++=tx0101.trans[i] >>8;
                }
                mfr[k].idword=0x0101;
                mfr[k].length=70;
                mfr[k].status=TX_REQ;
                retval=true;
            }
            break;
                   
            case 0x0103://retrieving log data. 
                if(getemptymf(&k)){      
                    mfr[k].dset=seq0103=(seq0103+0x20)&0xE0;
                    mfr[k].idword=0x0103;
                    if(ldlog()){
                        mfr[k].length=88;   //length of a log transfer: 2 bytes log counter,4 bytes cal. date,70 bytes as in tx0101,12 bytes as in tx013A,  
                        snk=&mfdata[k][0];
                        *snk++=tx0103.logfields.logcntr;
                        *snk++=tx0103.logfields.logcntr>>8;
                        for(i=1;i!=44;i++){
                            *snk++=tx0103.trans[i];
                            *snk++=tx0103.trans[i]>>8;
                        }
                    }
                    else{  //no more data in memory, send a transfer of 9 bytes all 0xFF and set counter to 0
                        mfr[k].length=16;  
                        for(i=0;i!=16;i++)
                            mfdata[k][i]=0xFF;
                    }
                    mfr[k].status=TX_REQ;
                    retval=true;
                }
                break;
                    
                case 0x0130:    //need to send a mirrored transfer
                    if(getemptymf(&k)){
                        mfr[i].dset=seq0130=(seq0130+0x20)&0xE0;
                        mfr[i].length=138;
                        mfr[i].idword=0x0130;
                        mem2trans(68,0,&mfdata[i][0],false);    //regular non-mirrored frame
                        mfr[i].status=TX_REQ;
                        retval=true;
                    }
                    break;
                    
                case 0x0131:    //send non-mirrored
                    if(getemptymf(&k)){
                        mfr[i].dset=seq0131=(seq0131+0x20)&0xE0;
                        mfr[i].length=40;
                        mfr[i].idword=0x0131;
                        mem2trans(20,0x01AC,&mfdata[i][0],false);
                        mfr[i].status=TX_REQ;
                        retval=true;
                    }
                    break;
                    
                case 0x0138:
                    if(getemptymf(&k)){
                        mfr[k].dset=seq0138=(seq0138+0x20)&0xE0;
                        mfr[k].length=32;
                        mfr[k].idword=0x0138;
                        for(i=0;i!=32;i++)
                            mfdata[k][i]=tx0138.val.gensn[i];      //will use this instead of direct memory transfer, since data may be invalid
                        mfr[k].status=TX_REQ;
                        retval=true;
                    }
                    break;
                    
                case 0x0139:
                    if(getemptymf(&k)){
                        mfr[k].dset=seq0139=(seq0139+0x20)&0xE0;
                        mfr[k].length=24;
                        mfr[k].idword=0x0139;
                        mem2trans(12,0x01E8,&mfdata[k][0],false);
                        mfr[k].status=TX_REQ;
                        retval=true;
                    }
                    break;
                    
            case 0x013A:        //send transfer 0x013A towards panels. 
                if(getemptymf(&k)){
                    mfr[k].dset=seq013A=(seq013A+0x20)&0xE0;
                    mfr[k].length=12;
                    mfr[k].idword=0x013A;
                    snk=&mfdata[k][0];
                    for(i=0;i!=6;i++){
                        *snk++=tx013A.trans[i];
                        *snk++=tx013A.trans[i]>>8;
                    }
                    mfr[k].status=TX_REQ;
                    retval=true;
                }
                break;
                
        case 0x013B:
        case 0x013C:
            if(getemptymf(&k)){
                if(rqtrans==0x013B){
                    adrx=0x0200;        //read first 16 message data
                    mfr[k].dset=seq013B=(seq013B+0x20)&0xE0;
                    mfr[k].idword=0x013B;
                }
                else{                   //read last 16 messages
                    adrx=0x0600;
                    mfr[k].dset=seq013C=(seq013C+0x20)&0xE0;
                    mfr[k].idword=0x013C;
                }
                mfr[k].length=128;
                snk=&mfdata[k][0];
                for(i=0;i!=16;i++){
                    if(readeeprom(adrx+40,rd,4)){
                        adrx+=64;
                        for(h=0;h!=4;h++){
                            *snk++=rd[h];
                            *snk++=rd[h]>>8;
                        }
                    }
                }
                if(!TESTBIT(STATWRDL,0))
                    mfr[k].status=TX_REQ;
            }
            break;
                
                
//no further transfers defined for the VCS                            
                default:
                    break;
            } 
    return(retval);
}

//will initiate a new MFR reception or append to an existing one
void checkmfr(uint16_t transferid,uint8_t src,volatile CANRxMessageBuffer *msg){
    bool tr_pend=true;       //indicates actual transfer is pending
    uint8_t i=0,k;
    while((i!=MFBUFS) && tr_pend){      //check if any mfr transfer is ongoing 
        if(mfx[i]->status==BUSY){    
            if((mfx[i]->idword==transferid)&&(mfx[i]->source==src)&&(msg->data[0]==(mfx[i]->dset+1))){   //only accept this if it is same sequence, same source, next data set
                tr_pend=false;  //this transfer passed. 
                mfx[i]->dset++;
                for(k=1;k!=8;k++)
                    mfdata[i][mfx[i]->wroffs++]=msg->data[k];
                if(mfx[i]->wroffs>=mfx[i]->length)
                    mfx[i]->status=DTA_AVAIL;
            }
            else
                i++;
        }
        else
            i++;
    }
    if(tr_pend && ((msg->data[1] & 0x1F)==0)){//transfer is still pending, did not find a buffer that is already busy with this transfer. Check if this is first frame of a new transfer
        i=0;                                    //will only start a new queue if lower 5 bits of dset are 0. Any other number indicates the transfer is still in progress
        while((i!=MFBUFS) && tr_pend){
            if(mfx[i]->status==IDLE){        //in order to open new MFR channel the least 5 bits of dset are required to be clear
                tr_pend=false;
                mfx[i]->idword=transferid;
                mfx[i]->source=src;
                mfx[i]->length=msg->data[0];
                mfx[i]->dset=msg->data[1];
                mfx[i]->wroffs=6;
                for(k=0;k!=6;k++)
                    mfdata[i][k]=msg->data[2+k];
                mfx[i]->status=BUSY;
                mfx[i]->rxstart=t_1ms;
            }
            else
                i++;
        }
    }
}



//will create a full ID section for an outbound frame. Transfer ID needs to be handed over
//target will be *msg; source will be set to 0; regular frames have priority of 3
void makeeid(CANTxMessageBuffer *msg,uint16_t transferid,uint8_t prio){
    msg->messageWord[0]=(prio<<6)|(transferid>>10);
    msg->messageWord[1]=0x30000008|((((transferid&0x03FF)<<8)|0x00)<<10);
}

//will check if "transfer is declared for the VCS
bool valtrans(uint16_t transfer){
    bool retval=false;
    uint16_t *bf;
    if((transfer>0x013F)&&(transfer<0x160))   //check for service warnings range
        retval=true; 
    else{
        bf=vcstrans;
        do
            if(*bf==transfer)
                retval=true;
        while(!retval&&(*bf++));
    }
    return(retval);
    
}

//this function will return true if the argument is in the single or multi frame array pointed to by *bp
bool inarray(uint8_t idbyte, uint8_t *bp){
    bool retval=false;
    while((!retval)&&(*bp)){
        if(idbyte==*bp++)
            retval=1;
    }
    return(retval);
}

//this function will line up an outbound queue in TX fifo 0. It expects the transfer id and total length in mfrout.
//outbound sequence is expected to be in mfrout.dset (upper 3 bits, lower 3 bits always 0 since transmission starts with sequence 0)
//If the fifo gets full the routine will sent *pending to true. May be called again with *pending still set to true
void txmfr(uint8_t mfrno, bool *pend){
    static uint8_t total,k,mfno,cntr;
    uint8_t ttx,i;
    bool appok=true;
    CANTxMessageBuffer outbf;
    if(*pend)
        makeeid(&outbf,mfr[mfno].idword,3);
    else{
        cntr=k=0;
        mfno=mfrno;
        total=mfr[mfno].length;
        makeeid(&outbf,mfr[mfno].idword,3);
    //prepare first frame
        outbf.data[0]=mfr[mfno].length;
        outbf.data[1]=mfr[mfno].dset;
        for(i=2;i<8;i++)
            outbf.data[i]=mfdata[mfno][k++];
        if(append2fifo1(&outbf)){   //if first package made it through then the transfer becomes pending
            *pend=true;
            total-=6;
            if(cancomstate!=CAN_STATE_BUSON)
                *pend=false;                //break transmission, CAN bus error occurred
        }
        else
            *pend=false;                    //again breal transmission, no free buffer
    }
//keep sending until either all buffers are out or append routine returns false (i.e. FIFO is full    
    while((*pend) && appok){
        ttx=total;
        outbf.data[0]=mfr[mfno].dset|(cntr+1);
        for(i=1;i<8;i++){
            if(ttx--)
                outbf.data[i]=mfdata[mfno][k+i-1];
            else{
                outbf.data[i]=0xFF;     //will fill last frame, unused space, with 0xFF
                ttx=0;
            }
        }
        if(append2fifo1(&outbf)){
            if(cancomstate!=CAN_STATE_BUSON)
                *pend=false;        //bus error, transfer is over
            else{
                cntr++;
                if(total>7)
                    total-=7;
                else
                    *pend=false;
                k+=7;           //increment 7 bytes that just went out
            }
        }
        else
            appok=false;
    }
}

//will look for an sfr buffer in status IDLE. Will return number of first such buffer. Will return 4 if no buffer is empty
bool getemptysf(uint8_t *k){
    bool retval=false;
    *k=0;
    while((!retval)&&(*k<SFBUFS)){
        retval=sfr[*k].status==IDLE;
        *k+=1;
    }
    *k-=1;
    return(retval);
}

//same as above, but for MFR buffers
bool getemptymf(uint8_t *k){
    bool retval=false;
    *k=0;
    while((!retval)&&(*k<MFBUFS)){
        retval=mfr[*k].status==IDLE;
        *k+=1;
    }
    *k-=1;
    return(retval);
}

//will check if from *bf on 4 bytes with valid caldate were sent. If so, calok, caldate and now will be updated
static uint32_t evalcal(uint8_t *bf){
    uint32_t u32x;
    u32x=get4bytes(bf);
    if((u32x!=0xFFFFFFFF)&&(u32x!=0)){
        calok=true;
        now=t_1ms;
        caldate=u32x;
    }
}

//will enforce power down cycle
void forcepwrdwn(void){
    uint32_t bf=0xCC44DD22;
    uint8_t i;
    for(i=0;i!=4;i++){
        sfr[i].status=IDLE;
        mfr[i].status=IDLE;
    }
    sfr[0].idword=2;
    sfr[0].status=TX_REQ;
    for(i=0;i!=4;i++)
        sfr[2].buf[i]=bf++;
    for(i=4;i!=8;i++)
        sfr[2].buf[i]=0xFF;
    stfeedb.fields.pwrdwn=true;
    nocan=1000;
}