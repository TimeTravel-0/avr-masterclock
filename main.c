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


void config_io_pins(void)
{
    // set these three pins as output
    DDRD |= (1 << PD0);
    DDRD |= (1 << PD1);
    DDRD |= (1 << PD2);
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
        case 0:
            output_positive();
            state=state+1;
            break;
        case 10:
        case 70:
            output_zero();
            state=state+1;
            break;
        case 60:
            output_negative();
            state=state+1;
            break;
        case 120:
            state=0;
            break;
        default:
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

    while(1){} // empty main loop - interesting stuff happens in ISRs.

    return 0;
}
