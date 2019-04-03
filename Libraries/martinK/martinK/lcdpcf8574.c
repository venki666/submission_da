/*
lcdpcf8574 lib 0x01

copyright (c) Davide Gironi, 2013

Released under GPLv3.
Please refer to LICENSE file for licensing information.
*/
#ifndef F_CPU
#define F_CPU 16000000L
#endif
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdarg.h>

#include "pcf8574.h"

#include "lcdpcf8574.h"

#define lcd_e_delay()   __asm__ __volatile__( "rjmp 1f\n 1:" );
#define lcd_e_toggle()  toggle_e()

#if LCD_LINES==1
#define LCD_FUNCTION_DEFAULT    LCD_FUNCTION_4BIT_1LINE 
#else
#define LCD_FUNCTION_DEFAULT    LCD_FUNCTION_4BIT_2LINES 
#endif

volatile uint8_t dataport = 0;

/* 
** function prototypes 
*/
static void toggle_e(void);

/*
** local functions
*/



/*************************************************************************
 delay loop for small accurate delays: 16-bit counter, 4 cycles/loop
*************************************************************************/
static inline void _delayFourCycles(unsigned int __count)
{
    if ( __count == 0 )    
        __asm__ __volatile__( "rjmp 1f\n 1:" );    // 2 cycles
    else
        __asm__ __volatile__ (
    	    "1: sbiw %0,1" "\n\t"                  
    	    "brne 1b"                              // 4 cycles/loop
    	    : "=w" (__count)
    	    : "0" (__count)
    	   );
}


/************************************************************************* 
delay for a minimum of <us> microseconds
the number of loops is calculated at compile-time from MCU clock frequency
*************************************************************************/
#define delay(us)  _delayFourCycles( ( ( 1*(F_CPU/4000) )*us)/1000 )


/* toggle Enable Pin to initiate write */
static void toggle_e(void)
{
	pcf8574_setoutputpinhigh(LCD_PCF8574_DEVICEID, LCD_E_PIN);
	lcd_e_delay();
	pcf8574_setoutputpinlow(LCD_PCF8574_DEVICEID, LCD_E_PIN);
}


/*************************************************************************
Low-level function to write byte to LCD controller
Input:    data   byte to write to LCD
          rs     1: write data    
                 0: write instruction
Returns:  none
*************************************************************************/
static void lcd_write(uint8_t data,uint8_t rs) 
{
	if (rs) /* write data        (RS=1, RW=0) */
		dataport |= _BV(LCD_RS_PIN);
	else /* write instruction (RS=0, RW=0) */
		dataport &= ~_BV(LCD_RS_PIN);
	dataport &= ~_BV(LCD_RW_PIN);
	//dataport |= _BV(LCD_LED_PIN); 
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);

	dataport &= ~0xf0; 
	dataport |= data & 0xf0; 
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);
	lcd_e_toggle();

	/* output low nibble */
	dataport &= ~0xf0; 
	dataport |= data << 4; 
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);
	lcd_e_toggle();

	dataport |= 0xf0; 
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);
}


/*************************************************************************
Low-level function to read byte from LCD controller
Input:    rs     1: read data    
                 0: read busy flag / address counter
Returns:  byte read from LCD controller
*************************************************************************/
#include "uart.h"
static uint8_t lcd_read(uint8_t rs) 
{
  uint8_t data = 0;
	if (rs) // data
		dataport |= _BV(LCD_RS_PIN);
	else // instruction
		dataport &= ~_BV(LCD_RS_PIN);
	dataport |= _BV(LCD_RW_PIN);
	dataport &= ~_BV(LCD_E_PIN); 
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);

	
	pcf8574_setoutputpinhigh(LCD_PCF8574_DEVICEID, LCD_E_PIN);
	lcd_e_delay();
	data = pcf8574_getoutputpin(LCD_PCF8574_DEVICEID, LCD_DATA3_PIN) << 4; /* read high nibble first */
	pcf8574_setoutputpinlow(LCD_PCF8574_DEVICEID, LCD_E_PIN);
	lcd_e_delay(); /* Enable 500ns low */
	pcf8574_setoutputpinhigh(LCD_PCF8574_DEVICEID, LCD_E_PIN);
	lcd_e_delay();
	data |= pcf8574_getoutputpin(LCD_PCF8574_DEVICEID, LCD_DATA3_PIN) &0x0F; /* read low nibble */
	pcf8574_setoutputpinlow(LCD_PCF8574_DEVICEID, LCD_E_PIN);
	
	//uart_printf("%02x ", data); 
	return data;
}


/*************************************************************************
loops while lcd is busy, returns address counter
*************************************************************************/
static uint8_t lcd_waitbusy(void)

{
	register uint8_t c;
	
	/* wait until busy flag is cleared */
	while ( (c=lcd_read(0)) & (1<<LCD_BUSY)) {}
	
	//delay(2000); 
	/* the address counter is updated 4us after the busy flag is cleared */
	delay(2);

	/* now read the address counter */
	return (lcd_read(0));  // return address counter
}/* lcd_waitbusy */


/*************************************************************************
Move cursor to the start of next line or to the first line if the cursor 
is already on the last line.
*************************************************************************/
static inline void lcd_newline(uint8_t pos)
{
    register uint8_t addressCounter;


#if LCD_LINES==1
    addressCounter = 0;
#endif
#if LCD_LINES==2
    if ( pos < (LCD_START_LINE2) )
        addressCounter = LCD_START_LINE2;
    else
        addressCounter = LCD_START_LINE1;
#endif
#if LCD_LINES==4
    if ( pos < LCD_START_LINE3 )
        addressCounter = LCD_START_LINE2;
    else if ( (pos >= LCD_START_LINE2) && (pos < LCD_START_LINE4) )
        addressCounter = LCD_START_LINE3;
    else if ( (pos >= LCD_START_LINE3) && (pos < LCD_START_LINE2) )
        addressCounter = LCD_START_LINE4;
    else 
        addressCounter = LCD_START_LINE1;
#endif
    lcd_command((1<<LCD_DDRAM)+addressCounter);

}/* lcd_newline */


/*
** PUBLIC FUNCTIONS 
*/

/*************************************************************************
Send LCD controller instruction command
Input:   instruction to send to LCD controller, see HD44780 data sheet
Returns: none
*************************************************************************/
void lcd_command(uint8_t cmd)
{
    lcd_waitbusy();
    lcd_write(cmd,0);
}


/*************************************************************************
Send data byte to LCD controller 
Input:   data to send to LCD controller, see HD44780 data sheet
Returns: none
*************************************************************************/
void lcd_data(uint8_t data)
{
    lcd_waitbusy();
    lcd_write(data,1);
}



/*************************************************************************
Set cursor to specified position
Input:    x  horizontal position  (0: left most position)
          y  vertical position    (0: first line)
Returns:  none
*************************************************************************/
void lcd_gotoxy(uint8_t x, uint8_t y)
{
#if LCD_LINES==1
    lcd_command((1<<LCD_DDRAM)+LCD_START_LINE1+x);
#endif
#if LCD_LINES==2
    if ( y==0 ) 
        lcd_command((1<<LCD_DDRAM)+LCD_START_LINE1+x);
    else
        lcd_command((1<<LCD_DDRAM)+LCD_START_LINE2+x);
#endif
#if LCD_LINES==4
    if ( y==0 )
        lcd_command((1<<LCD_DDRAM)+LCD_START_LINE1+x);
    else if ( y==1)
        lcd_command((1<<LCD_DDRAM)+LCD_START_LINE2+x);
    else if ( y==2)
        lcd_command((1<<LCD_DDRAM)+LCD_START_LINE3+x);
    else /* y==3 */
        lcd_command((1<<LCD_DDRAM)+LCD_START_LINE4+x);
#endif

}/* lcd_gotoxy */


/*************************************************************************
*************************************************************************/
int lcd_getxy(void)
{
    return lcd_waitbusy();
}


/*************************************************************************
Clear display and set cursor to home position
*************************************************************************/
void lcd_clrscr(void)
{
    lcd_command(1<<LCD_CLR);
}


/*************************************************************************
Set illumination pin
*************************************************************************/
void lcd_led(uint8_t onoff)
{
	if(onoff)
		dataport |= _BV(LCD_LED_PIN);
	else
		dataport &= ~_BV(LCD_LED_PIN);
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);
}


/*************************************************************************
Set cursor to home position
*************************************************************************/
void lcd_home(void)
{
    lcd_command(1<<LCD_HOME);
}


/*************************************************************************
Display character at current cursor position 
Input:    character to be displayed                                       
Returns:  none
*************************************************************************/
void lcd_putc(char c)
{
    uint8_t pos;

    pos = lcd_waitbusy();   // read busy-flag and address counter
    if (c=='\n')
    {
        lcd_newline(pos);
    }
    else
    {
#if LCD_WRAP_LINES==1
#if LCD_LINES==1
        if ( pos == LCD_START_LINE1+LCD_DISP_LENGTH ) {
            lcd_write((1<<LCD_DDRAM)+LCD_START_LINE1,0);
        }
#elif LCD_LINES==2
        if ( pos == LCD_START_LINE1+LCD_DISP_LENGTH ) {
            lcd_write((1<<LCD_DDRAM)+LCD_START_LINE2,0);    
        }else if ( pos == LCD_START_LINE2+LCD_DISP_LENGTH ){
            lcd_write((1<<LCD_DDRAM)+LCD_START_LINE1,0);
        }
#elif LCD_LINES==4
        if ( pos == LCD_START_LINE1+LCD_DISP_LENGTH ) {
            lcd_write((1<<LCD_DDRAM)+LCD_START_LINE2,0);    
        }else if ( pos == LCD_START_LINE2+LCD_DISP_LENGTH ) {
            lcd_write((1<<LCD_DDRAM)+LCD_START_LINE3,0);
        }else if ( pos == LCD_START_LINE3+LCD_DISP_LENGTH ) {
            lcd_write((1<<LCD_DDRAM)+LCD_START_LINE4,0);
        }else if ( pos == LCD_START_LINE4+LCD_DISP_LENGTH ) {
            lcd_write((1<<LCD_DDRAM)+LCD_START_LINE1,0);
        }
#endif
        lcd_waitbusy();
#endif
        lcd_write(c, 1);
    }

}/* lcd_putc */


/*************************************************************************
Display string without auto linefeed 
Input:    string to be displayed
Returns:  none
*************************************************************************/
void lcd_puts(const char *s)
/* print string on lcd (no auto linefeed) */
{
    register char c;

    while ( (c = *s++) ) {
        lcd_putc(c);
    }

}/* lcd_puts */


/*************************************************************************
Display string from program memory without auto linefeed 
Input:     string from program memory be be displayed                                        
Returns:   none
*************************************************************************/
void lcd_puts_p(const char *progmem_s)
/* print string from program memory on lcd (no auto linefeed) */
{
    register char c;

    while ( (c = pgm_read_byte(progmem_s++)) ) {
        lcd_putc(c);
    }

}/* lcd_puts_p */


uint16_t lcd_printf(const char *fmt, ...){
	char buf[32]; 
	uint16_t n; 
	va_list vl; 
	va_start(vl, fmt);
	n = vsnprintf(buf, sizeof(buf)-1, fmt, vl); 
	va_end(vl);
	lcd_puts(buf);
	return n; 
}

/*************************************************************************
Initialize display and select type of cursor 
Input:    dispAttr LCD_DISP_OFF            display off
                   LCD_DISP_ON             display on, cursor off
                   LCD_DISP_ON_CURSOR      display on, cursor on
                   LCD_DISP_CURSOR_BLINK   display on, cursor on flashing
Returns:  none
*************************************************************************/
void lcd_init(uint8_t dispAttr)
{
	#if LCD_PCF8574_INIT == 1
	//init pcf8574
	pcf8574_init();
	#endif
	dataport = 0;
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);
	
	// wait for display to power up
	delay(16000); 
	
	/* initial write to lcd is 8bit */
	dataport |= 0x30; 
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);
	lcd_e_toggle();
	
	delay(4992); // delay, busy flag can't be checked here 
	lcd_e_toggle();
	
	delay(64); 
	// repeat last command third time
	lcd_e_toggle();
	delay(64); 
	
	/* now configure for 4bit mode */
	dataport &= ~0xf0;
	dataport |= 0x20; 
	pcf8574_setoutput(LCD_PCF8574_DEVICEID, dataport);
	lcd_e_toggle();
	delay(64); // some displays need this additional delay
	
	lcd_command(LCD_FUNCTION_DEFAULT); 
	lcd_command(LCD_DISP_OFF); 
	lcd_clrscr();
	lcd_command(LCD_MODE_DEFAULT); 
	//lcd_command(dispAttr); 
	
	lcd_command(0x0c); 
	lcd_command(0x06); 
	lcd_data(0x48); 

}/* lcd_init */
