#include "includes/uart.h"

static volatile uint8_t tx_buf[TX_BUF_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;

/*
** Configure UART for TX-only at configured baud rate.
** U2X0 enables double-speed mode for better accuracy at 115200.
** UDRIE0 (TX interrupt) is enabled only when data is queued (lazy approach).
*/
void	uart_init(void)
{

	UBRR0H = (uint8_t)(UBRR_VALUE >> 8); // Set baud rate high byte
	UBRR0L = (uint8_t)UBRR_VALUE; // Set baud rate low byte

	UCSR0B = (1 << TXEN0); // Enable transmitter, no receiver

	UCSR0C =  (0 << UMSEL01) | (0 << UMSEL00) // Asynchronous USART
			| (0 << UPM01)   | (0 << UPM00) // No parity
			| (0 << USBS0) // 1 stop bit
			| (1 << UCSZ01)  | (1 << UCSZ00); // 8 data bits
}

/*
** Ring-buffer wrap: advance index with modulo using bit-mask.
** Only works because TX_BUF_SIZE is power-of-two (64).
*/
static uint8_t	tx_next(uint8_t idx)
{
	return ((uint8_t)((idx + 1) & (TX_BUF_SIZE - 1)));
}

/*
** Enable UART Data Register Empty interrupt.
** Safe to call multiple times (setting already-set bit is a no-op).
** This wakes the TX consumer ISR if queue was idle.
*/
static void	uart_tx_kick(void)
{
	UCSR0B |= (1 << UDRIE0);
}

/*
** Enqueue single byte; returns 0 if buffer full, 1 if success.
** Non-blocking: never waits. Returns immediately to avoid ISR latency.
** Kicks UDRIE0 to ensure consumer wakes up if idle.
*/
static uint8_t	uart_tx_push(uint8_t c)
{
	uint8_t	next = tx_next(tx_head);

	if (next == tx_tail)
		return (0);
	tx_buf[tx_head] = c;
	tx_head = next;
	uart_tx_kick();
	return (1);
}

/*
** Enqueue variable-length buffer; drops silently if insufficient space.
** Non-blocking to keep ISR latency bounded; samples lost if queue full.
** Each call to uart_tx_push checks free slots and kicks TX ISR.
*/
void	uart_tx_push_buf(volatile uint8_t *data, uint8_t len)
{
	uint8_t	free_slots;
	uint8_t	i;

	if (tx_head >= tx_tail)
		free_slots = (uint8_t)(TX_BUF_SIZE - (tx_head - tx_tail) - 1);
	else
		free_slots = (uint8_t)(tx_tail - tx_head - 1);
	if (free_slots < len)
		return ;

	i = 0;

	while (i < len)
	{
		uart_tx_push(data[i]);
		++i;
	}
}

/*
** USART Data Register Empty interrupt: drain TX queue to hardware.
** Called each time UDR0 becomes ready (every ~87 µs @ 115200 baud).
** Sends one byte per ISR; auto-disables UDRIE0 when queue empty.
** Paces output at UART bitrate with zero busy-waiting.
*/
void	__vector_19(void) __attribute__((signal, used));
void	__vector_19(void) // USART data register empty ISR
{
	if (tx_head == tx_tail)
	{
		UCSR0B &= ~(1 << UDRIE0);
		return ;
	}
	UDR0 = tx_buf[tx_tail];
	tx_tail = tx_next(tx_tail);
}