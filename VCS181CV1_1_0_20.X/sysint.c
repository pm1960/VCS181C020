#include <xc.h>
#include <sys/attribs.h>
#include <sys/kmem.h>
#include <stdbool.h>
#include <stdint.h>
#include "Structures.h"
#include "Globals.h"
#include "Defines.h"

static void eval_ac(volatile ACCHANNEL *ptr,uint16_t res);
static void eval_dc(volatile DCCHANNEL *ptr,uint16_t res);
static void chleds(void);

static uint32_t lastpulse,v_trig;


void __ISR(_TIMER_1_VECTOR, IPL3SOFT) Timer1Handler(void){    //most frequent interrupt, use shadow register set
    if(!AD1CON1bits.ASAM){
        if(adccntr<64){             //below channel assignment refer to the configuration AFTER initialization, i.e. RB11 no lnger being quizzed
            adchan[0]+=ADC1BUF0;    //oil pressure
            adchan[1]+=ADC1BUF1;    //Battery voltage
            adchan[2]+=ADC1BUF2;    //Tank
            adchan[3]+=ADC1BUF3;    //Water leak (fan breaker)
            adchan[4]+=ADC1BUF4;    //act. voltage upstream of the sense resistor
            adchan[5]+=ADC1BUF5;    //act. voltage downstream of the sense resistor
            adchan[6]+=ADC1BUF6;    //cylinder head temp
            adchan[7]+=ADC1BUF7;    //alternator bearing
            adchan[8]+=ADC1BUF8;    //exhaust elbow
            adchan[9]+=ADC1BUF9;    //coolant in
            adchan[10]+=ADC1BUFA;    //coil 1
            adchan[11]+=ADC1BUFB;    //coil 2
            adchan[12]+=ADC1BUFC;    //coil 3
            adchan[13]+=ADC1BUFD;    //coolant out
            adccntr++;
        }
        AD1CON1bits.ASAM=1;
        AD1CON1bits.SAMP=1;
    }
    IFS0bits.T1IF=false;
}

void __ISR(_TIMER_4_VECTOR, IPL4SOFT)timer4_handler(void){
    t_1ms++;
    if(nocan<MAXNOCAN)
        nocan++;
    if(!(t_1ms%500))
        chleds();
    IFS0bits.T4IF=false;
}

void __ISR(_INPUT_CAPTURE_1_VECTOR, ipl7SOFT) _IntHandlerDrvUpuls(void){
    uint32_t act;
    static uint32_t lastt;
    if(IC1CONbits.ICOV){
        while(IC1CONbits.ICBNE)
            act=IC1BUF;
        isr_flags &= ~GOTUPULS;
    }
    else{
        act=v_trig=IC1BUF;
        v_period=abs(act-lastt);
        lastt=act;
        isr_flags |= GOTUPULS;
    }
    IFS0bits.IC1IF=false;
}

void __ISR(_CAN_1_VECTOR, IPL6SOFT) _IntHandlerDrvCANInstance0(void){       //CAN communication port
    CANRxMessageBuffer *buf;
    if(C1VECbits.ICODE>0x40){
            //fatal error. stop CAN bus and try restart later
        cancomstat=CAN_STATE_RESET;	 //remember error status
        IEC1bits.CAN1IE=0;
    }

    else switch(C1VECbits.ICODE){
        case 2:	buf = PA_TO_KVA1(C1FIFOUA2);	//just one RX buffer, picking every message
            C1FIFOCON2SET=0x2000;		//set UINC bit;
            canrxbuf[canrxptr]=*buf;             //Transfer standard message from buffer 2 to circular buffer
            canrxptr++;
            break;
            
        }
    C1INTbits.WAKIF=0;      //reset from wait state     
    IFS1bits.CAN1IF=false;
}

void __ISR(_INPUT_CAPTURE_2_VECTOR, ipl7SOFT) _IntHandlerDrvIpuls(void){//this is the current signal
    uint32_t act;
    if(IC2CONbits.ICOV){
        while(IC2CONbits.ICBNE)
            act=IC2BUF;
        isr_flags &=~GOTIPULS;
    }
    else{
        act=IC2BUF;
        i_period=abs(act-v_trig);
    }
    IFS0bits.IC2IF=false;
}

//spi 4 runs at 3.33MHz, so one round of 3 bytes takes 24/3.33MHz=7.2us
//after gathering a result the function will call an evaluation function (AC or DC). If that evaluation function determines that all measurement
//for that buffer is done, successful or not, it will flag the buffer accordingly and switch to the opposite buffer. It will not check whether the
//buffer was processed by the program loop but rather switch to that buffer. It is the loops task to make sure buffers are processed in due time
void __ISR(_SPI_4_VECTOR,IPL5SRS)spi4handler(void){
    uint8_t rdx;
    static uint16_t adres;
    rdx=SPI4BUF;
    if(IFS1bits.SPI4EIF){//error; empty buffer and restart with channel 0 if possible
        LATCSET=0x4000;     //bring CS high
        SPI4CONbits.ON=false;
    }
    else switch(spistep){
        case SPI_MCP_START: //got junk data, discard
            spistep=SPI_MCP_CMD;
            SPI4BUF=0x80 |(spichannel <<4); //select channel, keep going
            break;
            
        case SPI_MCP_CMD:
            adres=rdx<<7;
            spistep=SPI_MCP_DUMMY;
            SPI4BUF=0;              //sent junk data
            break;
            
        case SPI_MCP_DUMMY:
            adres|=rdx;
            LATCSET=0x4000;
            if(spichannel<6){
                if(spichannel & isoptrs)
                    eval_ac(acchptr[0][spichannel],adres);
                else
                    eval_ac(acchptr[1][spichannel],adres);

            }
            else if(spichannel & isoptrs)
                eval_dc(dcchptr[0][spichannel-6],adres);
            else
                eval_dc(dcchptr[1][spichannel-6],adres);
            spichannel=(spichannel+1)%8;
//start over with next channel            
            
            LATCCLR=0x4000;
            spistep=SPI_MCP_START;
            SPI4BUF=1;          //start next round
        break;
    }
}

void __ISR(_SPI_2_VECTOR,IPL2SOFT)spi2handler(void){
    uint16_t rdx;
    LATGSET=0x0200;
    rdx=SPI2BUF;
    if(IFS1bits.SPI2EIF)
        SPI2CONbits.ON=false;
    else if(isr_flags & RDVCSTEMP){
        isr_flags &= ~RDVCSTEMP;
        vcstemp += rdx;
        LATGCLR=0x0200;
        SPI2BUF=0x7800;     //read oil temperature  
    }
    else{
        isr_flags |= RDVCSTEMP;
        oiltemp +=rdx;
        if(++u17cntr<64){
            LATGCLR=0x0200;
            SPI2BUF=0x6800;     //read VCS temperature  
        }
    }
    IFS1bits.SPI2RXIF=false;
}


void eval_ac(volatile ACCHANNEL *ptr,uint16_t res){
    uint32_t tmrx=((uint32_t)TMR5<<16) | (uint32_t)TMR4;
    switch(ptr->status){
        case ADC_IDLE:
            if(res<ADC_MINADC){
                ptr->status=ADC_WAIT4ZERO;  //from idle the sensing will always start at waiting for zero
                ptr->tmrstart=tmrx;
            }
            else if((tmrx-ptr->tmrstart)>MAXWAIT){      //some DC signal is sticking to the input
                ptr->status=ADC_FAILHIGH;
                TOGGLE_BIT(isoptrs,spichannel);
                TOGGLE_BIT(isoptrs,spichannel+8);
            }
            break;
            
        case ADC_WAIT4ZERO:
            if(res<ADC_MINADC){
                if((tmrx-ptr->tmrstart)>MAXWAIT){
                    ptr->status=ADC_TIMEOUT;
                    TOGGLE_BIT(isoptrs,spichannel);
                    TOGGLE_BIT(isoptrs,spichannel+8);
                }    
            }
            else{   //may start sensing if min. low time did elapse
                if((tmrx-ptr->tmrstart)>MINLOW){
                    ptr->status=ADC_SAMPLING;
                    ptr->tmrstart=tmrx;
                    ptr->bufferx=res*res;
                    ptr->samplesx=1;
                }
                else 
                    ptr->status=ADC_IDLE;
            }
            break;
            
        case ADC_SAMPLING:
            if(res>ADC_MINADC){
                if((tmrx-ptr->tmrstart)>MAXHALFPER){
                    ptr->status=ADC_FAILHIGH;
                    TOGGLE_BIT(isoptrs,spichannel);
                    TOGGLE_BIT(isoptrs,spichannel+8);
                }
                else{
                    ptr->bufferx+=res*res;
                    ptr->samplesx++;
                }
            }
            else{
                if((tmrx-ptr->tmrstart)<MINHALFPER)
                    ptr->status=ADC_FAILSHORT;
                else 
                    ptr->status=ADC_READY;
                TOGGLE_BIT(isoptrs,spichannel);
                TOGGLE_BIT(isoptrs,spichannel+8);
            }
            break;
    }
    
}



void eval_dc(volatile DCCHANNEL *ptr,uint16_t res){
    ptr->bufferx+=res;
    ptr->cntr++;
    if(ptr->cntr>64){
        ptr->status=ADC_READY;
        TOGGLE_BIT(isoptrs,spichannel);
        TOGGLE_BIT(isoptrs,spichannel+8);
    }
}



void chleds(void){
    switch (ledstat){
        case gn_solid:
            LATGCLR=0x2000;
            LATGSET=0x1000;
            break;
        case rd_solid:
            LATGCLR=0x1000;
            LATGSET=0x2000;
            break;
        case gn_blk:
            LATGCLR=0x2000;
            LATGbits.LATG12^=1;
            break;
        case rd_blk:
            LATGCLR=0x1000;
            LATGbits.LATG13^=1;
            break;
        case gnon_rdblk:
            LATGSET=0x1000;
            LATGbits.LATG13^=1;
            break;
        case rdon_gnblk:
            LATGSET=0x2000;
            LATGbits.LATG12^=1;
            break;
        case blk_alt:
            LATGbits.LATG12^=1;
            LATGbits.LATG13=!LATGbits.LATG12;
            break;
        case blk_sync:
            LATGbits.LATG12^=1;
            LATGbits.LATG13=LATGbits.LATG12;
            break;
        case all_off:
        default:LATGCLR=0x3000;
        break;     
    }
}