#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "BlueRotary.h"
#include "circular_buffer.h"

//================================================================
//Define Global Variables
//================================================================
FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
char message[MAX_MESSAGE_LENGTH];	//Buffer for UART messages
volatile char ring_tone_flag;
volatile char lines_available = 0;
volatile unsigned char portdhistory = 0xFF;
uint8_t dialed_number;
int get_number_timeout = 0;
uint16_t tone_time_off, tone_time_on;
uint16_t tone_a_step, tone_b_step;
uint16_t tone_a_place, tone_b_place;
uint8_t temp;
volatile char hook_status = HOOK_DOWN;
volatile char connected=0;
cbuf _rx_buf;
cbuf _tx_buf;
cbuf _dial_buf;
cbuf *rx_buf = &_rx_buf;
cbuf *tx_buf = &_tx_buf;
cbuf *dial_buf = &_dial_buf;
char uart_error;
char rx_buf_data[UART_RX_BUFFER_SIZE] = {0};
char tx_buf_data[UART_TX_BUFFER_SIZE] = {0};
char dial_buf_data[DIAL_BUFFER_SIZE] = {0};

ISR(USART_RX_vect)
{
    cli();
    char c = UDR0;
    if(c != '\r'){
        cbuf_put(rx_buf, c);
        uart_error |= UCSR0A & (UART_PARITY_ERROR|UART_DATA_OVERRUN|UART_FRAME_ERROR);
    } 
    else lines_available++;
    sei();
}

ISR(USART_UDRE_vect)
{
    cli();
    char c = 0;
    if(!cbuf_is_empty(tx_buf)){
        c = cbuf_get(tx_buf);
        if(c) {
            UDR0 = c;
        }
    }
    if(cbuf_is_empty(tx_buf)) {
        LED_OFF();
        UART_READY_INTERRUPT_TURN_OFF();
    }
    sei();
}

ISR(TIMER1_COMPA_vect)
{
    cli();
    cbi(PORTC, RING_PWR);
    short_ring_it();
    sbi(PORTC, RING_PWR);
    if(hook_status == HOOK_FLASH){
        hook_status = HOOK_HANGUP;
        LED_OFF();
        TIMER1_OFF();
    }
    else if(hook_status == HOOK_DOWN){
        hook_status = HOOK_FLASH;
        TIMER1_TIMEOUT_SET(TIMER1_140MS);
    } else if(hook_status == HOOK_UP){
        if(TIMER2_IS_ON()){
            TONES_OFF();
            TIMER1_TIMEOUT_SET(tone_time_off);
        }
        else{
            TONES_ON();
            TIMER1_TIMEOUT_SET(tone_time_on);
        }
    }
    sei();
    
}

ISR(TIMER2_OVF_vect)
{
	cli();
    sbi(PORTD,DT1);
    sbi(PORTD,DT2);
	sei();
}

ISR(TIMER2_COMPA_vect)
{
    cli();
    cbi(PORTD,DT1);
    OCR2A = pgm_read_byte(&(sine_table[(tone_a_place >> STEP_SHIFT)]));
    tone_a_place += tone_a_step;
    if(tone_a_place >= (SINE_SAMPLES << STEP_SHIFT)) tone_a_place -= (SINE_SAMPLES << STEP_SHIFT);
    sei();
}

ISR(TIMER2_COMPB_vect)
{
    cli();
    cbi(PORTD,DT2);
    OCR2B = pgm_read_byte(&(sine_table[(tone_b_place >> STEP_SHIFT)]));
    tone_b_place += tone_b_step;
    if(tone_b_place >= (SINE_SAMPLES << STEP_SHIFT)) tone_b_place -= (SINE_SAMPLES << STEP_SHIFT);
    sei();
}

ISR(PCINT1_vect) // HOOK!
{
    cli();
    sleep_disable();
    /*
    _delay_ms(40); // debouncing, I hope!
    if(hook_status == HOOK_FLASH){
        //output stuff to run call waiting!
    }
    if(OFF_HOOK()){ // if we're off hook
        hook_status = HOOK_UP;
        TIMER1_OFF();
    } 
    else { // if we're on hook
        end_tones();
        hook_status = HOOK_DOWN;
        TIMER1_TIMEOUT_SET(TIMER1_60MS);
        TIMER1_ON(TIMER1_PRESCALE_256);
    }
     */
    sei();
}

ISR(PCINT2_vect)
{
    cli();
    sleep_disable();
    char portdchange = PIND ^ portdhistory;
    portdhistory = PIND;

    if(portdchange & BT_UNPAIR){
        printf("SET BT PAIR *");
    }
    
    sei();
}

ISR(WDT_vect)
{
    cli();
    sei();
}

char uart_getc(){
    if(cbuf_is_empty(rx_buf)) {
        return 0;
    }
    char c = cbuf_get(rx_buf);
    return c;
}

char uart_putc(char c){
    if(!UART_READY_INTERRUPT_IS_ON()){
        UART_READY_INTERRUPT_TURN_ON();
    }
    cbuf_put(tx_buf, c);
    LED_ON();
    return tx_buf->error;
}

char uart_puts(char *c){
    for(int i = 0; c[i] != '\0' && i < MAX_MESSAGE_LENGTH; i++){
        if(c[i]== '\n') uart_putc('\r');
        uart_putc(c[i]);
    }
    return 0;
}

void get_message() {
    char uart_recv = 0;
    uint8_t message_complete = 0;
    uint8_t message_index = 0;
    uart_recv = uart_getc();
    while(!message_complete && message_index < MAX_MESSAGE_LENGTH - 1){
        while(!uart_recv) {
            sleep_mode();
            uart_recv = uart_getc();
        }
        if(uart_recv == '\n'){
            lines_available--;
            char next_char = cbuf_peek(rx_buf);
            while(next_char == '\n'){
                lines_available--;
                uart_getc();
                next_char = cbuf_peek(rx_buf);
            }
            if(message_index > 0) {
                message[message_index++] = '\0';
                message_complete = 1;
                return;
            }
        } 
        else { 
            message[message_index++] = uart_recv;
        }
        uart_recv = uart_getc();
    }
}


// This function waits for a specific reply from the BT module.
// It doesn't look to see if there's anything else we should pay attention to.
// This is a problem if we never see what we're looking for.
// Or also if we ignore something that needs our attention (losing the BT connection, for example)
// Which is why we really need an interpreter for what the BT module says.
void wait_for(const char *s){
    char match = 0;
    while(!match){
        while(!lines_available) sleep_mode();
        get_message();
        match = string_compare(message, s);
    }
}
 
static int uart_putchar(char c, FILE *F){
    if (c == '\n'){
        uart_putc('\r');
    }
    uart_putc(c);
    return 0;
}

int main (void)
{	
    stdout = &mystdout;
    ioinit();
    cbuf_init(rx_buf, UART_RX_BUFFER_SIZE, rx_buf_data);
    cbuf_init(tx_buf, UART_TX_BUFFER_SIZE, tx_buf_data);
    cbuf_init(dial_buf, DIAL_BUFFER_SIZE, dial_buf_data);
	//Initialize AVR I/O, UART and Interrupts

	//Turn on the bluetooth module
	cbi(PORTD, BT_RES);	//Bring module out of reset 
	sbi(PORTC, BT_EN);	//Enable module
	cbi(PORTC, RING_PWR);//Let the ringer "Warm Up"
    
    wait_for("READY"); // wait for the bluetooth module to say it is ready
    config_bluetooth();	//Put Blue Giga WT32 module into HFP mode
    wait_for("READY"); // wait for the bluetooth module to say it is ready

	while(1){
        sbi(PORTC, RING_PWR);
		while(!connected)	//Until we're connected to a phone, listen for incoming connections
		{				
			LED_ON();

			sei();		//Start looking for messages from Bluetooth

            wait_for("NO CARRIER 1");
            connected=1;	//Set the connected flag to notify program of status
            cbi(PORTC, RING_PWR);
            short_ring_it();	//Give user notification of established connection
            _delay_ms(100);
            short_ring_it();
            sbi(PORTC, RING_PWR);
		}
		
		while(connected){	//If we're connected, stay in this 'routine' until we're disconnected
            sbi(PORTC, RING_PWR);
			LED_OFF();
            // sleep until something happens
            sei();
            sleep_mode();
            LED_ON();
            cbi(PORTC, RING_PWR);

            while(lines_available){
                get_message();
                //Check to see if we're receiving an incoming call
                if(string_compare(message, "HFP 0 RING")){
                    incoming_call();	//If we're getting a RING, then answer the phone
                }
                //Check to see if the BT connection has been lost
                if(string_compare(message, "NO CARRIER 0")){
                    connected=0;	//We're no longer connected to a BT module!
                }
            }				
			
            if(OFF_HOOK()){
				place_call(); //We need to dial out
			}
		}
	}			
    return (0);
}

//==================================================
//Core functions
//==================================================
//Function: ioinit
//Purpose:	Initialize AVR I/O, UART and Interrupts
//Inputs:	None
//Outputs:	None
void ioinit(void)
{

    // clear the watchdog so we don't get stuck ever
    cli();
    MCUSR &= ~(1<<WDRF);
    WDTCSR |= (1<<WDCE)|(1<<WDE);
    WDTCSR = 0x00;
    //WDTCSR |= (1<<WDCE)|(1<<WDE);
    //WDTCSR = (1<<WDIE);
    sei();
    
    // handle power saving
    power_all_disable();
    power_timer0_enable();
    power_timer1_enable();
    power_timer2_enable();
    power_usart0_enable();
    
    set_sleep_mode(SLEEP_MODE_IDLE);
        
    //1 = output, 0 = input
	DDRB = (1<<CS) | (1<<SCK) | (1<<MOSI);										//Define Port B GPIO Outputs
    PORTB = (1<<MISO);
	DDRB &= ~(1<<MISO);
	
	DDRC = (1<<BT_EN) | (1<<RING_PWR) | (1<<RING1) | (1<<RING2) |  (1<<PSTAT); 	//Define Port C Outputs
	PORTC = (1<<HOOK);
	DDRC &= ~(1<<HOOK);															//Define Port C Inputs
	
	DDRD = (1<<DT1) | (1<<DT2) | (1<< BT_RES);										//Define Port D Outputs
	PORTD = (1<<ROTARY) | (1<<EROTARY);	
    DDRD &= ~((1<<ROTARY)|(1<<EROTARY));								//Define Port D inputs

    //SPI Bus setup
	//SPCR = (1<<SPE)|(1<<MSTR)|(1<<CPHA)|(1<<CPOL)|(1<<SPR0);	//Might need to change the phase

    UBRR0H = (MYUBRR >> 8) & 0x7F;	//Make sure highest bit(URSEL) is 0 indicating we are writing to UBRRH
	UBRR0L = MYUBRR;
    if(DOUBLE_BAUD_RATE) UCSR0A |= (1<<U2X0);	
	UCSR0B = (1<<RXCIE0)|(1<<UDRIE0)|(1<<RXEN0)|(1<<TXEN0);	//Enable Interrupts on receive character    
    //UCSR0C = (1<<UCSZ00)|(1<<UCSZ01);	
    stdout = &mystdout;

	//Init timer for dial tone
    ASSR = 0;
    TCCR2A=(1<<WGM21)|(1<<WGM20);
    TIMSK2 = (1<<OCIE2A)|(1<<TOIE2)|(1<<OCIE2B);

    // Init timer for hook flash
    TCCR1A= 0x00;
    TCCR1B=(1<<WGM12);
    
    TIMER1_OFF();    
    
    TIMSK1 |= (1<<TOIE1)|(1<<OCIE1A);
    
    // pin change interrupts
    // turned on so we wake up on anything happening... I hope.

    PCICR = (1<<PCIE1)|(1<<PCIE2);
    // PCINT19 = EROTARY // PCINT20 = ROTARY // PCINT21 = BT_UNPAIR
    PCMSK2 = (1<<PCINT19)|(1<<PCINT20)|(1<<PCINT21);
    portdhistory = PIND;
    // PCINT8 = HOOK
    PCMSK1 = (1<<PCINT8);


}

//Function: config_bluetooth
//Purpose:	Initialize Bluetooth module
//Inputs:	None
//Outputs:	None
//NOTE: UART must be configured to send data at 57600bps
void config_bluetooth(void)
{   
    printf("SET CONTROL CONFIG 100\nAT\n");	//Enable SCO Links
	wait_for("OK");
    printf("SET PROFILE HFP ON\nAT\n");		//Put iWRAP into HFP mode
	wait_for("OK");
    printf("SET CONTROL AUTOCALL 111F 15000 HFP\nAT\n");
	wait_for("OK");
#ifndef BT_PASSWORD
	printf("SET BT AUTH * 1234\nAT\n");		//Set the password
#else
    printf("SET BT AUTH * ");
    printf(BT_PASSWORD);
    printf("\nAT\n");
#endif
	wait_for("OK");
	printf("SET BT CLASS 200404\nAT\n");	//Set device class
	wait_for("OK");
#ifndef BT_NAME
	printf("SET BT NAME SPARKY\nAT\n");		//Set the bluetooth name
#else
    printf("SET BT NAME ");
    printf(BT_NAME);
    printf("\nAT\n");
#endif
	wait_for("OK");
	printf("RESET\n");
	//WT32 should now be configured and ready to accept connection from other Bluetooth devices
	//Just need to wait for a RING event to connect and establish and SCO connection.
}

//Function:	incoming_call
//Purpose:	Rings the bell until phone is off the hook, then commands the BT module to answer the call
//			When user replaces handset on hook, command is issued to end the call
//Inputs:	None
//Outputs:	None
void incoming_call(void)
{
    cbi(PORTC, RING_PWR);	//Power on Ringer Circuit

	ring_it();
    for(int i = 0 ; i < 300; i++)
    {
        _delay_ms(10);
        if(OFF_HOOK()) break;	//if the phone is taken off the hook, then get out of here
    }	

    if(OFF_HOOK())	//The phone has been taken off the hooK
    {
        sbi(PORTC, RING_PWR);
        printf("ANSWER\n");				//User the iWRAP command to answer the call
        wait_for("STATUS \"call\" 1");
        while(OFF_HOOK() && connected){
            if(ROTARY_ENGAGED()){ // look for dialing, generate the proper tone
                if(get_rotary_number()){
                    printf("\nDTMF ");
                    uart_putc(cbuf_get(dial_buf));
                    printf("\n");
                }
            }
            if(lines_available){
                get_message();
                if(string_compare(message, "NO CARRIER 0")){
                    connected = 0;
                }
                if(string_compare(message, "STATUS \"call\" 0")){
                    return; 
                }
            }
        } 		//Wait for user to hang up
        printf("HANGUP\n");
        wait_for("STATUS \"call\" 0");
    }
    
}

//Function:	place_call
//Purpose:	Plays dial tone until number is pressed. Records dialed numbers until a 4 second pause is recorded. Dials recorded number
//Inputs:	None
//Outputs:	None
void place_call(void)
{
    LED_ON();
    
    //Play dial tone until the rotary is touched or phone is hung up
    start_tones(DIAL_TONE_LOW, DIAL_TONE_HIGH);
    while(!ROTARY_ENGAGED()){	//keep waiting for messages until the user starts dialing
		if(!OFF_HOOK()){	//If the phone is back on the hook, stop the dial tone and exit the function.
            end_tones();
			return;
        }

        if(lines_available){ 
            get_message();
            if(string_compare(message, "NO CARRIER 0")){
                connected=0;
                return;		//If we lose the bluetooth connection, exit the function and start looking for a new BT connection
            }
        }
    }
        
    end_tones();
    dialed_number = 0;
	get_number_timeout=0;
	if(get_rotary_number()){	//If a number was dialed, then keep looking for numbers
		while(get_number_timeout < 40){	//Make sure we haven't reached the 4 second timeout
			dialed_number = 0;
			
			//If the rotary starts moving, collect the number
			if(ROTARY_ENGAGED()){
				get_rotary_number(); //Wait for user to start dialing
				get_number_timeout=0;
			}	
			_delay_ms(100);
			get_number_timeout++;
		}
		//If we've reached the timeout, and we have a number to dial, then dial it!
		if(!cbuf_is_empty(dial_buf)) dial_number();
    }
    get_number_timeout=0;
    wait_for("NO CARRIER 1");
	LED_OFF();
}

//Toggle the ring pins at 20Hz or 50ms
//Function:	ring_it
//Purpose:	Rings the bell
//Inputs:	None
//Outputs:	None
void ring_it(void)
{
    LED_ON();

    for(char i = 0 ; i < 50 ; i++)
    {
        sbi(PORTC, RING1);
		cbi(PORTC, RING2);
		_delay_ms(25.0);
    
		cbi(PORTC, RING1);
        sbi(PORTC, RING2);
        _delay_ms(25.0);
		if(OFF_HOOK()) break;
    } 

    cbi(PORTC, RING1);
    cbi(PORTC, RING2);

    LED_OFF();
}

//Toggle the ring pins at 20Hz or 50ms
//Function:	short_ring_it
//Purpose:	Rings the bell for a shorter period of time than 'ring_it.' Used for notification rings.
//Inputs:	None	
//Outputs:	None
void short_ring_it(void)
{    
    for(char i = 0 ; i < 1 ; i++)
    {
        sbi(PORTC, RING1);
        cbi(PORTC, RING2);
        _delay_ms(25.0);
        
        cbi(PORTC, RING1);
        sbi(PORTC, RING2);
        _delay_ms(25.0);
    } 

    cbi(PORTC, RING1);
    cbi(PORTC, RING2);
}

//Use to find a string within a search string
//search : "hithere\0"
//find : "the\0" - return OK
char string_compare(const char *search_string, const char *find_string)
{
    unsigned char find_spot = 0;
    for(unsigned char search_spot = 0 ; search_spot < MAX_MESSAGE_LENGTH; search_spot++)
    {
        if(find_string[find_spot] == '\0'){
            return OK; //We've reached the end of the search string - that's good!
        }
        if(search_string[search_spot] == '\0'){ 
            return ERROR; //End of string found
        }

        if(find_string[find_spot] == search_string[search_spot]){ //We found another character
            find_spot++; //Look for the next spot in the search string
        }
        else find_spot = 0;
    }

    return 0;
}

//Function:	get_rotary_number
//Purpose:	Determines which number has been dialed on the phone. Adds the number to the PhoneNumber array used for dialing out
//Inputs:	None
//Outputs:	None
char get_rotary_number(void){
	_delay_ms(50); //Wait for switch to debounce
	
	//Count the rotary "spins" until it's done rotating
    dialed_number = 0;
	while(OFF_HOOK() && ROTARY_ENGAGED())
	{
		
		//Now count how many times the mechnical switch toggles
		while(!ROTARY_PULSE() && OFF_HOOK());	
		_delay_ms(40); //Wait for switch to debounce
		LED_ON();
		
		while(ROTARY_PULSE() && OFF_HOOK());
		_delay_ms(40); //Wait for switch to debounce
		LED_OFF();  
		dialed_number++;
	}
    //If the phone was put back on the hook, get out of this routine!
	if(!OFF_HOOK()){
		LED_OFF();
		return 0;
    }
	
	//We have a new number!
    if (dialed_number == 12) dialed_number = '*' - '0';
    if (dialed_number == 11) dialed_number = '#' - '0';
    if (dialed_number == 10) dialed_number = 0; //Correct for operator call
    //Let's make sure we counted correctly...
	if(dialed_number == '*' - '0' || dialed_number == '#' - '0' || (dialed_number >= 0 && dialed_number <= 9))
    {
        if(cbuf_is_full(dial_buf)) short_ring_it(); //let the user know that the buffer is full
        else cbuf_put(dial_buf, dialed_number + '0');
    }
    else
    {
        //Some how we got a bad number - ignore it and try again
        return 0;
    }
	return 1;
}

//Function:	dial_number
//Purpose:	Dials the number recorded in the get_rotary_number routine
//Inputs:	None
//Outputs:	None
void dial_number(void){
    if(OFF_HOOK()) //Make sure we still have the phone off hook
    {
        //Once we are here, we have the number, time to send it to the cell phone
        //May need to establish an SCO connection

        printf("\nATD");
        while(!cbuf_is_empty(dial_buf))
            uart_putc(cbuf_get(dial_buf));
        printf(";\n");		//Should this be \r or \n?
	
        while(OFF_HOOK()){ // let's see if we can have tone dials!
            if(ROTARY_ENGAGED()){
                if(get_rotary_number()){
                    printf("\nDTMF ");
                    uart_putc(cbuf_get(dial_buf));
                    printf("\n");
                }
            }
        }//Wait for user to hang up the phone
        printf("HANGUP\n");
    }
}

void start_tones(uint32_t freq_a, uint32_t freq_b){
    tone_a_place = 0;
    tone_b_place = 0;
    tone_a_step = (freq_a * SINE_SAMPLES * (TICKS_PER_CYCLE << STEP_SHIFT)) / F_CPU;
    tone_b_step = (freq_b * SINE_SAMPLES * (TICKS_PER_CYCLE << STEP_SHIFT)) / F_CPU;    
    TONES_ON();
}

void end_tones(){
    TONES_OFF();
}

// Note that you can only use this when the phone is off the hook!
void pulse_tones(uint32_t freq_a, uint32_t freq_b, uint16_t ms_on, uint16_t ms_off){
    // 1000 ms in 1 s. It isn't a magic number, I swear!
    tone_time_on = ms_on * (F_CPU >> TIMER1_SCALE_SHIFT) / 1000UL;
    tone_time_off = ms_off * (F_CPU >> TIMER1_SCALE_SHIFT) / 1000UL;
    start_tones(freq_a, freq_b);
    TIMER1_TIMEOUT_SET(tone_time_on);
    TIMER1_ON(TIMER1_PRESCALE_256);
    
}


