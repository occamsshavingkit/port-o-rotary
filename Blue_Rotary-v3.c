#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "BlueRotary.h"

//================================================================
//Define Global Variables
//================================================================
volatile char message[MAX_MESSAGE_LENGTH];	//Buffer for UART messages
volatile char message_complete, ring_tone_flag;	//general purpse flags
char final_message[MAX_MESSAGE_LENGTH];	//Final buffer for UART messages
volatile int message_index=0;
int dialed_number, counter;
int get_number_timeout=0;
char number_length, temp;
char phone_number[20];
int connected=0;
unsigned location_350=0, location_440=0;

//================================================================
ISR (USART_RX_vect)		//USART Receive Interrupt
{
	char character=0;
	cli();		//Disable Interrupts
	
	character=UDR0;	//Copy the character in the UART
	//Make sure we have received a valid character before copying it to the buffer
	if(isascii(character) || character==':' || character=='\n')message[message_index]=character;
	//Check to see if we've received the last character in a message
	if(message[message_index]=='\n' || message[message_index]=='\r'){
		message_complete=1;
	}
	else{
		message_index++;
	}
	sei();	//Enable Interrupts
}

//ISR (SIG_OUTPUT_COMPARE1B)	//Timer Interrupt used to generate a ringtone
ISR(TIMER0_OVF_vect)
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


int main (void)
{	
	//Initialize AVR I/O, UART and Interrupts
    ioinit();

	//Turn on the bluetooth module
	cbi(PORTD, BT_RES);	//Bring module out of reset 
	sbi(PORTC, BT_EN);	//Enable module
	LED_ON();
	_delay_ms(500);		//Allow module to stabilize
	LED_OFF();
	cbi(PORTC, RING_PWR);//Let the ringer "Warm Up"
	_delay_ms(2000);
	config_bluetooth();	//Put Blue Giga WT32 module into HFP mode
	short_ring_it();	//Give notification to user that bootup process is complete
	while(1){
	
	
		while(!connected)	//Until we're connected to a phone, listen for incoming connections
		{				
			LED_ON();

			//Clear the message buffers
			for(int i=0; i<MAX_MESSAGE_LENGTH;i++)message[i]='\0';
			for(int i=0; i<MAX_MESSAGE_LENGTH;i++)final_message[i]='\0';
			message_index=0;
			message_complete=0;		
			
			sei();		//Start looking for messages from Bluetooth
			//Wait for A message
			while(!message_complete){	//NOTE: Message is received in the UART interrupt
				_delay_ms(400);
				LED_OFF();
				_delay_ms(100);
				LED_ON();
			}
			cli();		//Stop looking for messages while we process this one!
			
			message_complete=0;
			message_index=0;
			//Copy the received message into a final message string
			for(int i=0; i<MAX_MESSAGE_LENGTH;i++)final_message[i] = message[i];			
			for(int i=0; i<MAX_MESSAGE_LENGTH;i++)message[i]='\0';
			
			//If we get a RING 0 event then we are "connected"
			if(string_compare(final_message, "RING")){			
				connected=1;	//Set the connected flag to notify program of status
				short_ring_it();	//Give user notification of established connection
				_delay_ms(250);
				short_ring_it();
			}

		}
		
		while(connected){	//If we're connected, stay in this 'routine' until we're disconnected
			LED_ON();
			sei();
			while((PINC & (1<<HOOK))!=(1<<HOOK)) //Wait for user to lift phone off hook
			{
				//If we receive a message while the phone is on the hook then evaluate it!
				if(message_complete){
					cli();
					message_complete=0;
					message_index=0;
					for(int i=0; i<MAX_MESSAGE_LENGTH;i++)final_message[i] = message[i];
					LED_OFF();
					sei();
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
						break;	//If we lose the bluetooth connection, start searching again
					}
				}
				
			}
			
			//If the phone is taken off the hook, place a call
			if(PINC & (1<<HOOK)){
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
	
	// USART Baud rate: 57600 (With 8 MHz Clock)
    UBRR0H = (MYUBRR >> 8) & 0x7F;	//Make sure highest bit(URSEL) is 0 indicating we are writing to UBRRH
	UBRR0L = MYUBRR;
    UCSR0A = (1<<U2X0);	
	UCSR0B = (1<<RXCIE0)|(1<<RXEN0)|(1<<TXEN0);	//Enable Interrupts on receive character
//    UCSR0C = (1<<URSE0L)|(1<<UCSZ0)|(1<<UCSZ1);	
    stdout = &mystdout; //Required for printf init
	cli();
    
	
	//Init timer for dial tone
    ASSR = 0;
    TCCR2A=(1<<WGM21)|(1<<WGM20);
//    TCCR0B=(1<<CS00); // this turns on the timer now!
    TIMSK2 = (1<<OCIE2A)|(1<<TOIE2)|(1<<OCIE2B);
}

//Function: config_bluetooth
//Purpose:	Initialize Bluetooth module
//Inputs:	None
//Outputs:	None
//NOTE: UART must be configured to send data at 57600bps
void config_bluetooth(void)
{   
	printf("SET CONTROL CONFIG 100\nAT\n");	//Enable SCO Links
    _delay_ms(100);
	printf("SET PROFILE HFP ON\nAT\n");		//Put iWRAP into HFP mode
    _delay_ms(100);
	printf("SET BT AUTH * 1234\nAT\n");		//Set the password
    _delay_ms(100);
	printf("SET BT CLASS 200404\nAT\n");	//Set device class
    _delay_ms(100);
	printf("SET BT NAME SPARKY\nAT\n");		//Set the bluetooth name
    _delay_ms(100);
	printf("RESET\n");
	//WT32 should now be configured and ready to accept connection from other Bluetooth devices
	//Just need to wait for a RING event to connect and establish and SCO connection.
}

static int uart_putchar(char c, FILE *stream)
{
  if (c == '\n')
    uart_putchar('\r', stream);
  
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

uint8_t uart_getchar(void)
{
    while( !(UCSR0A & (1<<RXC0)) );
	return(UDR0);
}

//Function:	delay_ms
//Purpose:	General delay. Lasts ~1ms
//Inputs:	int x - Time to delay
//Outputs:	None
void delay_ms(uint16_t x)
{
  uint8_t y, z;
  for ( ; x > 0 ; x--){
    for ( y = 0 ; y < 40 ; y++){
      for ( z = 0 ; z < 40 ; z++){
        asm volatile ("nop");
      }
    }
  }
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
		_delay_ms(200);

        printf("ANSWER\n");				//User the iWRAP command to answer the call
        while(PINC & (1<<HOOK)); 		//Wait for user to hang up
        printf("HANGUP\n");

		_delay_ms(1000);
    }
    
}

//Function:	dial_tone
//Purpose:	Generate a dial tone on the handset
//Inputs:	None
//Outputs:	None
void dial_tone(void)
{   // should be about 350 Hz
	sbi(PORTD, DT1);
    _delay_ms(50.0/35.0);    
    cbi(PORTD, DT1);
    _delay_ms(50.0/35.0);
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
//	TIMSK1 = (1<<OCIE1B);
    TCCR2B |= (1<<CS20);
    while(PIND & (1<<EROTARY)){	//If the Rotary starts spinning, get out of the dial tone
//		dial_tone();
		if((PINC & (1<<HOOK))!=(1<<HOOK)){	//If the phone is back on the hook, stop the dial tone and exit the function.
			UCSR0B |= (1<<RXCIE0);					//because there is no number to be dialed.
//			TIMSK1 &= ~(1<<OCIE0B);
            TCCR2B &= ~(1<<CS20);
			LED_OFF();
			return;
			}
			
			//If we get a message before we start dialing, check to make sure it isn't the "disconnect" message.
			if(message_complete){
				cli();
				message_complete=0;
				message_index=0;
				for(int i=0; i<MAX_MESSAGE_LENGTH;i++)final_message[i] = message[i];
				for(int i=0; i<MAX_MESSAGE_LENGTH;i++)message[i]='\0';
				sei();
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
    TCCR2B &= ~(1<<CS20);
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
		if(counter > 0)dial_number();
	}
	printf("HANGUP\n");	//After dialing number, wait for the phone to go on the hook and then hang up.
    get_number_timeout=0;
	_delay_ms(1000);		//Let the bluetooth messages stop.
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
		if(PINC & (1<<HOOK)) break;
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
    LED_ON();

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

    LED_OFF();
}

//Use to find a string within a search string
//search : "hithere\0"
//find : "the\0" - return OK
char string_compare(const char *search_string, const char *find_string)
{

    int find_spot, search_spot;
    char spot_character, search_character;
    find_spot = 0;

    for(search_spot = 0 ; ; search_spot++)
    {
        if(find_string[find_spot] == '\0') return OK; //We've reached the end of the search string - that's good!
        if(search_string[search_spot] == '\0') return ERROR; //End of string found

        spot_character = find_string[find_spot]; //Compiler limit
        search_character = search_string[search_spot]; //Compiler limit

        if(spot_character == search_character) //We found another character
            find_spot++; //Look for the next spot in the search string
        else if(find_spot > 0) //No character found, so reset the find_spot
            find_spot = 0;
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
	number_length++; //Increase number length by 1 digit
    if (dialed_number == 10) dialed_number = 0; //Correct for operator call
    //Let's make sure we counted correctly...
	if(dialed_number >= 0 && dialed_number <= 9)
    {
        //Store this number into the array
        phone_number[counter] = dialed_number + '0';
    }
    else
    {
        //Some how we got a bad number - ignore it and try again
        counter--;
    }
	return 1;
}

//Function:	dial_number
//Purpose:	Dials the number recorded in the get_rotary_number routine
//Inputs:	None
//Outputs:	None
void dial_number(void){
    if(PINC & (1<<HOOK)) //Make sure we still have the phone off hook
    {
        //Once we are here, we have the number, time to send it to the cell phone
        //May need to establish an SCO connection

        printf("\nATD");	//Do I need the \r leading?
	// \r is automatically put in!
        for(counter = 0; counter < number_length ; counter++)
            printf("%c", phone_number[counter]);
        printf(";\n");		//Should this be \r or \n?
	
        while(PINC & (1<<HOOK)); //Wait for user to hang up the phone
    }
}
