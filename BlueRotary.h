//*******************************************************
//					GPIO Definitions
//*******************************************************
#define HOOK		0		//Port C.0
#define BT_EN		1		//Port C.1
#define RING_PWR	2		//Port C.2
#define RING1		3		//Port C.3
#define RING2		4		//Port C.4
#define PSTAT		5		//Port C.5
#define BATT		7

#define BT_TX		0		//Port D.0
#define BT_RX		1		//Port D.1
#define	BT_RES		2		//Port D.2
#define EROTARY		3		//Port D.3
#define ROTARY		4		//Port D.4
#define BT_UNPAIR   5       //Port D.5
#define DT1			6		//Port D.6
#define DT2			7		//Port D.7

#define	CS			2		//Port B.2
#define MOSI		3		//Port B.3
#define MISO		4		//Port B.4
#define SCK			5		//Port B.5

//*******************************************************
//						Macros
//*******************************************************
#define	LED_ON()	cbi(PORTC, PSTAT)
#define	LED_OFF()	sbi(PORTC, PSTAT)

#define TONES_ON()  TIMER2_ON(TIMER2_PRESCALE_1)
#define TONES_OFF() TIMER2_OFF()

#define RINGER_POWER_UP()	cbi(PORTC, RING_PWR)
#define RINGER_POWER_DOWN()	sbi(PORTC, RING_PWR)

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))

//this is how many reads to take of the battery. 
#define AVERAGE 16
//*******************************************************
//					General Definitions
//*******************************************************
#define MAX_MESSAGE_LENGTH	128	//Buffer length for UART messages
#define	OK		1
#define	ERROR	0
#define BAUD_RATE 57600UL
#define DOUBLE_BAUD_RATE 1
#define MYUBRR  (((F_CPU / BAUD_RATE) >> (4 - DOUBLE_BAUD_RATE)) - 1)
#define HOOK_UP     0
#define HOOK_DOWN   1
#define HOOK_FLASH  2
#define HOOK_HANGUP 3
#define	ROTARY_DIALING	1
#define	ROTARY_FINISHED	0
#define STEP_SHIFT 6
#define TIMER1_SCALE_SHIFT 8
#define TIMER1_SCALE (1<<TIMER1_SCALE_SHIFT)
#define SINE_SAMPLES 255UL
#define TICKS_PER_CYCLE 256UL
#define TICKS_PER_CYCLE_SHIFT 8
#define DIAL_TONE_HIGH 440UL
#define DIAL_TONE_LOW 350UL
#define TIMER1_60MS 1875U
#define TIMER1_140MS 4375U
#define GET_UART_CHAR(x) ((x) & 0x00FF)
#define GET_UART_ERROR(x) ((x) & 0xFF00)

#define ROTARY_ENGAGED()            (!(PIND & (1<<EROTARY)))
#define ROTARY_PULSE()              (PIND & (1<<ROTARY))
#define OFF_HOOK()                  (PINC & (1<<HOOK))
#define UART_READY_INTERRUPT_IS_ON()    (UCSR0B & (1<<UDRIE0))
#define UART_READY_INTERRUPT_TURN_ON()  UCSR0B |= (1<<UDRIE0)
#define UART_READY_INTERRUPT_TURN_OFF() UCSR0B &= ~(1<<UDRIE0)


// The TIMERn_ON() macros are meant to be used with the definitions below them
#define TIMER0_ON(x) \
do { \
    TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00)); \
    TCCR0B |=(x); \
} while (0)
#define TIMER0_OFF()                TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00))
#define TIMER0_PRESCALE_1           (1<<CS00)
#define TIMER0_PRESCALE_8           (1<<CS01)
#define TIMER0_PRESCALE_64          ((1<<CS01)|(1<<CS00))
#define TIMER0_PRESCALE_256         (1<<CS02)
#define TIMER0_PRESCALE_1024        ((1<<CS02)|(1<<CS00))
#define TIMER0_EXTERNAL_FALLING     ((1<<CS02)|(1<<CS01))
#define TIMER0_EXTERNAL_RISING      ((1<<CS02)|(1<<CS01)|(1<<CS00))

#define TIMER1_ON(x) \
do { \
  TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10)); \
  TCCR1B |= (x); \
} while (0)
#define TIMER1_OFF()                TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10))
#define TIMER1_TIMEOUT_SET(x)       OCR1A = (x)
#define TIMER1_PRESCALE_1           (1<<CS10)
#define TIMER1_PRESCALE_8           (1<<CS11)
#define TIMER1_PRESCALE_64          ((1<<CS11)|(1<<CS10))
#define TIMER1_PRESCALE_256         (1<<CS12)
#define TIMER1_PRESCALE_1024        ((1<<CS12)|(1<<CS10))
#define TIMER1_EXTERNAL_FALLING     ((1<<CS12)|(1<<CS11))
#define TIMER1_EXTERNAL_RISING      ((1<<CS12)|(1<<CS11)|(1<<CS10))

#define TIMER2_ON(x) \
do { \
    TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20)); \
    TCCR2B |= (x); \
} while (0)
#define TIMER2_OFF()                TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20))
#define TIMER2_IS_ON()              !!(TCCR2B & ((1<<CS22)|(1<<CS21)|(1<<CS20)))
#define TIMER2_PRESCALE_1           (1<<CS20)
#define TIMER2_PRESCALE_8           (1<<CS21)
#define TIMER2_PRESCALE_32          ((1<<CS21)|(1<<CS20))
#define TIMER2_PRESCALE_64          (1<<CS22)
#define TIMER2_PRESCALE_128         ((1<<CS22)|(1<<CS20))
#define TIMER2_PRESCALE_256         ((1<<CS22)|(1<<CS21))
#define TIMER2_PRESCALE_1024        ((1<<CS22)|(1<<CS21)|(1<<CS20))


//=======================================================
//					Function Definitions
//=======================================================
void ioinit(void);
void delay_ms(uint16_t x);
void config_bluetooth(void);
static int uart_putchar(char c, FILE *f);
char uart_getc(void);
char uart_putc(char c);
char uart_puts(char *c);
void incoming_call(void);
void place_call(void);		
void ring_it(void);			
void short_ring_it(void);	
char string_compare(const char *search_string, const char *find_string);	
void dial_tone(void);	
char get_rotary_number(void);
void dial_number(void);	
void get_message(void);
void wait_for(const char *);
// void copy_message(void);
void start_tones(unsigned long, unsigned long);
void end_tones( void );
void pulse_tones(unsigned long, unsigned long, unsigned, unsigned);
uint16_t readBatt(void);


//                  UART Stuff
//========================================================

#define UART_PARITY_ERROR   (1<<UPE0)
#define UART_DATA_OVERRUN   (1<<DOR0)
#define UART_FRAME_ERROR    (1<<FE0)

#define DIAL_BUFFER_SIZE 32

/** Size of the circular receive buffer, must be power of 2 */
#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 256
#endif
/** Size of the circular transmit buffer, must be power of 2 */
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 32
#endif

/* test if the size of the circular buffers fits into SRAM */
#if ( (UART_RX_BUFFER_SIZE+UART_TX_BUFFER_SIZE) >= (RAMEND-0x60 ) )
#error "size of UART_RX_BUFFER_SIZE + UART_TX_BUFFER_SIZE larger than size of SRAM"
#endif

#if ( (UART_RX_BUFFER_SIZE & (UART_RX_BUFFER_SIZE-1)) )
#error "UART_RX_BUFFER_SIZE not a power of two!"
#endif

#if ( (UART_TX_BUFFER_SIZE & (UART_TX_BUFFER_SIZE-1)) )
#error "UART_TX_BUFFER_SIZE not a power of two!"
#endif


const unsigned char sine_table[] PROGMEM = {
    128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170, 173, 176, 179, 182, 185, 188, 
    190, 193, 196, 198, 201, 203, 206, 208, 211, 213, 215, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 
    237, 238, 240, 241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 
    255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246, 245, 244, 243, 241, 
    240, 238, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220, 218, 215, 213, 211, 208, 206, 203, 201, 198, 
    196, 193, 190, 188, 185, 182, 179, 176, 173, 170, 167, 165, 162, 158, 155, 152, 149, 146, 143, 140, 137, 
    134, 131, 128, 124, 121, 118, 115, 112, 109, 106, 103, 100, 97, 93, 90, 88, 85, 82, 79, 76, 73, 70, 67, 
    65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40, 37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17, 15, 14, 12, 
    11, 10, 9, 7, 6, 5, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 5, 6, 7, 9, 10, 
    11, 12, 14, 15, 17, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35, 37, 40, 42, 44, 47, 49, 52, 54, 57, 59, 62, 
    65, 67, 70, 73, 76, 79, 82, 85, 88, 90, 93, 97, 100, 103, 106, 109, 112, 115, 118, 121, 124
};