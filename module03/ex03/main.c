#include "uart.h"

void uart_tx(char c)
{
    // Poll UDRE0 (TX buffer empty flag) before writing 
    // prevents overwriting a byte still being shifted out (p.186, 200, 212, 210)
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void print_msg(char *s) {
    int i = 0;
    while(s[i])
        uart_tx(s[i++]);
}

// each event on hardware has its own vector number
// 19 - 1 -> USART Rx Complete 
void __vector_18(void) __attribute__((signal, used));
void __vector_18(void) {
    char c = UDR0;
    // finish actual string
    if (c == '\r') {
        hex_buf[idx] = '\0';
        state = VALIDATING;
        print_msg("\r\n");
        return ;
    }
    // ignore if full buffer
    if (idx >= BUF_SIZE - 1)
        return;
    
    if (state == WAIT_WRITING) {
        hex_buf[idx++] = c;
        uart_tx(c); // echo normal
    }
}

// p.182 - USART initialization
void uart_init(void)
{
    // Split 12-bit UBRR value across high and low registers (p.204)
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);

    // Enable TX and RX and USART Receive Complete interrupt (p.201)
    UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);;

    // UCSZ01:UCSZ00 = 1:1 -> 8 data bits, USBS0=0 -> 1 stop bit,
    // UPM01:UPM00 = 0:0 -> no parity (8N1 frame format) (p.203)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void init_rgb() {
    // config timer0 and timer2
    /* PD5 (OC0B) as output 
       PD6 (0C0A) as output
       PD3 (OC2B) as output
    */
    DDRD |= (1 << BLUE);
    DDRD |= (1 << RED);
    DDRD |= (1 << GREEN);

    // initialize off (avoid glitch)
    OCR0A = 0;
    OCR0B = 0;
    OCR2B = 0;

    // setup timer0
    TCCR0A = (1 << WGM01) | (1 << WGM00) | (1 << COM0B1) | (1 << COM0A1);
    TCCR0B = (1 << CS01) | (1 << CS00); // prescaler 64

    // setup timer2
    TCCR2A = (1 << WGM21) | (1 << WGM20) | (1 << COM2B1);
    TCCR2B = (1 << CS22); // prescaler 64
}

/* the OCR = 0 problem in fast PWM hardware generate 1 cycle spike
    pin is never LOW for real. The solution is: when value is 0, 
    desconect the pin timer and assume manual control by PORTD */
void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    // if 0, desconect the timer pin (LOW forced)
    // if >0, connect on non-inverting PWM
    TCCR0A = (1 << WGM01) | (1 << WGM00)
           | (r ? (1 << COM0B1) : 0)
           | (g ? (1 << COM0A1) : 0);
    TCCR2A = (1 << WGM21) | (1 << WGM20)
           | (b ? (1 << COM2B1) : 0);

    OCR0B = r;
    OCR0A = g;
    OCR2B = b;

    // manual LOW
    if (!r) PORTD &= ~(1 << RED);
    if (!g) PORTD &= ~(1 << GREEN);
    if (!b) PORTD &= ~(1 << BLUE);
}

static uint8_t hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return c - 'a' + 10;
}

static uint8_t hex_byte(char high, char low)
{
    return (hex_val(high) << 4) | hex_val(low);
}

int valid_hex(volatile char *s)
{
    if (s[7] != '\0' || s[0] != '#')
        return print_msg("Wrong format\r\n"), 0;

    for (int i = 1; i < 7; i++)
    {
        if (!(
            (s[i] >= '0' && s[i] <= '9') ||
            (s[i] >= 'A' && s[i] <= 'F') ||
            (s[i] >= 'a' && s[i] <= 'f')
        ))
            return print_msg("Invalid hex\r\n"), 0;
    }

    uint8_t r = hex_byte(s[1], s[2]);
    uint8_t g = hex_byte(s[3], s[4]);
    uint8_t b = hex_byte(s[5], s[6]);

    set_rgb(r, g, b);
    return 1;
}

int main() {
    init_rgb();
    set_rgb(0, 0, 0);
    uart_init();
    SREG |= (1 << 7);  // Global Interrupt Enable
    while (1) {
        if (state == VALIDATING) {
            valid_hex(hex_buf);
            state = WAIT_WRITING;
            idx = 0;
        }
    }
}
