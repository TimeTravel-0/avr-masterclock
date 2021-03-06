
//#define F_CPU 32768UL //32kHz quartz crystal
#define F_CPU 512000UL // 4096000 but we divide this by 8!

#include <avr/io.h>
#include <util/delay.h>
#include <avr/fuse.h>
#include <avr/interrupt.h>

/*
#    avr runs on 32768 Hz clock crystal (XTAL pins)
#    >> NO it runs off 4,096 MHz
#
#    basic function is as follows:
#
#    while true:
#        sleep 50 seconds
#        create positive pulse (one of the two outputs high, the other low) across H bridge for 10 seconds
#        sleep 50 seconds
#        create negative pulse (one of the two outputs low, the other high) across H bridge for 10 seconds
#
#   (low prio meanwhile: flicker with pin/LED for fun)
#
*/



// fuses settings in source code get compiled into data at magic numbered addresses in the elf file
// these addresses are outside avr memory space, e.g. prog fails if avrdude does not know that

// defaults:
// avrdude: safemode: lfuse reads as 64
// avrdude: safemode: hfuse reads as DF
// avrdude: safemode: efuse reads as FF

// see http://www.engbedded.com/fusecalc/ for 0xD8, 0xDF, 0xFF

FUSES =
{
// external clock, long startup time, do divide clock by 8
.low = (FUSE_CKSEL0 & FUSE_CKSEL1 & FUSE_CKSEL2 & /*FUSE_CKSEL3 & FUSE_SUT0 & */ FUSE_SUT1  & FUSE_CKDIV8 ), // 0xD8
.high = HFUSE_DEFAULT,
.extended = EFUSE_DEFAULT,
};


#define IR_TIME_THRESHOLD 5
#define IR_TIME_OUT 20

// global vars
volatile uint16_t isrctr, ircmd, ircmdpos;
volatile uint8_t fsm_fast;


uint8_t prbs(void)
{
    static uint32_t sequence = 1; // initial value
    sequence = sequence << 1 |  (((sequence >> 30) ^ (sequence >> 27)) & 1); // prbs calculation
    return sequence & 0x01; // return a bit
}

void config_io_pins(void)
{
    // set these pins as output
    DDRD |= (1 << PD0); // h bridge 1
    DDRD |= (1 << PD1); // h bridge 2
    DDRD |= (1 << PD2); // second ticker (toggles every second)
    //DDRD |= (1 << PD3); // flicker output, prbs driven
    DDRD |= (1 << PD5);
    
    
    DDRB |= (1 << PB0); // h bridge 1
    DDRB |= (1 << PB1); // h bridge 2
    DDRB |= (1 << PB2); // h bridge 3
    DDRB |= (1 << PB3); // h bridge 4
    
    // initial data
    PORTD|= ( 1 << PD5 );
    
}

void output_positive(void) // H bridge pos.
{
    PORTB |= (1 << PB0); // set this pin high
    PORTB &=~(1 << PB1); // set this pin low
}

void output_negative(void) // H bridge neg.
{
    PORTB &=~(1 << PB0); // set this pin low
    PORTB |= (1 << PB1); // set this pin high
}

void output_zero(void) // H bridge off
{
    PORTB &=~(1 << PB0); // set this pin low
    PORTB &=~(1 << PB1); // set this pin low
}

// finite state machine
void fsm(void)
{
    static uint32_t state=0;

    switch(state)
    {
        case 0: // sate 0
            output_positive();
            state=1; // go to state 1 next
            break;
        case 20: // state 2
        case 620: // state 62
            //output_zero();
            state=state+1; // go to next state
            break;
        case 600: // state 60
            output_negative();
            state=610; // go to state 61 next
            break;
        case 1190: // state 119
            state=0; // return to first state instead of state 120
            break;
        default: // default state. if no other sate applies, go to next state
            state=state+1;
    }

}

void config_interrupts(void)
{
    // clock input to AVR is 32768 Hz ; no 4,096MHz
    // timer 0 should fire at 1Hz
    // this means divide input clock by 2^15 ; no! 4096000 Hz / ? = 1
    // see Figure 27. 8-bit Timer/Counter Block Diagram in attiny2313 datasheet for reference

    // timer/counter control register B - TTC0B
    // last 3 bit define clock select for timer 0
    // 001 = 2^1  = 1
    // 010 = 2^3  = 8
    // 011 = 2^6  = 64
    // 100 = 2^8  = 256
    // 101 = 2^10 = 1024
    TCCR0B = 0b00000010; // prescale = 8
    TCCR0A = (1<<WGM01); // CTC mode, so we can go for 2^7 here
    
    // triggering at 2ms / 4ms interval

    // the timer starts at value 0:
    TCNT0 = 0x0;

    TIMSK |= (1<<OCIE0A); // enable compare interrupt for timer 0

    // The counter value (TCNT0) increases until a Compare Match occurs
    // between TCNT0 and OCR0A, and then counter (TCNT0) is cleared.
    OCR0A = 0x7f; // half way up
    
    
    
    // int1
    
	MCUCR |= (1<<ISC10) | (1<<ISC11); // logic level change of INT1 creates interrupt
    GIMSK = 1<<INT1;					// Enable INT1
    
    
    // timer 1
    
    
    TCCR1B = 0b00000001; // prescale = 8
    TCCR1A = (1<<WGM01); // CTC mode, so we can go for 2^7 here
    TCNT1 = 0x0;
    TIMSK |= (1<<OCIE1A); // enable compare interrupt for timer 0
    OCR1A = 0x4f; // half way up


    sei(); // enable global interrupts
}

// called at 2ms!
ISR(TIMER0_COMPA_vect)
{
    fsm();
    //PORTD^= ( 1 << PD2 ); // toggle bit to show step
    //PORTB^= ( 1 << PB3 ); // toggle bit to show step
}

// called at ~200us interval
ISR(TIMER1_COMPA_vect)
{
    TCNT1 = 0x0; // reset timer counter
    //PORTB^= ( 1 << PB3 ); // toggle bit to show step
    isrctr=isrctr+1;
    
    if(isrctr>IR_TIME_OUT)
    {
        isrctr=0;
        ircmd=0;
        ircmdpos=0;
    }
    
    // long pulse is ~ 1200 us  ; 1200 / 200 = 9
    // short pulse is ~ 640 us  ; 640 / 200 = 3
    
    
    
    
}

ISR(INT1_vect) // infrared receiver
{
    
    
    
    //PORTB^= ( 1 << PB3 ); // toggle bit to show step
    
    if(MCUCR & (1<<ISC10)) // we just had a rising edge
	{
        // low time was "isrctr".
        
        ircmd<<=1; // shift up 1 bit
        if(isrctr < IR_TIME_THRESHOLD)
        {
            // we received a short pulse (logical zero)
            //ircmd&=~(1);
        }
        else
        {
            // we received a long pulse (logical one)
            ircmd|=1;
        }
        
        
        /*
        PORTB&= ~( 1 << PB3 ); // set to 0
        _delay_us(5);
        if(ircmd&0x01==0x01) _delay_us(15);
        PORTB|= ( 1 << PB3 ); // set to 1
        */
        
        //  S = 11011 0100 1011 1000 11001 
        // 2S = 11111 0100 1011 1000 11001 
        
        //  S = 10 0110 0011 1010 0101 1011 ; = 0x 6 3A 5B
        // 2S = 10 0110 0011 1010 0101 1111 ; = 0x 6 3A 5F
        
        // in sw:
        //  S = 1111 0001 1101 0010 00 <-> 0100 1011 1000 1111 = 4B8F
        // 2S = 1111 0001 1101 0011 00 <-> 1100 1011 1000 1111 = CB8F
        
        ircmdpos+=1;
        if(ircmdpos>20)
        {
        
        /*
        for(uint8_t i=0;i<20;i++)
        {
            PORTB&= ~( 1 << PB3 ); // set to 0
            PORTB|= ( 1 << PB3 ); // set to 1
            PORTB&= ~( 1 << PB3 ); // set to 0
            if(ircmd&0x01 == 0x01) PORTB|= ( 1 << PB3 ); // set to 1
            ircmd>>=1;
            _delay_us(10);
        }
        */
        
        
        isrctr=0;
        ircmd=0;
        ircmdpos=0;
        
        }
        
        if((ircmd&0xFF) == 0x4B)
        {
            
            fsm();
            
            PORTD&= ~( 1 << PD5 );
            _delay_ms(10);
            PORTD|= ( 1 << PD5 );
            
            fsm_fast=0x01;
            ircmd = 0x00;
            isrctr = 0x00;    
        }
        if((ircmd&0xFF) == 0xCB)
        {
            
            PORTD&= ~( 1 << PD5 );
            _delay_ms(500);
            PORTD|= ( 1 << PD5 );
            _delay_ms(500);
            PORTD&= ~( 1 << PD5 );
            _delay_ms(500);
            PORTD|= ( 1 << PD5 );
            
            ircmd = 0x00;
            isrctr = 0x00;    
        }
        
        
        

		/*Set interrupt sense to falling edge*/
		MCUCR |= (1<<ISC11);
		MCUCR &= ~(1<<ISC10);
	}
	else // we just had a falling edge
	{
        isrctr=0; // reset counter
	
		/*Set interrupt sense to rising edge*/
		MCUCR |= (1<<ISC10) | (1<<ISC11);
	}
}


int main(void)
{
    
    ircmd=0x00;
    fsm_fast=0x00;
    config_io_pins();
    config_interrupts();


    while(1)
    {

        // flicker an LED just because we can (and to simulate a neon tube for flip clocks ;)
        if(prbs()) // 50% certaincy to switch LED on
        {
            //PORTD&= ~( 1 << PD5 ); // set
            PORTB&= ~( 1 << PB2 ); // set
        }
        else // 50 %
        {
            if(prbs() && prbs() && prbs() ) // 0.5 * 0.5 * 0.5 = 6.25 % certaincy to switch LED off
            {
                //PORTD|= ( 1 << PD5 ); // reset
                PORTB|= ( 1 << PB2 ); // reset
            }
        }

        // delay random delay times to make flicker more realistic
        _delay_ms(1); // wait 1ms, because smaller values make no sense (not visible to eye)
        if(prbs()) // 50% certaincy for 5ms delay
        {
            _delay_ms(5);
        }
        if(prbs()) // 50% certaincy for 19ms delay
        {
            _delay_ms(19);
        }

    } // interesting stuff happens in ISRs.

    return 0;
}
