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
#define	LED_ON()	cbi(PORTC, PSTAT);
#define	LED_OFF()	sbi(PORTC, PSTAT);
#define DIALTONE_ON() (TCCR2B |= (1<<CS20))
#define DIALTONE_OFF() (TCCR2B &= ~(1<<CS20))

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))
//*******************************************************
//					General Definitions
//*******************************************************
#define MAX_MESSAGE_LENGTH	64	//Buffer length for UART messages
#define	OK		1
#define	ERROR	0
#define BAUD_RATE 57600UL
#define DOUBLE_BAUD_RATE 1
#define MYUBRR  (((F_CPU / BAUD_RATE) >> (4 - DOUBLE_BAUD_RATE)) - 1)
#define	OFF_HOOK	1
#define	ON_HOOK		0
#define HOOK_UP     0
#define HOOK_DOWN   1
#define HOOK_FLASH  2
#define HOOK_HANGUP 3
#define	ROTARY_DIALING	1
#define	ROTARY_FINISHED	0
#define STEP_SHIFT 7
#define SINE_SAMPLES 255U 
#define TICKS_PER_CYCLE 256U
// see http://www.atmel.com/Images/doc1982.pdf
#define STEP_350 (350UL * SINE_SAMPLES * (TICKS_PER_CYCLE << STEP_SHIFT) / F_CPU)
#define STEP_440 (440UL * SINE_SAMPLES * (TICKS_PER_CYCLE << STEP_SHIFT) / F_CPU)
#define TIMER1_60MS 1875U
#define TIMER1_140MS 4375U
#define GET_UART_CHAR(x) ((x) & 0x00FF)
#define GET_UART_ERROR(x) ((x) & 0xFF00)

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
void copy_message(void);

//                  UART Stuff
//========================================================

#define UART_PARITY_ERROR   (1<<UPE0)
#define UART_DATA_OVERRUN   (1<<DOR0)
#define UART_FRAME_ERROR    (1<<FE0)

/** Size of the circular receive buffer, must be power of 2 */
#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 32
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

#if ( (UART_TX_BUFFER_SIZE & (UART_RX_BUFFER_SIZE-1)) )
#error "UART_TX_BUFFER_SIZE not a power of two!"
#endif


const unsigned char sine_table[] PROGMEM = {128,
    131,
    134,
    137,
    140,
    143,
    146,
    149,
    152,
    156,
    159,
    162,
    165,
    168,
    171,
    174,
    176,
    179,
    182,
    185,
    188,
    191,
    193,
    196,
    199,
    201,
    204,
    206,
    209,
    211,
    213,
    216,
    218,
    220,
    222,
    224,
    226,
    228,
    230,
    232,
    234,
    235,
    237,
    239,
    240,
    242,
    243,
    244,
    246,
    247,
    248,
    249,
    250,
    251,
    251,
    252,
    253,
    253,
    254,
    254,
    254,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    254,
    254,
    253,
    253,
    252,
    252,
    251,
    250,
    249,
    248,
    247,
    246,
    245,
    244,
    242,
    241,
    239,
    238,
    236,
    235,
    233,
    231,
    229,
    227,
    225,
    223,
    221,
    219,
    217,
    215,
    212,
    210,
    207,
    205,
    202,
    200,
    197,
    195,
    192,
    189,
    186,
    184,
    181,
    178,
    175,
    172,
    169,
    166,
    163,
    160,
    157,
    154,
    151,
    148,
    145,
    142,
    138,
    135,
    132,
    129,
    126,
    123,
    120,
    117,
    113,
    110,
    107,
    104,
    101,
    98,
    95,
    92,
    89,
    86,
    83,
    80,
    77,
    74,
    71,
    69,
    66,
    63,
    60,
    58,
    55,
    53,
    50,
    48,
    45,
    43,
    40,
    38,
    36,
    34,
    32,
    30,
    28,
    26,
    24,
    22,
    20,
    19,
    17,
    16,
    14,
    13,
    11,
    10,
    9,
    8,
    7,
    6,
    5,
    4,
    3,
    3,
    2,
    2,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    2,
    2,
    3,
    4,
    4,
    5,
    6,
    7,
    8,
    9,
    11,
    12,
    13,
    15,
    16,
    18,
    20,
    21,
    23,
    25,
    27,
    29,
    31,
    33,
    35,
    37,
    39,
    42,
    44,
    46,
    49,
    51,
    54,
    56,
    59,
    62,
    64,
    67,
    70,
    73,
    76,
    79,
    81,
    84,
    87,
    90,
    93,
    96,
    99,
    103,
    106,
    109,
    112,
    115,
    118,
    121,
    124};