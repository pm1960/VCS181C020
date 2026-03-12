#include <xc.h>
#include <sys/attribs.h>
#include <stdbool.h>
#include <stdint.h>
#include "Structures.h"
#include "Globals.h"
#include "Defines.h"

static void eval_ac(volatile ACCHANNEL *ptr,uint16_t res);
static void eval_dc(volatile DCCHANNEL *ptr,uint16_t res);

static uint32_t lastpulse;

void __ISR(_TIMER_1_VECTOR, IPL3SOFT) Timer1Handler(void){    //most frequent interrupt, use shadow register set
    uint16_t pgx;
    if(!AD1CON1bits.ASAM){
        if(adccntr<64){             //below channel assignment refer to the configuration AFTER initialization, i.e. RB11 no lnger being quizzed
            adchan[0]+=ADC1BUF0;    //multiplexer
            adchan[1]+=ADC1BUF1;    //oil pressure
            adchan[2]+=ADC1BUF2;    //battery voltage
            adchan[3]+=ADC1BUF3;    //tank
            adchan[4]+=ADC1BUF4;    //act. voltage upstream of the sense resistor
            adchan[5]+=ADC1BUF5;    //act. voltage downstream of the sense resistor
            adchan[6]+=ADC1BUF6;    //board temperature
            adchan[7]+=ADC1BUF7;    //oil temperature
            if(++adccntr==64){      //step up multiplexer
                if(++muxptr==7)
                    muxptr=0;       //outside ISR the loop routine must be aware the MUX is pointing to the next higher input
                pgx=(uint16_t)muxptr<<6;
                LATGCLR=0x01C0;
                LATGSET=pgx;
            }
        }
        AD1CON1bits.ASAM=1;
        AD1CON1bits.SAMP=1;
    }
    IFS0bits.T1IF=false;
}

void __ISR(_EXTERNAL_2_VECTOR, IPL4SOFT) INT2_Handler(void){        //voltage pulse
    uint32_t thispulse;
    uint16_t hi = TMR5;   // latches TMR4
    uint16_t lo = TMR4;
    thispulse = ((uint32_t)hi << 16) | lo;
    v_period=lastpulse-thispulse;
    lastpulse=thispulse;
    isr_flags |= GOTUPULS;
    IFS0bits.INT2IF=false;
}

void __ISR(_EXTERNAL_1_VECTOR, IPL4SOFT) INT1_Handler(void){        //current pulse
    uint32_t thispulse;
    uint16_t hi = TMR5;   // latches TMR4
    uint16_t lo = TMR4;
    thispulse = ((uint32_t)hi << 16) | lo;
    i_period=lastpulse-thispulse;            //always refernce this to the last voltage pulse that was catched
     isr_flags |= GOTIPULS;
    IFS0bits.INT1IF=false;
}
void __ISR(_TIMER_2_VECTOR, IPL3SOFT)timer2_handler(void){
    t_1ms++;
    if(nocan<MAXNOCAN)
        nocan++;
    IFS0bits.T2IF=false;
}

//spi 3 runs at 3.33MHz, so one round of 3 bytes takes 24/3.33MHz=7.2us
//after gathering a result the function will call an evaluation function (AC or DC). If that evaluation function determines that all measurement
//for that buffer is done, successful or not, it will flag the buffer accordingly and switch to the opposite buffer. It will not check whether the
//buffer was processed by the program loop but rather switch to that buffer. It is the loops task to make sure buffers are processed in due time
void __ISR(_SPI_3_VECTOR,IPL5SRS)spi3handler(void){
    uint8_t rdx;
    static uint16_t adres;
    rdx=SPI3BUF;
    if(IFS0bits.SPI3EIF){//error; empty buffer and restart with channel 0 if possible
        LATDSET=0x0010;     //bring CS high
        SPI3CONbits.ON=false;
    }
    else switch(spistep){
        case SPI_MCP_START: //got junk data, discard
            spistep=SPI_MCP_CMD;
            SPI3BUF=0x80 |(spichannel <<4); //select channel, keep going
            break;
            
        case SPI_MCP_CMD:
            adres=rdx<<7;
            spistep=SPI_MCP_DUMMY;
            SPI3BUF=0;              //sent junk data
            break;
            
        case SPI_MCP_DUMMY:
            adres|=rdx;
            LATDSET=0x0010;
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
            
            LATDCLR=0x0010;
            spistep=SPI_MCP_START;
            SPI3BUF=1;          //start next round
        break;
    }
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

