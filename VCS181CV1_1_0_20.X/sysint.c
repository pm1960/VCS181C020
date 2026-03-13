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

void __ISR(_ADC_VECTOR, IPL3SOFT) adchandler(void){    
    adchan[0]+=ADC1BUF0;    //oil pressure
    adchan[1]+=ADC1BUF1;    //battery voltage
    adchan[2]+=ADC1BUF2;    //tank
    adchan[3]+=ADC1BUF3;    //water leak /Fan breaker status
    adchan[4]+=ADC1BUF4;    //act. voltage upstream of the sense resistor
    adchan[5]+=ADC1BUF5;    //act. voltage downstream of the sense resistor
    adchan[6]+=ADC1BUF6;    //cyl. head temperature
    adchan[7]+=ADC1BUF7;    //alt. bearing temperature
    adchan[8]+=ADC1BUF8;    //exh. manifold head temperature
    adchan[9]+=ADC1BUF9;    //coolant in temperature
    adchan[10]+=ADC1BUFA;    //coil 1  temperature
    adchan[11]+=ADC1BUFB;    //coil 2  temperature
    adchan[12]+=ADC1BUFC;    //coil 3  temperature
    adchan[13]+=ADC1BUFD;    //coolant out temperature
    if(++adccntr<64)
        AD1CON1bits.SAMP=1;
    IFS1bits.AD1IF=false;
}

void __ISR(_TIMER_4_VECTOR, IPL4SOFT)timer4_handler(void){
    t_1ms++;
    if(nocan<MAXNOCAN)
        nocan++;
    IFS0bits.T4IF=false;
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
//    if(IFS)
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

