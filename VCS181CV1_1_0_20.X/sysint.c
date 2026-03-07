#include "StateDefinitions.h"
#include "Defines.h"

//#define UONLY

static uint32_t calladc[]={0x00018000,0x00019000,0x0001A000,0x0001B000,0x0001C000,0x0001D000,0x0001E000,0x001F000};   //these variables used to start ADC

static void chleds(void);
static void nextchan(void);
static void sampon(void);
static void nextcx(void);


void __ISR(_CHANGE_NOTICE_VECTOR, IPL1SOFT) _IntHandlerChangeNotification(void){
    uint16_t pd;
    asm("ei");
    pd=PORTD;           //resolve interrupt mismatch
    IEC1bits.CNIE=0;    //will no longer need this interrupt, low power mode will turn it on
    IFS1bits.CNIF=0;
}

void __ISR(_TIMER_3_VECTOR,IPL5SOFT) T3InterruptHandler(void){  //Timer 3, once every 9.5seconds
    asm("ei");
    t3ovr++;
    IFS0bits.T3IF=false;
}


void __ISR(_TIMER_4_VECTOR,IPL4SRS) T4InterruptHandler(void){
    static uint16_t itms;
    static bool up;
    asm("ei");
#if defined(TESTLINMOT)
    static uint16_t actstep;
    if(++actstep>100){
        actstep=0;
        OC2RS=0;
//        if(up){
//            if(OC2RS<7500)
//                OC2RS+=500;
//            else
//                up=false;
//        }
//        else{
//            if(OC2RS>500)
//                OC2RS-=500;
//            else
//                up=true;
//        }
    }
#endif
    t_1ms++;
    if(++itms==1000){   
        t_1s++;         
        itms=0;
        nocan++;
    }
    if(!(t_1ms%600))
        chleds();
    IFS0bits.T4IF=false;
}


void __ISR(_INPUT_CAPTURE_1_VECTOR, ipl3SOFT) _IntHandlerDrvUpuls(void){
    uint32_t act;
    asm("ei");
    if(IC1CONbits.ICOV){
        while(IC1CONbits.ICBNE)
            act=IC1BUF;
        isrcom.Bits.lastupuls=false;
    }
    else{
        act=IC1BUF|(t3ovr<<16);
        if(isrcom.Bits.lastupuls){      //may recalculate period, last rising edge was valid
            isrcom.Bits.period=act-isrcom.Bits.lastutime;   //period in units of 1600ns
            isrcom.Bits.gotfreq=true;
        }
        isrcom.Bits.lastupuls=true;
        isrcom.Bits.lastutime=act;
    }
    IFS0bits.IC1IF=false;
}


void __ISR(_INPUT_CAPTURE_2_VECTOR, ipl3SOFT) _IntHandlerDrvIpuls(void){//this is the current signal
    uint32_t act;
    asm("ei");
    if(IC2CONbits.ICOV){
        while(IC2CONbits.ICBNE)
            act=IC2BUF;
        isrcom.Bits.gotpf=false;
    }
    else{
        act=IC2BUF|(t3ovr<<16);
        if(isrcom.Bits.lastupuls){//will not attempt to calculate phase shift if no valid voltage signal was detected
            isrcom.Bits.phshift=act-isrcom.Bits.lastutime;
            if(isrcom.Bits.phshift<(isrcom.Bits.period>>1)){//phase shift must never exceed 180degree
                isrcom.Bits.gotpf=true;
                isrcom.Bits.lastitime=t_1ms;
                if(isrcom.Bits.phshift>(isrcom.Bits.period>>2))//phase shift above pi/4 indicates capacitive load
                    isrcom.Bits.phshift=(isrcom.Bits.period>>1)-isrcom.Bits.phshift; //calculate complementary angle      
            }
            else
                isrcom.Bits.gotpf=false;
        }
        else
            isrcom.Bits.gotpf=false;
    }
    IFS0bits.IC2IF=false;
}

void __ISR(_SPI_4_VECTOR, ipl4SOFT) _IntHandlerSPI4(void){
    uint8_t i=0;
    uint32_t adres;
    asm("ei");
    if(IFS1bits.SPI4EIF){//error; empty buffer and restart with channel 0 if possible
        LATCSET=0x4000;
        TMR1=0;
        SPI4CONbits.ON=false;
        SPI4CONbits.ON=true;
        adres=SPI4BUF;
        for(i=0;i!=8;i++){
            adcch[i].adccntr=adcch[i].adcdata=0;
            adcch[i].stat=EXT_ADC_WAIT4ZERO;
        }
#ifdef  UONLY
        adcind=0;
#else
        adcind=7;
#endif
        sampon();
        IFS1bits.SPI4EIF=false;
    }
    else if(IFS1bits.SPI4RXIF){//  previous data is shifted out. retrieve new data and decide next action
//check status of actual channel, unless channel is 6 or 7. In such case one measurement is enough
        LATCSET=0x4000;
        TMR1=0;
        adres=SPI4BUF&0x03FF;
        SPI4CONbits.ON=false;
        if(adcind>5){
            adcch[adcind].adcdata+=adres;
            adcch[adcind].adccntr++;
            adcch[adcind].stat=EXT_ADC_DTAAVAIL; 
#ifdef UONLY
            nextcx();
#else
            nextchan();
#endif
        }
        else    
        switch(adcch[adcind].stat){
            case EXT_ADC_WAIT4ZERO: //waiting for minimum
                if(adres<MINADCVAL){
                    adcch[adcind].stat=EXT_ADC_WAIT4H;
                    adcch[adcind].adccntr=0;
                    sampon();   //start next sampling process
                }
                else if(++adcch[adcind].adccntr>MAXNOACT){
                        adcch[adcind].stat=EXT_ADC_TIMEOUT;    //faulty channel, reading DC
#ifdef UONLY
                        nextcx();
#else
                        nextchan();
#endif
                    }
                else
                    sampon();
                break;
                
            case EXT_ADC_WAIT4H: //waiting for rising edge
                if(adres>MINADCVAL){
                    adcch[adcind].adcdata=adres*adres;
                    adcch[adcind].stat=EXT_ADC_SAMPLING;
                    adcch[adcind].adccntr=0;
                    sampon();
                }
                else if(++adcch[adcind].adccntr>MAXNOACT){
                    adcch[adcind].stat=EXT_ADC_READZERO;    //measurement is ready, result is null
#ifdef UONLY
                    nextcx();
#else
                    nextchan();
#endif  
                }
                else
                    sampon();
                break;
                
            case EXT_ADC_SAMPLING:
                adcch[adcind].adcdata+=adres*adres;
                if(++adcch[adcind].adccntr>MAXNOACT){
                    adcch[adcind].stat=EXT_ADC_TIMEOUT;        //error, reading DC
#ifdef UONLY
                    nextcx();
#else
                    nextchan();
#endif
                }
                else if(adres<STOPADCVAL){
                    adcch[adcind].stat=EXT_ADC_DTAAVAIL;        //got result
#ifdef UONLY
                    nextcx();
#else
                    nextchan();
#endif
                }
                else
                    sampon();
                break;
            default:        //routine must never hit this point!
                isrcom.Bits.spi4fail=true;
                SPI4CONbits.ON=false;
                break;
        } //switch statement
        IFS1bits.SPI4RXIF=false;
    }
}

void __ISR(_UART_4_VECTOR, ipl3SOFT) _IntHandlerDrvUsartInstance0(void){   //USB port
    asm("ei");
    if(IFS2bits.U4EIF){
        usbstat=USBCOM_STATE_INIT;
        IFS2bits.U4EIF=false;   
        LATDSET=0x0800;         //disable USB
        IFS2bits.U4EIF=false;   //do not service USB interrupt again. USB routine will rectify this once port gets reconnected
        IEC2bits.U4EIE=IEC2bits.U4RXIE=IEC2bits.U4TXIE=false;
    }
    else if((IFS2bits.U4TXIF)&&(IEC2bits.U4TXIE)){  //will need to check for IE bit, since HW will set TXIF bit if buffer is empty!
        while((!U4STAbits.UTXBF)&&(u4txptr<u4txbuf[4]))
            U4TXREG=u4txbuf[u4txptr++];
        if(u4txptr==u4txbuf[4])
            IEC2bits.U4TXIE=false;  //buffer completely shifted out. Will leave UTXEN set, but disable interrupts
        IFS2bits.U4TXIF=false;
    } 
    else if(IFS2bits.U4RXIF){
        while(U4STAbits.URXDA)
            u4rxbuf[u4rxptr++]=U4RXREG;
        if(u4rxptr<6){ //check if initialization bytes are as expected.
            if((u4rxbuf[0]!=0xAA)||(u4rxbuf[1]!=0x55))   //abandon reception if initialization went wrong. 
                u4rxptr=0;                             //re-initialize pointer, wrong INI bytes   
            else{
                t_u4rx=t_1ms;       
                isrcom.Bits.rxon=true;
            }
        }
        else{//check if another 6 bytes are available
            if((u4rxbuf[4]-u4rxptr)<6)
                U4STAbits.URXISEL=0;        //interrupt after every bit
            if(u4rxbuf[4]==u4rxptr){
                U4STAbits.URXISEL=0b10; //revert to full size buffer
                isrcom.Bits.rxon=false;
            }
        }
        IFS2bits.U4RXIF=false;
    }
}


void __ISR(_UART_1_VECTOR, ipl2SOFT) _IntHandlerDrvUsartInstance1(void){    //Serial port RS485
    asm("ei");
    /* This interrupt not in use at this time */
    IFS0bits.U1EIF=false;
    IFS0bits.U1TXIF=false;
    IFS0bits.U1RXIF=false;
}

void __ISR(_CAN_1_VECTOR, IPL7SOFT) _IntHandlerDrvCANInstance0(void){       //CAN communication port
    asm("ei");
   CANRxMessageBuffer *buf;
    if(C1VECbits.ICODE>0x40){
            //fatal error. stop CAN bus and try restart later
            cancomstate=CAN_STATE_INIT;	 //remember error status
            IEC1bits.CAN1IE=0;
            tbusfail=t_1ms;
//            if(C1VECbits.ICODE==0x42)
    }

    else switch(C1VECbits.ICODE){
        case 2:	buf = PA_TO_KVA1(C1FIFOUA2);	//just one RX buffer, picking every message
            C1FIFOCON2SET=0x2000;		//set UINC bit;
            canrxbuf[++canrxptr]=*buf;             //Transfer standard message from buffer 2 to circular buffer
            break;
        }
    C1INTbits.WAKIF=0;      //reset from wait state     
    IFS1bits.CAN1IF=false;
}

void nextcx(void){
    switch(adcind){
        case 0:
            adcind=2;
            break;
        case 1:
            adcind=0;
            break;
        case 2:
            adcind=1;
            break;
        default:
            adcind=0;
    }
    if(adcch[adcind].stat!=EXT_ADC_WAIT4ZERO){
        isrcom.Bits.spi4fail=true;
        SPI4CONbits.ON=false;
    }
    else
        sampon();
}


//searches for next channel. that channels status must be 0b00, or the routine will mark an error
//if channel switching was successful the routine will initiate sampling
void nextchan(void){
    if(++adcind==8)
        adcind=0;
    if(!TESTBIT(GENCFGL,0)){ //skip third phase if single phase generator
        if(adcind==2)
            adcind=3;
        else if(adcind==5)
            adcind=6;
    }
    if(adcch[adcind].stat!=EXT_ADC_WAIT4ZERO){
        isrcom.Bits.spi4fail=true;
        SPI4CONbits.ON=false;
    }
    else
        sampon();
}

//will restart sampling process. will load data as per adcind into the tx buffer
void sampon(void){
    while(TMR1<11); //this is the minimum CS time. Will wait 275ns
    LATCCLR=0x4000;
    SPI4CONbits.ON=true;
    SPI4BUF=calladc[adcind];
}

void chleds(void){
    switch (ledstat){
        case GNSOLID:
            LATGCLR=0x2000;
            LATGSET=0x1000;
            break;
        case RDSOLID:
            LATGCLR=0x1000;
            LATGSET=0x2000;
            break;
        case GNBLK:
            LATGCLR=0x2000;
            LATGbits.LATG12^=1;
            break;
        case RDBLK:
            LATGCLR=0x1000;
            LATGbits.LATG13^=1;
            break;
        case GNRDBLK:
            LATGSET=0x1000;
            LATGbits.LATG13^=1;
            break;
        case RDGNBLK:
            LATGSET=0x2000;
            LATGbits.LATG12^=1;
            break;
        case BLKALT:
            LATGbits.LATG12^=1;
            LATGbits.LATG13=!LATGbits.LATG12;
            break;
        case BLKSYNC:
            LATGbits.LATG12^=1;
            LATGbits.LATG13=LATGbits.LATG12;
            break;
        case ALLOFF:
        default:LATGCLR=0x3000;
        break;     
    }
}

