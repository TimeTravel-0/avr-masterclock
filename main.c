#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    DDRD |= (1 << PD0);
    DDRD |= (1 << PD1);

    while(1) {
        PORTD ^= (1 << PD0);
        _delay_ms(500);
        PORTD ^= (1 << PD1);
        _delay_ms(500);
    }
    return 0;
}
