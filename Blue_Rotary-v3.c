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
volatile char message[MAX_MESSAGE_LENGTH];	//Buffer for UART messages
volatile char message_complete = 0;
volatile char ring_tone_flag;
volatile char message_unread = 0;	//general purpse flags
char final_message[MAX_MESSAGE_LENGTH];	//Final buffer for UART messages
volatile int message_index = 0;
int dialed_number, counter;
int get_number_timeout = 0;
char number_length, temp;
char phone_number[20];
volatile char hook_status = HOOK_DOWN;
volatile int connected=0;
unsigned location_350=0, location_440=0;
cbuf _rx_buf;
cbuf _tx_buf;
cbuf *rx_buf = &_rx_buf;
cbuf *tx_buf = &_tx_buf;
char uart_error;
char rx_buf_data[UART_RX_BUFFER_SIZE] = {0};
char tx_buf_data[UART_TX_BUFFER_SIZE] = {0};

ISR(USART_RX_vect)
{
    cli();
    char c = UDR0;
    cbuf_put(rx_buf, c);
    uart_error |= UCSR0A & (UART_PARITY_ERROR|UART_DATA_OVERRUN|UART_FRAME_ERROR);
    //if(UCSR0A & UART_DATA_OVERRUN) short_ring_it();
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
        LED_OFF()
        UCSR0B &= ~(1<<UDRIE0);
    }
    sei();
}

ISR(TIMER1_COMPA_vect)
{
    cli();
    if(hook_status == HOOK_FLASH){
        hook_status = HOOK_HANGUP;
        TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
    }
    else if(hook_status == HOOK_DOWN){
        hook_status = HOOK_FLASH;
        OCR1A = TIMER1_140MS;
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
    OCR2A = pgm_read_byte(&(sine_table[(location_350 >> STEP_SHIFT)]));
    location_350 += STEP_350;
    if(location_350 >= (SINE_SAMPLES << STEP_SHIFT)) location_350 -= (SINE_SAMPLES << STEP_SHIFT);
    sei();
}

ISR(TIMER2_COMPB_vect)
{
    cli();
    cbi(PORTD,DT2);
    OCR2B = pgm_read_byte(&(sine_table[(location_440 >> STEP_SHIFT)]));
    location_440 += STEP_440;
    if(location_440 >= (SINE_SAMPLES << STEP_SHIFT)) location_440 -= (SINE_SAMPLES << STEP_SHIFT);
    sei();
}

ISR(PCINT1_vect) // HOOK!
{
    cli();
    sleep_disable();
    _delay_ms(40); // debouncing, I hope!
    if(hook_status == HOOK_FLASH){
        //output stuff to run call waiting!
    }
    if(PINC & (1<<HOOK)){ // if we're off hook
        hook_status = HOOK_UP;
        TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
    } 
    else { // if we're on hook
        hook_status = HOOK_DOWN;
        OCR1A = TIMER1_60MS;
        TCCR1B |= (1<<CS12);
    }
    sei();
}

ISR(PCINT2_vect)
{
    cli();
    sleep_disable();
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
    if(!(UCSR0B & (1<<UDRIE0))){
        UCSR0B |= (1<<UDRIE0);
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
    if(message_unread) return;
    uart_recv = uart_getc();
    while(!message_complete && message_index < MAX_MESSAGE_LENGTH - 1){
        while(!uart_recv) {
            //sleep_mode();
            uart_recv = uart_getc();
        }
        if((uart_recv == '\n' || uart_recv == '\r')){
            if(message_index > 0) {
                message[message_index++] = '\0';
                message_complete = 1;
                message_unread = 1;      
                return;
            }
        } 
        else { 
            message[message_index++] = uart_recv;
        }
        uart_recv = uart_getc();
    }
}

void wait_for(const char *s){
    char match = 0;
    while(!match){
        while(!message_complete) get_message();
        copy_message();
        match = string_compare(final_message, s);
    }
}

void copy_message(void){
    for(int i = 0; i < message_index; i++){
        final_message[i] = message[i];
        message[i] = '\0';
    }
    message_complete=0;
    message_unread=0;
    message_index=0;
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
	//Initialize AVR I/O, UART and Interrupts

	//Turn on the bluetooth module
	cbi(PORTD, BT_RES);	//Bring module out of reset 
	sbi(PORTC, BT_EN);	//Enable module
	cbi(PORTC, RING_PWR);//Let the ringer "Warm Up"
    
    wait_for("READY"); // wait for the bluetooth module to say it is ready

    config_bluetooth();	//Put Blue Giga WT32 module into HFP mode
	while(1){
        sbi(PORTC, RING_PWR);
		while(!connected)	//Until we're connected to a phone, listen for incoming connections
		{				
			LED_ON();

			sei();		//Start looking for messages from Bluetooth

            wait_for("CONNECT 0 HFP");
            connected=1;	//Set the connected flag to notify program of status
            cbi(PORTC, RING_PWR);
            short_ring_it();	//Give user notification of established connection
            _delay_ms(100);
            short_ring_it();
		}
		
		while(connected){	//If we're connected, stay in this 'routine' until we're disconnected
			LED_OFF();
            // sleep until something happens
            sei();
            sbi(PORTC, RING_PWR);
            sleep_mode();
            LED_ON();

            get_message();

			//If the phone is taken off the hook, place a call
			if(hook_status == HOOK_UP){
				place_call(); //We need to dial out
			}
            
            if(message_complete){
                copy_message();
                LED_OFF();
                //Check to see if we're receiving an incoming call
                if(string_compare(final_message, "HFP 0 RING")){
                    incoming_call();	//If we're getting a RING, then answer the phone
                    for(int i=0; i<MAX_MESSAGE_LENGTH;i++)final_message[i]='\0';
                    for(int i=0; i<MAX_MESSAGE_LENGTH;i++)message[i]='\0';
                }
                //Check to see if the BT connection has been lost
                if(string_compare(final_message, "NO CARRIER 0")){
                    for(int i=0; i<MAX_MESSAGE_LENGTH;i++)message[i]='\0';
                    for(int i=0; i<MAX_MESSAGE_LENGTH;i++)final_message[i]='\0';
                    connected=0;	//We're no longer connected to a BT module!
                    message_complete=0;
                    _delay_ms(200);
                }
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
    TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
    TIMSK1 |= (1<<TOIE1)|(1<<OCIE1A);
    // pin change interrupts
    // turned on so we wake up on anything happening... I hope.
    PCICR = (1<<PCIE1);
    // PCINT19 = EROTARY // PCINT20 = ROTARY // PCINT21 = BT_UNPAIR
    // PCMSK2 = (1<<PCINT19)|(1<<PCINT20)|(1<<PCINT21);
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
	printf("SET BT AUTH * 1234\nAT\n");		//Set the password
	wait_for("OK");
	printf("SET BT CLASS 200404\nAT\n");	//Set device class
	wait_for("OK");
	printf("SET BT NAME SPARKY\nAT\n");		//Set the bluetooth name
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
    for(int i = 0 ; i < 300 ; i++)
    {
        _delay_ms(10);
        if(PINC & (1<<HOOK)) break;	//if the phone is taken off the hook, then get out of here
    }	

    if(PINC & (1<<HOOK))	//The phone has been taken off the hooK
    {
        sbi(PORTC, RING_PWR);
        printf("ANSWER\n");				//User the iWRAP command to answer the call
        wait_for("STATUS \"call\" 1");
        while(hook_status != HOOK_HANGUP){
            number_length = 0;
            counter = 0;
            if((PIND & (1<<EROTARY)) != (1<<EROTARY)){ // look for dialing, generate the proper tone
                if(get_rotary_number()){
                    printf("\nDTMF ");
                    uart_putc(phone_number[0]);
                    printf("\n");
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
	UCSR0B &= ~(1<<RXCIE0);
    DIALTONE_ON();
    while(PIND & (1<<EROTARY)){	//If the Rotary starts spinning, get out of the dial tone
        get_message();
		if((PINC & (1<<HOOK))!=(1<<HOOK)){	//If the phone is back on the hook, stop the dial tone and exit the function.
			UCSR0B |= (1<<RXCIE0);					//because there is no number to be dialed.
            DIALTONE_OFF();
			LED_OFF();
			return;
        }
			
			//If we get a message before we start dialing, check to make sure it isn't the "disconnect" message.
        if(message_complete){
            copy_message();
            if(string_compare(final_message, "NO CARRIER 0")){
                for(int i=0; i<MAX_MESSAGE_LENGTH;i++)message[i]='\0';
                for(int i=0; i<MAX_MESSAGE_LENGTH;i++)final_message[i]='\0';
                connected=0;
                message_complete=0;
                return;		//If we lose the bluetooth connection, exit the function and start looking for a new BT connection
            }
        }
	}
    UCSR0B |= (1<<RXCIE0);
    DIALTONE_OFF();
    //Begin Read Rotary
    number_length = 0;
    dialed_number = 0;
    counter = 0;	
	get_number_timeout=0;
	if(get_rotary_number()){	//If a number was dialed, then keep looking for numbers
		counter++;
		while(get_number_timeout < 40){	//Make sure we haven't reached the 4 second timeout
			dialed_number = 0;
			
			//If the rotary starts moving, collect the number
			if((PIND & (1<<EROTARY)) != (1<<EROTARY)){
				if(get_rotary_number())counter++; //Wait for user to start dialing
				get_number_timeout=0;
			}	
			_delay_ms(100);
			get_number_timeout++;
		}
		//If we've reached the timeout, and we have a number to dial, then dial it!
		if(counter > 0) dial_number();
	}
	printf("HANGUP\n");	//After dialing number, wait for the phone to go on the hook and then hang up.
    get_number_timeout=0;
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
		if(hook_status == HOOK_UP) break;
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
    char find_spot = 0;
    for(char search_spot = 0 ; search_spot < MAX_MESSAGE_LENGTH; search_spot++)
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
	while(PINC & (1<<HOOK))
	{
		
		//Now count how many times the mechnical switch toggles
		while(((PIND & (1<<ROTARY))!=(1<<ROTARY)) && (PINC & (1<<HOOK)));	
		_delay_ms(40); //Wait for switch to debounce
		LED_ON();
		
		while((PIND & (1<<ROTARY)) && (PINC & (1<<HOOK)));
		_delay_ms(40); //Wait for switch to debounce
		LED_OFF();  
		dialed_number++;
		if(PIND & (1<<EROTARY))break;

	}
    //If the phone was put back on the hook, get out of this routine!
	if((PINC & (1<<HOOK))!=(1<<HOOK)){
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
        //Store this number into the array
        number_length++; //Increase number length by 1 digit
        phone_number[counter] = dialed_number + '0';
    }
    else
    {
        //Some how we got a bad number - ignore it and try again
        counter--;
        return 0;
    }
	return 1;
}

//Function:	dial_number
//Purpose:	Dials the number recorded in the get_rotary_number routine
//Inputs:	None
//Outputs:	None
void dial_number(void){
    if(hook_status != HOOK_HANGUP) //Make sure we still have the phone off hook
    {
        //Once we are here, we have the number, time to send it to the cell phone
        //May need to establish an SCO connection

        printf("\nATD");	//Do I need the \r leading?
	// \r is automatically put in!
        for(counter = 0; counter < number_length ; counter++)
            uart_putc(phone_number[counter]);
        printf(";\n");		//Should this be \r or \n?
	
        while(hook_status != HOOK_HANGUP){ // let's see if we can have tone dials!
            number_length = 0;
            counter = 0;
            if((PIND & (1<<EROTARY)) != (1<<EROTARY)){
                if(get_rotary_number()){
                    printf("\nDTMF ");
                    uart_putc(phone_number[0]);
                    printf("\n");
                }
            }
        }//Wait for user to hang up the phone
    }
}
