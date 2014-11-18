
#define F_CPU 12000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/fuse.h>
#include <avr/interrupt.h>

/*
#    avr runs on 32768 Hz clock crystal (XTAL pins)
#
#    basic function is as follows:
#
#    while true:
#        sleep 50 seconds
#        create positive pulse (one of the two outputs high, the other low) across H bridge for 10 seconds
#        sleep 50 seconds
#        create negative pulse (one of the two outputs low, the other high) across H bridge for 10 seconds
#
#
*/



// fuses settings in source code get compiled into data at magic numbered addresses in the hex file
// these addresses are outside avr memory space, e.g. prog fails if avrdude does not know that

// defaults:
// avrdude: safemode: lfuse reads as 64
// avrdude: safemode: hfuse reads as DF
// avrdude: safemode: efuse reads as FF

// see http://www.engbedded.com/fusecalc/
// see http://www.avrfreaks.net/forum/help-avrdude-and-fuse-program-elf-file

FUSES =
{
.low = (FUSE_CKSEL0 & FUSE_CKSEL1 & FUSE_CKSEL2 & FUSE_CKSEL3 & FUSE_SUT0 /* & FUSE_CKDIV8 */), // 0xe0
.high = HFUSE_DEFAULT,
.extended = EFUSE_DEFAULT,
};



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
    DDRD |= (1 << PD3); // flicker output, prbs driven
}

void output_positive(void) // H bridge pos.
{
    PORTD |= (1 << PD0); // set this pin high
    PORTD &=~(1 << PD1); // set this pin low
}

void output_negative(void) // H bridge neg.
{
    PORTD &=~(1 << PD0); // set this pin low
    PORTD |= (1 << PD1); // set this pin high
}

void output_zero(void) // H bridge off
{
    PORTD &=~(1 << PD0); // set this pin low
    PORTD &=~(1 << PD1); // set this pin low
}

// finite state machine
void fsm(void)
{
    static unsigned int state=0;

    switch(state)
    {
        case 0: // sate 0
            output_positive();
            state=state+1;
            break;
        case 10: // state 10
        case 70: // state 70
            output_zero();
            state=state+1;
            break;
        case 60: // state 60
            output_negative();
            state=state+1;
            break;
        case 120: // state 120
            state=0;
            break;
        default: // default state. if no other sate applies, go to next state
            state=state+1;
    }

}

void config_interrupts(void)
{
    TCCR0B = 0b00000101; // clock divider TODO
    TIMSK |= (1<<TOIE0); // enable overflow interrupt for timer 0
    sei(); // enable global interrupts
}

ISR(TIMER0_OVF_vect) // timer 0 overflow interrupt service routine
{
    fsm();
    PORTD^= ( 1 << PD2 ); // toggle bit to show step
}


int main(void)
{
    config_io_pins();
    config_interrupts();


    while(1)
    {

        // flicker an LED just because we can (and to simulate a neon tube for flip clocks ;)
        if(prbs()) // 50% certaincy to switch LED on
        {
            PORTD&= ~( 1 << PD3 ); // set
        }
        else // 50 %
        {
            if(prbs() && prbs() && prbs() ) // 0.5 * 0.5 * 0.5 = 6.25 % certaincy to switch LED off
            {
                PORTD|= ( 1 << PD3 ); // reset
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
