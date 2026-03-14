#include "CanCom.h"
#include "CanFunctions.h"
static uint16_t infilter[]={0x0001,0x0002,0x0003,0x0005,0x0007,0x0009,0x000A,0x000B,0x0010,0x0100,0x0130,0x0138,0x013A,0x01B0,0x01B1,0x01B2,0x01B3,0x01B4,0x01B5,0x01B6,0};

static bool canoffon(void);
static void readeidbuf(void);
static void checkmfr(uint16_t transferid,uint8_t src,volatile CANRxMessageBuffer *msg);
static bool valtrans(uint16_t transfer);
static void proctrans(uint16_t transferid,uint8_t src, uint8_t *msg);
static void sendsfr(void);
static bool txsfr(uint8_t sbufno);
static void sendmfr(void);
static void txmfr(uint8_t mfrno, bool *pend);
static void makeeid(CANTxMessageBuffer *msg,uint16_t transferid,uint8_t prio);
static bool append2fifo0(CANTxMessageBuffer *msg);
static bool append2fifo1(CANTxMessageBuffer *msg);
static void killorphans(void);
static bool getemptysf(uint8_t *bn);
static bool getemptymf(uint8_t *bn);
static void resmem(uint16_t respat);
static void res2def(void);
static void rescumulative(void);
static uint8_t ldid(uint8_t *dta);
static void store1bx(uint8_t *inbuf,TXB3232 *tx,uint16_t eeadr);


void CANCOM_Tasks (void){
    static uint32_t tbus,tr101; 
    switch (cancomstat){
        case CAN_STATE_INIT:
            if(!canoffon())
                cancomstat=CAN_STATE_HWIRESP;
            else if(!initcan())
                cancomstat=CAN_STATE_HWIRESP;
            else{
                cancomstat=CAN_STATE_BUSOFF;
                tbus=t_1ms;
            }
            break;
            
        case CAN_STATE_BUSOFF:
                if((t_1ms-tbus)>CANOFF)
                    cancomstat=CAN_STATE_BUSON;
            break;
                            
        case CAN_STATE_BUSON:
            if(!stat_bits.pwrdwn)
                readeidbuf();   
            if(((t_1ms-tr101)>TR101RATE) && (sysstat>SYS_CONNECT))
                if(txtrans(0x0101,NULL))
                    tr101=t_1ms;

            if(((t_1ms-tr13A)>TR13ARATE)&& (sysstat>SYS_CONNECT))
                if(txtrans(0x013A,NULL))
                    tr13A=t_1ms;

            killorphans();
            sendsfr();
            sendmfr();
            break;
            
        case CAN_STATE_HWIRESP: 
            if((t_1ms-tbus)>CANDELAY)
                cancomstat=CAN_STATE_INIT;
            break;
            
        case CAN_STATE_RXONLY:
            while(canrxptr!=canprocptr){
                nocan=0;
                canprocptr++;
                canprocptr&=0x3F;
            }
            break;
            
        case CAN_STATE_RESET:
            C1CONbits.ON=false;
            tbus=t_1ms;
            cancomstat=CAN_STATE_HWIRESP;
            break;
            
        default:
            cancomstat=CAN_STATE_INIT;
            break;
    }
}

//this routine will turn off the CAN bus if it was on. If it is not successful it will return false
bool canoffon(void){
    uint32_t tmrx;
    tmrx=t_1ms;
    C1CONbits.ON=false;
    while(C1CONbits.CANBUSY && ((t_1ms-tmrx)>10));
    if(C1CONbits.CANBUSY)
        return(false);
    else{
        C1CONbits.ON=true;
        tmrx=t_1ms;
        C1CONbits.REQOP=4;
        while(((t_1ms-tmrx)<=10)&&(C1CONbits.OPMOD!=C1CONbits.REQOP));
        if(C1CONbits.OPMOD!=C1CONbits.REQOP)
            return(false);
        else
            return(true);
    }
}

bool initcan(void){
    uint32_t tmrx;
    uint8_t i;
    LATDCLR=0x0020;
   //will now turn CAN module 1 back on and then prepare interrupts
    C1CONSET=0x0010A000;	//Enable time-stamp capture, turn on module and set SIDLE bit
    C1INTCLR=0xF80FF80F;	//clear all interrupts
    C1INTSET=0x70020000;	//enable receive(RBIE), System err.(SERRIE) CAN bus err. (CERRIE) and wake up interrupts

    //enable RX not empty interrupt for FIFO 2 (RX FIFO)
    C1FIFOINT2SET=0x10000;
   
    //CAN ISR priority already set in cpusetup())
    IFS1bits.CAN1IF=0;
    IEC1bits.CAN1IE=1;	//enable CAN interrupts;

    C1CONbits.REQOP=0b100;      //place CAN into configuration mode for further setup
    while(C1CONbits.OPMOD!=0b100);
     //set up for correct bit timing: 250kbit/s, with 10 time quanta per bit, FTQ is 2.5MHz
    C1CFG=0;                //reinit to zero, 1 sample point only
    C1CFGbits.SEG2PHTS=1;	//segment 2 freely programmable
    C1CFGbits.SEG2PH=2;		//phase segment 2 is 3 TQ
    C1CFGbits.SEG1PH=2;		//same as phase segment 1
    C1CFGbits.PRSEG=2;		//propagation delay ist 3 TQ
    C1CFGbits.SJW=1;		//SJW=2 TQ
    C1CFGbits.BRP=15;		//Prescale FSYS by 16, with an additional prescaler of 2 this results in 400ns per TQ
    C1FIFOBA=KVA_TO_PA(C1FiFoBuff);         //assign physical address to buffer base address

    //now initialize all buffers
    C1FIFOCON0=C1FIFOCON1=0x001F0080;        //All TX FIFO set for max. size (32 messages)
    C1FIFOCON0bits.TXPRI=0b11;               //FIFO 0 has highest priority
    C1FIFOCON1bits.TXPRI=0b10;               //FIFO 1 has lower priority
    C1FIFOCON2=0x001F2000;                   //RX FIFO set for max. size, priority 0, DONLY cleared
    
//message filtering: This CAN bus is set up for FP CAN bus communication using extended data format.
//the filter will pass through all messages since only 2 messages are of no concern to the VCS (address gathering). 
//thus the SW will only filter for extended messages (will not let pass standard messages)

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
        cancomstat=CAN_STATE_INIT;
        return(true);
    }
}

//will create a full ID section for an outbound frame. Transfer ID needs to be handed over
//target will be *msg; source will be set to canadr; regular frames have priority of 3
void makeeid(CANTxMessageBuffer *msg,uint16_t transferid,uint8_t prio){
    msg->messageWord[0]=(prio<<6)|(transferid>>10);
    msg->messageWord[1]=0x30000008|((((transferid&0x03FF)<<8))<<10);       //source address is 0
}

bool getemptymf(uint8_t *bn){//will return pointer to an empty mf buffer. Will return 5 if no buffer is empty
    uint8_t i=0;
    while((i<MFBUFS)&&(mfrx[i]->status!=IDLE))
        i++;
    *bn=i;
    return(*bn<MFBUFS);
}

bool getemptysf(uint8_t *bn){//will return pointer to an empty sf buffer. Will return false if no buffer is empty
    uint8_t i=0;
    while((i<SFBUFS)&&(sfrx[i]->val.status!=IDLE))
        i++;
    *bn=i;
    return(*bn<SFBUFS);
}
bool sendrq(RQHANDLER *rqx){
    uint8_t k;
    if(getemptysf(&k)){
        sfrx[k]->val.idword=0x0001;
        sfrx[k]->val.status=TX_REQ;
        sfrx[k]->val.buf[0]=rqx->source;
        sfrx[k]->val.buf[1]=rqx->transfer;
        sfrx[k]->val.buf[2]=rqx->transfer>>8;
        sfrx[k]->val.buf[3]=sfrx[k]->val.buf[4]=sfrx[k]->val.buf[5]=sfrx[k]->val.buf[6]=sfrx[k]->val.buf[7]=0xFF; 
        rqx->lastissued=t_1ms;
        rqx->status=2;
        return(true);
    }
    else
        return(false);
}

//this function will read and process data from EID FIFO. Inbound data transfers will be assembled 
//into either a multi-frame input buffer or into a single input frame buffer, depending on the ID word. If a new message is detected
//and no buffer is free, then that message will be lost. 
void readeidbuf(void){
    uint8_t i,src;
    uint16_t transferid;
    
    while(canprocptr!=canrxptr){  //will stop processing if some data becomes available. In that case, that data needs to be processed first
        src=canrxbuf[canprocptr].msgEID.EID&0xFF;   //source of this frame
        transferid=((canrxbuf[canprocptr].msgEID.EID>>8)&0x03FF)|((canrxbuf[canprocptr].msgSID.SID&0x3F)<<10);
        transferid;
        if(valtrans(transferid)){ 
            if(transferid<0x0100)
                proctrans(transferid,src,(uint8_t*)&canrxbuf[canprocptr].data[0]);    
            
            else{//will need to check for multiple frame transmission(s) and call for processing of such transmission(s))
                checkmfr(transferid,src,&canrxbuf[canprocptr]);
                for(i=0;i!=MFBUFS;i++){
                    if(mfrx[i]->status==DTA_AVAIL){
                        proctrans(transferid,src,&mfdata[i][0]);
                        mfrx[i]->status=IDLE;
                    }          
                }
            }
        }
        canprocptr++;
    }//while, canprocptr!=canrxptr;
}

//will initiate a new MFR reception or append to an existing one
void checkmfr(uint16_t transferid,uint8_t src,volatile CANRxMessageBuffer *msg){
    bool tr_pend=true;       //indicates actual transfer is pending
    uint8_t i=0,k;
    while((i!=MFBUFS) && tr_pend){      //check if any mfr transfer is ongoing 
        if(mfrx[i]->status==BUSY){    
            if((mfrx[i]->idword==transferid)&&(mfrx[i]->source==src)&&(msg->data[0]==(mfrx[i]->dset+1))){   //only accept this if it is same sequence, same source, next data set
                tr_pend=false;  //this transfer passed. 
                mfrx[i]->dset++;
                for(k=1;k!=8;k++)
                    mfdata[i][mfrx[i]->wroffs++]=msg->data[k];
                if(mfrx[i]->wroffs>=mfrx[i]->length)
                    mfrx[i]->status=DTA_AVAIL;    
            }
            else
                i++;
        }
        else
            i++;
    }
    if(tr_pend && ((msg->data[1] & 0x1F)==0)){//transfer is still pending, did not find a buffer that is already busy with this transfer. Check if this is first frame of a new transfer
        i=0;                                    //will only start a new queue if lower 65 bits of dset are 0.
        while((i!=MFBUFS) && tr_pend){
            if(mfrx[i]->status==IDLE){        //in order to open new MFR channel the least 5 bits of dset are required to be clear
                tr_pend=false;
                mfrx[i]->idword=transferid;
                mfrx[i]->source=src;
                mfrx[i]->length=msg->data[0];
                mfrx[i]->dset=msg->data[1];
                mfrx[i]->wroffs=6;
                for(k=0;k!=6;k++)
                    mfdata[i][k]=msg->data[2+k];
                mfrx[i]->status=BUSY;
                mfrx[i]->rxstart=t_1ms;
            }
            else
                i++;
        }
    }
}

//will check if "transfer is declared for the VCS
bool valtrans(uint16_t transfer){
    bool retval=false;
    uint16_t *bf;
    if(transfer==0)
        retval=false;
    else if((transfer>0x03FF) && (transfer<0x0600))
        return(true);
    else{
        bf=(uint16_t *)(infilter);
        while(!retval && *bf>0){
            if(*bf == transfer)
                retval=true;
            else
                bf+=1;
        }
    }
    return(retval);  
}


bool txtrans(uint16_t rqtrans,uint8_t *bf){
    static uint8_t seq0100,seq0101,seq0103,seq0130,seq0138,seq013A,sequpld[512],seq013f[32],
                    seq01b0,seq0104,seq0106,seq01b1,seq01b2,seq01b3,seq01b4,seq01b5,seq01b6,        //sequence counters for MF transfers
                    tx104ctr;     //Transfer 0x0104 counter. first call will upload latest transfer, then each call will upload the one before
    uint16_t rdbf[111],u16x;
    bool retval=false;
    uint8_t i,k,m,*snk; 
    TXB3232 rwb;
    if((rqtrans>0x013F)&&(rqtrans<0x0160)){  //these are service warnings. 
        if(getemptymf(&i)){    //if busy with other transactions then skip
            mfrx[i]->dset=seq013f[rqtrans-0x0140]=(seq013f[rqtrans-0x0140]+0x20)&0xE0;
            mfrx[i]->idword=rqtrans;
            mfrx[i]->length=64;
            readeeprom(0x0300+(rqtrans-0x0140)*0x40,rdbf,0x40);
            for(k=0;k!=0x40;k++){  
                mfdata[i][2*k]=rdbf[k];
                mfdata[i][2*k+1]=rdbf[k]>>8;
            }     
            mfrx[i]->status=TX_REQ;
            retval=true;
        }
    }
    else if((rqtrans<0x0600)&& (rqtrans>0x03FF)){
        if(getemptymf(&k)){
            retval=true;
            mfrx[k]->status=TX_REQ;
            mfrx[k]->dset=sequpld[rqtrans-0x0400]=(sequpld[rqtrans-0x0400]+0x20)&0xE0;
            mfrx[k]->idword=rqtrans;
            mfrx[k]->length=128;
            readeeprom((rqtrans-0x0400)*0x80,&rwb.b16[0],64);
            for(i=0;i!=128;i++)
                mfdata[k][i]=rwb.bf8[i];
        }
    }
    
    else switch(rqtrans){
        case 0x0002:
            if(getemptysf(&k)){
                retval=true;
                sfrx[k]->val.idword=2;
                sfrx[k]->val.status=TX_REQ;
                if(stat_bits.pwrdwn){
                    sfrx[k]->val.buf[0]=0x22;
                    sfrx[k]->val.buf[1]=0xDD;
                    sfrx[k]->val.buf[2]=0x44;
                    sfrx[k]->val.buf[3]=0xCC;
                }
                else for(i=0;i!=4;i++)
                    sfrx[k]->val.buf[i]=*bf++;
                sfrx[k]->val.buf[4]=sfrx[k]->val.buf[5]=sfrx[k]->val.buf[6]=sfrx[k]->val.buf[7]=0xFF;
            }
            break;
            
        case 0x0003:
            if(getemptysf(&i)){
                sfrx[i]->val.idword=3;
                sfrx[i]->val.status=TX_REQ;
                sfrx[i]->val.buf[0]=tx0003.trans[0];
                sfrx[i]->val.buf[1]=tx0003.trans[0]>>8;
                sfrx[i]->val.buf[2]=tx0003.trans[1];
                sfrx[i]->val.buf[3]=tx0003.trans[1]>>8;

                sfrx[i]->val.buf[4]=caldate.trans;
                sfrx[i]->val.buf[5]=caldate.trans>>8;
                sfrx[i]->val.buf[6]=caldate.trans>>16;
                sfrx[i]->val.buf[7]=caldate.trans>>24;
//unlike manual control of digital outputs, the bit indicating manual operation is dynamic. If it was set when receiving transfer 3 the receiving routine does the mode change and clears the bit. 
//Thus as long as the control system is preparing or running or ending manual operation that bit needs to be sent out as set
                if((gcontstat>=GENCONTR_STATE_PREPMANOP)&& (gcontstat<=GENCONTR_STATE_ENDMANOP))
                    SETBIT(sfrx[i]->val.buf[1],0);
            }
            break;
            
        case 0x0009:
            if(getemptysf(&i)){
                sfrx[i]->val.idword=9;
                sfrx[i]->val.status=TX_REQ;
                rwb.b32[0]=valsn;
                rwb.b32[1]=perssn;
                for(k=0;k!=8;k++)
                    sfrx[i]->val.buf[k]=rwb.bf8[k];
            }
            break;

        case 0x000A:
        case 0x000B:
            if(getemptysf(&k)){
                retval=true;
                sfrx[k]->val.idword=rqtrans;
                sfrx[k]->val.status=TX_REQ;
                if(rqtrans==0x000A)
                    for(i=0;i!=4;i++){
                        sfrx[k]->val.buf[2*i]=tx000A.trans[i];
                        sfrx[k]->val.buf[2*i+1]=tx000A.trans[i]>>8;
                    }
                else{
                    sfrx[k]->val.buf[0]=alptr;
                    sfrx[k]->val.buf[1]=alptr>>8;
                    sfrx[k]->val.buf[2]=evptr;
                    sfrx[k]->val.buf[3]=evptr>>8;
                    sfrx[k]->val.buf[4]=sfrx[k]->val.buf[5]=sfrx[k]->val.buf[6]=sfrx[k]->val.buf[7]=0xFF;
                }
            }
            break;
            
        case 0x0100:    //general ID frame. If multiple output frame is not available then this request will not get answered. 
            if(getemptymf(&k)){  //need to handle inverter requests as well
                mfrx[k]->dset=seq0100=(seq0100+0x20)&0xE0;
                mfrx[k]->idword=0x0100;
                mfrx[k]->length=ldid(&mfdata[k][0]);
                mfrx[k]->status=TX_REQ;
                retval=true;
            }
            break;
            
        case 0x0101:
            if(getemptymf(&k)){
                mfrx[k]->dset=seq0101=(seq0101+0x20)&0xE0;
                mfrx[k]->idword=0x0101;
                mfrx[k]->status=TX_REQ;
                mfrx[k]->length=TR101LEN<<1;
                *statvar=(uint8_t)gcontstat | (st_feedb.trans[0]<<8);
                tx0101.trans_w[11]=batvolt;
                for(i=0;i!=TR101LEN<<1;i++)
                    mfdata[k][i]=tx0101.trans_b[i];
                retval=true;
            }
            break;
            
        case 0x0103:
            if(getemptymf(&k)){
                retval=true;
                mfrx[k]->dset=seq0103=(seq0103+0x20)&0xE0;
                mfrx[k]->idword=0x0103;
                mfrx[k]->status=TX_REQ;
                if(lgreq==200){             //previous request was trying to access an invalid log data memory
                    mfrx[k]->length=0x32;
                    for(i=0;i!=16;i++)
                        mfdata[k][i]=0xFF;      
                }
                else{
                    snk=mfdata[k];
                    mfrx[k]->length=222;
                    readeeprom(0x1000+lgreq*0x0100,rdbf,111);
                    for(i=0;i!=111;i++){        //transfer 103 goes straight from the memory read buffer to the outbound buffer. No specific "transfer 103" is defined as a global structure. This is only used in the documentation for descriptive reasons
                        *snk++=rdbf[i];
                        *snk++=rdbf[i]>>8;
                    }
                }
            }
            break;
            
            case 0x0104:        //will upload one transfer 104 per each request, auto incrementing 0...39. 
            if(getemptymf(&k))
                if(panelx20.nodesw!=0){
                    if(tx104ctr>39)
                        tx104ctr=0;
                    mfrx[k]->dset=seq0104=(seq0104+0x20)&0xE0;
                    mfrx[k]->idword=0x0104;
                    mfrx[k]->length=33;       //16 words plus one byte tx0104ctr
                    mfdata[k][0]=tx104ctr;
                    for(i=0;i!=16;i++){
                        mfdata[k][2*i+1]=tx0104[tx104ctr][i];
                        mfdata[k][2*i+2]=tx0104[tx104ctr][i]>>8;
                    }
                    tx104ctr++;
                    mfrx[k]->status=TX_REQ;
                    retval=true;
                }
            break;
            
        case 0x0106:
            if(getemptymf(&k)){    
                m=192;
                mfrx[k]->idword=0x0106;
                mfrx[k]->status=TX_REQ;      
                mfrx[k]->dset=seq0106=(seq0106+0x20)&0xE0;
                mfrx[k]->length=194;
                retval=true;
                snk=&mfdata[k][0];
                *snk++ = stastoptr;
                *snk++ = stastoptr>>8;
                u16x=stastoptr;
                while((u16x) && (m>5)){      //will now read max 192/6=32 log data sets
                    if(u16x==0x0100)      //log counter just did roll over
                        u16x=0x0020;
                    u16x-=1;      //read last start and last stop
                    m-=6;  
                    readeeprom(0xB64A+(u16x&0x1F)*6,rdbf,3);       
                    for(i=0;i!=3;i++){
                        *snk++=rdbf[i];
                        *snk++=rdbf[i]>>8;
                    }
                }
                while(m){  //fill up remaining space
                    *snk++=0xFF;
                    m--;
                }
            }
            break;
            
        case 0x0130:
            if(getemptymf(&k)){
                mfrx[k]->dset=seq0130=(seq0130+0x20)&0xE0;
                mfrx[k]->idword=0x0130;
                mfrx[k]->status=TX_REQ;
                mfrx[k]->length=TR130LEN<<1;
                for(i=0;i!=TR130LEN;i++){
                    mfdata[k][2*i]=tx0130.trans_w[i];
                    mfdata[k][2*i+1]=tx0130.trans_w[i]>>8;
                }
                retval=true;
                tr13A=t_1ms;
            }
            break;
            
        case 0x0138:
            if(getemptymf(&k)){
                mfrx[k]->dset=seq0138=(seq0138+0x20)&0xE0;
                mfrx[k]->idword=0x0138;
                mfrx[k]->status=TX_REQ;
                mfrx[k]->length=64;
                for(i=0;i!=32;i++){
                    mfdata[k][2*i]=tx0138.trans[i];  //transmit as ASCII, big endian
                    mfdata[k][2*i+1]=tx0138.trans[i]>>8;
                }
                retval=true;
            }
            break;
            
            
        case 0x013A:
            if(getemptymf(&k)){
                mfrx[k]->dset=seq013A=(seq013A+0x20)&0xE0;
                mfrx[k]->idword=0x013A;
                mfrx[k]->status=TX_REQ;
                mfrx[k]->length=12;
                for(i=0;i!=6;i++){
                    mfdata[k][2*i]=tx013A.trans[i];
                    mfdata[k][2*i+1]=tx013A.trans[i]>>8;
                }
                retval=true;
            }
            break;
            
        case 0x01B0:    //Next trigger message, bits 0..15
            case 0x01B1:    //Next trigger message, bits 0..15
            case 0x01B2:    //previous to last reset op time / date for bits 0..15
            case 0x01B3:    //previous to last reset op time/ date for bits 16..31
            case 0x01B4:    //last reset op time / date for bits 0..15
            case 0x01B5:    //last reset op time / date for bits 16..31
                if(getemptymf(&k))
                    if(!TESTBIT(*miscwordlow,4) || (panelx20.nodesw == 0xFFFFFFFF)){
                        retval=true;
                        mfrx[k]->status=TX_REQ;              
                        mfrx[k]->idword=rqtrans;
                        mfrx[k]->length=128;
                        if(rqtrans==0x01B0){
                            mfrx[k]->dset=seq01b0=(seq01b0+0x20)&0xE0;
                            snk=&tx01B0.bf8[0];
                            x01B0.status=0;
                        }
                        else if(rqtrans==0x01B1){
                            mfrx[k]->dset=seq01b1=(seq01b1+0x20)&0xE0;
                            snk=&tx01B1.bf8[0];
                            x01B1.status=0;
                        }
                        else if(rqtrans==0x01B2){
                            mfrx[k]->dset=seq01b2=(seq01b2+0x20)&0xE0;
                            snk=&tx01B2.bf8[0];
                            x01B2.status=0;
                        }
                        else if(rqtrans==0x01B3){
                            mfrx[k]->dset=seq01b3=(seq01b3+0x20)&0xE0;
                            snk=&tx01B3.bf8[0];
                            x01B3.status=0;
                        }
                        else if(rqtrans==0x01B4){
                            mfrx[k]->dset=seq01b4=(seq01b4+0x20)&0xE0;
                            snk=&tx01B4.bf8[0];
                            x01B4.status=0;
                        }
                        else if(rqtrans==0x01B5){
                            mfrx[k]->dset=seq01b5=(seq01b5+0x20)&0xE0;
                            snk=&tx01B5.bf8[0];
                            x01B5.status=0;
                        }
                        for(i=0;i!=128;i++)
                            mfdata[k][i]=*snk++;
                    }
                break;
                    
            case 0x01B6:    
                if(getemptymf(&k))
                    if(!TESTBIT(*miscwordlow,4) || (panelx20.nodesw == 0xFFFFFFFF)){
                        retval=true;
                        mfrx[k]->dset=seq01b6=(seq01b6+0x20)&0xE0;
                        mfrx[k]->status=TX_REQ;              
                        mfrx[k]->idword=rqtrans;
                        mfrx[k]->length=14;
                        for(i=0;i!=5;i++){
                            mfdata[k][2*i]=tx01B6.trans[i];
                            mfdata[k][2*i+1]=tx01B6.trans[i]>>8;
                        }
                    }
                break;
                    
        default:
            break;
    }
    return(retval);
}

uint8_t ldid(uint8_t *dta){
    uint8_t *src,i;
    uint32_t fxx;
    fxx=SWVER;

    for(i=0;i!=4;i++){
        *dta++=fxx;
        fxx>>=8;
    }
    fxx=tx000A.trans[2]|(tx000A.trans[3]<<16);
    for(i=0;i!=4;i++){
        *dta++=fxx;
        fxx>>=8;
    }  
    *dta++=PROTREL;
    *dta++=PROTREL>>8;
    *dta++=TR101LEN;
    *dta++=TR130LEN;
        
    i=12;
    if(hwtype==UNKNOWN)
        src=(uint8_t *)unknown;
    else if(hwtype==ISC004)
        src=(uint8_t *)verc04;
    else if(hwtype==ISC005)
        src=(uint8_t *)verc05;
    while(*dta++=*src++)
        i++;
    *dta=0;     //string needs to be zero terminated
    i++;
    return(i);
}

void  proctrans(uint16_t transferid,uint8_t src, uint8_t *msg){
    static uint8_t reason;
    static uint32_t updreason;
    static bool alon;
    uint32_t tx13as[3];
    uint16_t rqtrans,u16x,bfin[TR130LEN];
    uint8_t i,*bf;
    CANNODE *nde;
    TXB3232 rwb;
    bool ok,remcommand;
    stat_bits.onpending=false;
    if(src>0x1F){
        lastuserint=t_1ms;
        nocan=0;
        stat_bits.gotgp203=true;
    }
    bf=NULL;
    if((transferid>0x03FF) && (transferid<0x0600)){ //this is a direct write to Memory
        for(i=0;i!=128;i++)
            rwb.bf8[i]=*msg++;
        writeeeprom((transferid-0x0400)*0x80,&rwb.b16[0],64);
    }
    else if((transferid>0x013F)&&(transferid<0x0160)){//got a service notification
        for(i=0;i!=0x40;i++)
            rwb.bf8[i]=*msg++;      //will only program SN at this time. will calculate trigger and re-trigger times when activating warranty   
        while(!writeeeprom(0x0300+(transferid-0x0140)*0x40, rwb.b16,0x40));
    }
        
    else switch(transferid){    
        case 0x0001:    //general frame request
            if(*msg++ ==0){
                rqtrans=*msg++;
                rqtrans|=(*msg++)<<8;
                txtrans(rqtrans,NULL);
            }
            break;
        
        case 0x0002:
            if(get4bytes(&msg)==0xCC44DD22)
                stat_bits.pwrdwn=true;
            break;
            
        case 0x0003:
            tx0003.trans[0]=*msg++;
            tx0003.trans[0]|=*msg++ <<8;
            tx0003.trans[1]=0;
            tx0003.trans[1]=*msg++;     //upper 7 bits either not used at all or used in inverter only. disregard inside IDA

                if(TESTBIT(*genconfl,8)){      //if rem. start on panel is static      
                    if(!tx0003.val.staticstat){
                        CLEARBIT(*stwordhigh,6);
                        CLEARBIT(*stwordhigh,5);
                        if(st_feedb.val.actstat || (gcontstat==GENCONTR_STATE_UNACKAL) || st_feedb.val.alpending)
                            remcommand=tx0003.val.startstop=true;  //requested a stop condition
                    }
                    else if(!st_feedb.val.actstat){
                        if(st_feedb.val.startinhib || (gcontstat==GENCONTR_STATE_UNACKAL) || TESTBIT(*miscwordlow,0))
                            SETBIT(*stwordhigh,6);
                        else if(!TESTBIT(*stwordhigh,6))
                            remcommand=tx0003.val.startstop=true;  //requested a start
                    }
                }
                caldate.trans=get4bytes(&msg);
                //handle manual operation 
                if(tx0003.val.manop){       //will either enter manual operation or exit
                    tx0003.val.manop=st_feedb.val.newin=false;
                    if((gcontstat==GENCONTR_STATE_MANOP)||(gcontstat==GENCONTR_STATE_PREPMANOP))
                        gcontstat=GENCONTR_STATE_ENDMANOP;
                    else if(gcontstat==GENCONTR_STATE_INSTOP){
                        gcontstat = GENCONTR_STATE_PREPMANOP;
                        st_feedb.val.statustime=t_1ms;
                    }
                }
                else if(tx0003.val.testmode){
                    tx0003.val.testmode=st_feedb.val.newin=false;
                    if((gcontstat==GENCONTR_STATE_DRYTEST)||(gcontstat==GENCONTR_STATE_PREPDRY))
                        gcontstat=GENCONTR_STATE_ENDMANOP;  //same status for ending dry test or manual operation
                    else if(gcontstat==GENCONTR_STATE_INSTOP){
                        gcontstat = GENCONTR_STATE_PREPDRY;
                        st_feedb.val.statustime=t_1ms;
                    }
                }
                
                
//now check for start/stop and manual control in this order. Above  if-loop will prevent entering start/stop from within manual control
//the SW however must not process start/stop commands if it is in test mode. Test mode h    
                else if(tx0003.val.startstop){
                    tx0003.val.manop=tx0003.val.startstop=tx0003.val.testmode=false; //clear all toggle bits, start-stop has highest priority
                    if(st_feedb.val.actstat || (gcontstat==GENCONTR_STATE_UNACKAL) || TESTBIT(*miscwordlow,0)){            //bit 0 will toggle actual status: off->on->cool down->off. will need to accept multiple stop calls if in "unacknowledged alarms" status
                        if((gcontstat==GENCONTR_STATE_INRUN) && TESTBIT(*genconfl,5) && !remcommand){ //skip cool-down in case of static stop
                            gcontstat=GENCONTR_STATE_COOLDWN;
                            st_feedb.val.statustime=t_1ms;
                        }
                        else{   
                            st_feedb.val.req_in=false;
                            st_feedb.val.newin=true;
                        }  
                        if(tx0003.val.staticstat)
                            SETBIT(*stwordhigh,5);  //command was issued manually, signal start/stop conflict
                    }
                    else if(st_feedb.val.alpending){
                        st_feedb.val.newin=true;        
                        st_feedb.val.req_in=false;
                    }
                    else if(!st_feedb.val.startinhib){    //will not allow start if some alarm is pending
                        st_feedb.val.req_in=true;
                        st_feedb.val.newin=true;
                    }
                }
                else if(gcontstat==GENCONTR_STATE_MANOP){
                    st_feedb.val.newin=true;            //enforce updating the outputs
                }
            break;
                
        case 0x0005: //Panel sending status bits, update pattern, timer and RTC if OK, Sysjobs handles status bits
            lastrx0005=t_1ms;
            if((src>0x1F)&&(src<0x28)){
                stat_bits.gotgp203=true;
                panels[src-0x20].sourceadr=src;
                panels[src-0x20].ext_contr=get4bytes(&msg);
                panels[src-0x20].lastcom=t_1ms;
                if(TESTBIT(panels[src-0x20].ext_contr,1)){
                    caldate.trans=get4bytes(&msg);
                    now=t_1ms;
                }
                if(src == 0x20){
                    if(TESTBIT(panels[src-0x20].ext_contr,2))
                        stat_bits.nocmdata=true;
                    else
                        stat_bits.nocmdata=false;
                }
//logging start and stop is entirely under the panel control and is based on the assumption that the panel will report a certain start or stop reason for max. 20ms. So within a time frame of 20s from receiving the 
//first start or stop reason and until that reason did not change the start or stop log will be updated just once. Outside of this function only alarm shutdowns will log stops. 
//if the Stop was triggered by an alarm then the panel input will be ignored. To avoid an overlapping of panel input and alarm stop the panel input will be ignored if an alarm was logged and for the first ack. after such incident 
                i=(panels[src-0x20].ext_contr>>8)&0xFF;     //extract current reason
                
                if(*stastoreas==(0x80|ALSTOP))
                    alon=true;
                else if(alon){
                    if((i==BUTTSTO) || (i==STATSTO) || (i==DYNSTO) || (i==NMEASTO)){
                        alon=false;
                        reason=i;
                        updreason=t_1ms;
                    }
                }
                else if(i){
                    if(i!=reason){
                        reason=i;
                        logstasto(reason,src-0x20);
                        updreason=t_1ms;
                        showstasto=t_1ms;
                        *stastoreas=0x80|reason;
                    }
                }
                if((t_1ms-updreason)>20000)
                    reason=0; 
            }
            if(src==rq0005.source)
                rq0005.status=0;
            break;  
                
                
        case 0x0007:
            u16x=*msg++;
            u16x|=*msg++ <<8;
            if(u16x & 0xC9)
                resmem(u16x & 0xC9);
            else if(TESTBIT(u16x,1))
                res2def();
            else if(TESTBIT(u16x,2))
                rescumulative();
            else if(TESTBIT(u16x,5))
                writeeeprom(0x00D0,tx0130.trans_w,TR130LEN-2);  //make current data default
            break;
            
        case 0x0009:
            updreason=get4bytes(&msg);
            caldate.trans=get4bytes(&msg);
            if(chkvalrtc(&caldate))        //do not reset if the date in the panel is invalid!
                for(i=0;i!=32;i++)
                    if(TESTBIT(updreason,i))
                        ressrvnot(i);
            break;
            
            
        case 0x000A:
            if(*msg++ ==0){ //caller addressing the engine controller
                msg+=3;     //first 3 bytes after node address not used
                u16x=*msg++;
                u16x|=*msg++ <<8;
                rqtrans=*msg++;
                rqtrans|=*msg++ <<8;
                if((u16x!=tx000A.val.snl) || (rqtrans != tx000A.val.snh)){
                    tx000A.val.snl=u16x;
                    tx000A.val.snh=rqtrans;
                    writeeeprom(0x1A4,&tx000A.trans[2],2);
                } 
            }
            break;
            
        case 0x000B:
            rq000B.status=0;
            msg+=4;
            lgreq=*msg++;
            msg+=1; 
            if(lgreq>149)
                lgreq=200;      //will indicate invalid read request
            else if(lgreq<75){
                if(lgreq>alptr)
                    if(!TESTBIT(alptr,8))   //bit 8 of pointer indicates that a roll-over did occur
                        lgreq=200;          //invalid request if attempting to read beyond log pointer without previous overflow
            }
            else{
                if((lgreq-75)>evptr)
                    if(!TESTBIT(evptr,8))
                        lgreq=200;          //invalid request if attempting to read beyond log pointer without previous overflow
            }
            break;
            
        case 0x0010:
            if(*msg++ ==0){//check if engine controller board is addressed{
                prepsoftres();
                cpureset();
            }
            break;
 
        case 0x0100:
            rq0100.status=0;
            stat_bits.gotgp203=true;
            if(src==0x20){
                nde=&panelx20;
                bf=&panelx20.nodetype[0];
                stat_bits.gotgp203=true;
                nde->nodesw=get4bytes(&msg);
                nde->nodesn=get4bytes(&msg);
                nde->protrel=*msg++;
                nde->protrel=*msg++<<8;
                nde->tx10xlen=*msg++;
                nde->tx13xlen=*msg++;
                while(*bf++ = *msg++);
            }
            if((src>0x20) && (src<0x28))
                panels[src-0x20].pansw=get4bytes(&msg);
            break;
            
        case 0x0130:
            for(i=0;i!=TR130LEN;i++){
                bfin[i]=*msg++;
                bfin[i]|=*msg++ <<8;
            }
            if(checkmirr(bfin)){     //will have to save data, to default or regular, just as was requested with previous transfer 0x0003
                CLEARBIT(*stwordlow,2);
                writeeeprom(0x1A0,&bfin[TR130LEN-2],2);
                for(i=0;i!=TR130LEN;i++)
                    tx0130.trans_w[i]=bfin[i];
                if(tx0003.val.rqsavnorm)
                    writeeeprom(0,bfin,TR130LEN-2);
                if(tx0003.val.rqsavdef)
                    writeeeprom(0x0D0,bfin,TR130LEN-2);
                caldate.trans=bfin[TR130LEN-2] | (bfin[TR130LEN-1] <<16);
                tx0003.val.rqsavdef=tx0003.val.rqsavdef=false;
                bfin[0]=SWVER&0xFFFF;
                bfin[1]=SWVER>>16;
                writeeeprom(0x01FC,bfin,2);
                writecal(0x01A0);
            }
            break;
            
        case 0x0138:
            rq0138.status=0;
            for(i=0;i!=32;i++)
                tx0138.val.gensn[i]=*msg++;
            for(i=0;i!=32;i++)
                tx0138.val.fireboy[i]=*msg++;
            writeeeprom(0x01A8,tx0138.trans,32);
            break;

        case 0x013A:
            rq013A.status=0;
            ok=true;
            for(i=0;i!=3;i++){
                tx13as[i]=get4bytes(&msg);
                if(tx13as[i]>9999998)
                    ok=false;
            }
            if(ok || (src==0x28) || (src==0x10)){       //accept data either if it is valid or anything if the source is CM210 or test device
                ok=false;
                if(tx013A.v32.optime!=tx13as[0])
                    ok|=true;
                if(tx013A.v32.starts!=tx13as[1])
                    ok|=true;
                if(tx013A.v32.energy!=tx13as[2])
                    ok|=true;
                if(ok){
                    tx013A.v32.optime=tx13as[0];
                    tx013A.v32.starts=tx13as[1];
                    tx013A.v32.energy=tx13as[2];
                    writeeeprom(0x01E8,tx013A.trans,6);
                }    
            } 
            break;
            
        case 0x01B0:
            if(rq01B0.source ==src){    //avoid loading from anything else than a VCS
                rq01B0.status=0;
                store1bx(msg,&tx01B0,0x0308);
            }
            break;
            
           
        case 0x01B1:
            if(rq01B1.source == src){
                rq01B1.status=0;
                store1bx(msg,&tx01B1,0x0708);
            }
            break;
            
        case 0x01B2: 
            if(rq01B2.source ==src){
                rq01B2.status=0;
                store1bx(msg,&tx01B2,0x0310);
            }
            break;
            
        case 0x01B3: 
            if(rq01B3.source ==src){
                rq01B3.status=0;
                store1bx(msg,&tx01B3,0x0710);
            }
            break;
            
        case 0x01B4: 
            if(rq01B4.source ==src){
                rq01B4.status=0;
                store1bx(msg,&tx01B4,0x0318);
            }
            break;
            
        case 0x01B5: 
            if(rq01B5.source ==src){
                rq01B4.status=0;
                store1bx(msg,&tx01B5,0x0718);
            }
            break;

        case 0x01B6:
            if(rq01B6.source==src)
                rq01B6.status=0;
            for(i=0;i!=5;i++){
                tx01B6.trans[i]=*msg++;
                tx01B6.trans[i] |= *msg++ <<8;
            }
            writeeeprom(0x02B0,tx01B6.trans,5);
            break;
            
        default:
            break;

    }
}


void resmem(uint16_t respat){
    uint16_t i,bf[64];
    for(i=0;i!=64;i++)
        bf[i]=0xFFFF;
    if(TESTBIT(respat,0)){      //reset log data
        lgreq=alptr=evptr=0;
        for(i=0;i!=600;i++)
            while(!writeeeprom(0x1000+0x40*i,bf,64));   //regular log data and alarm log data
        for(i=0;i!=20;i++)
            while(!writeeeprom(0x0B00+0x40*i,bf,64));//fast log data 
        bf[0]=bf[1]=0;
        while(!writeeeprom(0x0200,bf,2));       //alarm and log data pointer
    }
    else if(TESTBIT(respat,3))      //service notifications
        for(i=0;i!=32;i++)
            while(!writeeeprom(0x0300+i*64,bf,32));
    else if(TESTBIT(respat,6))     //erase entire memory, 0x0000 to 0xFFFF
        for(i=0;i!=1024;i++)
            while(!writeeeprom(i*64,bf,64));
    
    else if(TESTBIT(respat,7)){  //make this device "not programmed
        for(i=0;i!=TR130LEN;i++)
            tx0130.trans_w[i]=0xFFFF;
        while(!writeeeprom(0,tx0130.trans_w,TR130LEN));
    }
}

void res2def(void){
    uint16_t bf[TR130LEN-2];
    if(readeeprom(0xD0,bf,TR130LEN-2)){
        if(checkmirr(bf)){
            writeeeprom(0,bf,TR130LEN-2);
            prepsoftres();
            cpureset();
        }
    }
}

void rescumulative(void){
    uint8_t i;
    for(i=0;i!=6;i++)
        tx013A.trans[i]=0;
    writeeeprom(0x01E8,tx013A.trans,6);
    writeeeprom(0x01F4,tx013A.trans,1);   //clear energy remainder as well;
}


//this function is used to receive and store buffers 1B0 to 1B5
void store1bx( uint8_t *inbuf,TXB3232 *tx,uint16_t eeadr){
    uint8_t i;
    for(i=0;i!=128;i++)
        tx->bf8[i]=*inbuf++;
    for(i=0;i!=16;i++)
        writeeeprom(eeadr+i*0x40,&tx->b16[4*i],4);
    validatesvns();
}

void killorphans(void){
    uint8_t i;
    for(i=0;i!=MFBUFS;i++)
        if(mfrx[i]->status==BUSY)
            if((t_1ms-mfrx[i]->rxstart)>RXMAX)
                mfrx[i]->status=IDLE;
}


//this function will line up an outbound queue in TX fifo 0. It expects the transfer id and total length in mfrout.
//outbound sequence is expected to be in mfrout.dset (upper 3 bits, lower 3 bits always 0 since transmission starts with sequence 0)
//If the fifo gets full the routine will set *pend to true. May be called again with *pending still set to true
void txmfr(uint8_t mfrno, bool *pend){
    static uint8_t total,i,*bf,cntr;
    static CANMFR *mf;
    bool sending=true;
    uint8_t ttx,*blast;
    CANTxMessageBuffer outbf;
    if(! (*pend)){
        cntr=0;
        mf=mfrx[mfrno];
        bf=&mfdata[mfrno][0];
        total=mf->length;
        makeeid(&outbf,mf->idword,3);
    //prepare first frame
        outbf.data[0]=mf->length;
        outbf.data[1]=mf->dset;
            for(i=2;i<8;i++)
                outbf.data[i]=*bf++;
        if(append2fifo1(&outbf)){   //if first package made it through then the transfer becomes pending
            *pend=true;
            total-=6;
            if(cancomstat!=CAN_STATE_BUSON)
                *pend=false;                //break transmission, CAN bus error occurred
        }
    }
    else
        makeeid(&outbf,mf->idword,3);
        
//keep sending until either all buffers are out or append routine returns false (i.e. FIFO is full    
    while(*pend && sending){
        ttx=total;
        outbf.data[0]=mf->dset|(cntr+1);
        blast=bf;
        for(i=1;i<8;i++){
            if(ttx--)
                outbf.data[i]=*bf++;
            else{
                outbf.data[i]=0xFF;     //will fill last frame, unused space, with 0xFF
                ttx=0;
            }
        }
        if(append2fifo1(&outbf)){
            if(cancomstat!=CAN_STATE_BUSON)
                *pend=false;        //bus error, transfer is over
            else{
                cntr++;
                if(total>7)
                    total-=7;
                else
                    *pend=false;
            }
        }
        else{
            sending=false;
            bf=blast;
        }
    }
}

//this function will transmit sfrout. It assumes the buffer is ready for transmission, particularly unused bytes need to be set to 0xFF
//and IDword to be set to actual transfer ID. Source will be default set to zero, since this is the VCS
bool txsfr(uint8_t sbufno){
    CANTxMessageBuffer outbf;
    CANSFR *sf;
    sf=sfrx[sbufno];
    makeeid(&outbf,sf->val.idword,3);
    outbf.messageWord[2]=sf->val.buf[0]|(sf->val.buf[1]<<8)|(sf->val.buf[2]<<16)|(sf->val.buf[3]<<24);
    outbf.messageWord[3]=sf->val.buf[4]|(sf->val.buf[5]<<8)|(sf->val.buf[6]<<16)|(sf->val.buf[7]<<24);
    return(append2fifo0(&outbf));
}

//this function will try to append a message to a CAN Tx FIFO 1 (lower prio FIFO)
//If no space is free it will keep trying for tmax miliseconds
//It will return true if message got appended, otherwise false
bool  append2fifo1(CANTxMessageBuffer *msg){
    CANTxMessageBuffer *buffapend;
    if(C1FIFOINT1bits.TXNFULLIF){
        buffapend=PA_TO_KVA1(C1FIFOUA1);
        *buffapend=*msg;
        C1FIFOCON1SET=0x2000;   //set UINC
        C1FIFOCON1SET=0x08;     //set transmit request
        return(true);
    }//if fifo 1
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

//this function will SFR's if required and as long as the buffer is available
void sendsfr(void){
    uint8_t i=0;
    while(i<SFBUFS){
        if(sfrx[i]->val.status==TX_REQ){
            if(txsfr(i))
                sfrx[i]->val.status=IDLE;
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
    static uint8_t pendbuf;
    if(pending){    //previous MFR was not fully sent, try again
        if((t_1ms-tmrx)>MAXFIFOFULL)
            cancomstat=CAN_STATE_INIT;      //transfer needs to be done in 500ms or a bus error will be triggered
        else if(C1FIFOINT1bits.TXEMPTYIF){
            txmfr(pendbuf,&pending);      //buffer No is irrelevant if last transfer was pending
            if(!pending)
                mfrx[pendbuf]->status=IDLE;
        }
    }
    else{
        pendbuf=0;
        while((pendbuf<MFBUFS) && !pending){
            if(mfrx[pendbuf]->status==TX_REQ){
                txmfr(pendbuf,&pending);
                if(pending)
                    tmrx=t_1ms;
                else
                    mfrx[pendbuf]->status=IDLE;
            }
            if(!pending)
                pendbuf++;
        }
    }
}