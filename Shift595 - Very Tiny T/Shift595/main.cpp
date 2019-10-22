#include <avr/io.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>

#define HC595_PORT   PORTB
#define HC595_DDR    DDRB

#define BTN_PIN PB0
#define HC595_DS_POS PB1      //Data pin (DS) pin location
#define HC595_SH_CP_POS PB4      //Shift Clock (SH_CP) pin location
#define HC595_ST_CP_POS PB3      //Store Clock (ST_CP) pin location
#define HC595_OE PB2			//Output Enable

uint8_t mode = 0;
uint8_t j = 0;

uint64_t line_pattern1[] = {0x7800000, 0x18600000, 0x20100000, 0x40080000, 0x80040000,  0X100020000, 0X2200011801, 0X1C0000E402, 0X0000000204, 0X00000001F8 };
uint64_t line_pattern2[] = {0x7800000, 0x1FE00000, 0x3FF00000, 0x7FF80000, 0xFFFC0000,  0x1FFFE0000, 0x23FFFF1801, 0x3FFFFFFC03, 0x3FFFFFFE07, 0x3FFFFFFFFF };		
	
void HC595Init()
{
   //Make the Data(DS), Shift clock (SH_CP), Store Clock (ST_CP) lines output
   HC595_DDR |= ((1<<HC595_SH_CP_POS)|(1<<HC595_ST_CP_POS)|(1<<HC595_DS_POS)|(1<<HC595_OE));
   HC595_PORT |= (1<<HC595_PORT);

}

#define HC595DataHigh() (HC595_PORT|=(1<<HC595_DS_POS))
#define HC595DataLow() (HC595_PORT&=(~(1<<HC595_DS_POS)))

void HC595Pulse()
{
   //Pulse the Shift Clock
   HC595_PORT|=(1<<HC595_SH_CP_POS);//HIGH

   HC595_PORT&=(~(1<<HC595_SH_CP_POS));//LOW
}


void HC595Latch()
{
   HC595_PORT|=(1<<HC595_ST_CP_POS);//HIGH
   _delay_us(1);

   HC595_PORT&=(~(1<<HC595_ST_CP_POS));//LOW
   _delay_us(1);
}


void HC595Write(uint8_t data)
{
   for(uint8_t i=0;i<8;i++)
   {

      if(data & 0b10000000)
      {
         HC595DataHigh();
      }
      else
      {
         HC595DataLow();
      }
      HC595Pulse();  //Pulse the Clock line
      data=data<<1;  //Now bring next bit at MSB position
   }
}


void Wait()
{
   _delay_ms(150);
}

void Wait2()
{
	_delay_us(10);
}



void write5Bytes(uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4)
{
	
	//uint8_t temp = byte2;
	//byte2 = (byte2 & 0x0F) | ((byte2>>1) & (0x70)) | ((byte2<<3) & (0x80));
	HC595Write(byte4);
	HC595Write(byte3);
	HC595Write(byte2);
	HC595Write(byte1);
	HC595Write(byte0);
	HC595Latch();
}

void EEPROM_write(uint8_t uiAddress, uint8_t ucData) //Stolen from https://stackoverflow.com/questions/4412111/avr-eeprom-read-write
{
	while(EECR & (1<<EEPE));    /* Wait for completion of previous write */
	EEARH = 0x00;
	EEARL = uiAddress;
	EEDR = ucData;
	cli();
	EECR |= (1<<EEMPE);     /* Write logical one to EEMPE */
	EECR |= (1<<EEPE);      /* Start eeprom write by setting EEPE */
	sei();
}
unsigned char EEPROM_read(uint8_t uiAddress) //Stolen from https://stackoverflow.com/questions/4412111/avr-eeprom-read-write
{
	while(EECR & (1<<EEPE)); /* Wait for completion of previous write */

	EEARH = (uiAddress>>8);  /* Set up address register */
	EEARL = uiAddress;

	EECR |= (1<<EERE);       /* Start eeprom read by writing EERE */

	return EEDR;             /* Return data from Data Register */
}


ISR(PCINT0_vect)
{
	_delay_ms(1);
	if (((PINB&(1<<BTN_PIN)) == 0x00))
	{
		sleep_disable();
		mode++;
		j = 0xFD; //Exit current mode loop
		if (mode >= 9){ //Max mode options - 1
			mode = 0;
		}
	}
}


int main()
{
	

	HC595Init();
	
	PORTB |= (1<<BTN_PIN); //Btn pull up enable
	mode = EEPROM_read(0x07); // Read mode selection from last power cycle
	if(mode > 0x09){ //catch any errors in EEPROM
		mode = 0x00;
	}
	
	//PRR = (1<<PRTIM0) | (1<<PRTIM1) | (1<<PRUSI) | (1<<PRADC); //power reduction
	GIMSK |= (1<<PCIE);
	PCMSK |= (1<<BTN_PIN);
	sei();
		  
	while(1)
	{	
		if (mode == 0)
		{
			for (j=0;j<10;j++)
			{
				write5Bytes(line_pattern1[j],line_pattern1[j]>>8,line_pattern1[j]>>16,line_pattern1[j]>>24,line_pattern1[j]>>32);
				//data_byte = data_byte<<1;
				Wait();   	
				Wait();	 
				Wait();				
			}
		}
		if (mode == 1)
		{
			for (j=0;j<10;j++)
			{
				write5Bytes(line_pattern2[j],line_pattern2[j]>>8,line_pattern2[j]>>16,line_pattern2[j]>>24,line_pattern2[j]>>32);
				//data_byte = data_byte<<1;
				Wait();
				Wait();
			}
			
			for (j=10;j>0;j--)
			{
				if (mode == 1) //Check for exit condition
				{
					write5Bytes(line_pattern2[j-1],line_pattern2[j-1]>>8,line_pattern2[j-1]>>16,line_pattern2[j-1]>>24,line_pattern2[j-1]>>32);
					//data_byte = data_byte<<1;
					Wait();
					Wait();
				}
			}
					
		}
		if (mode == 2)
		{
			uint64_t shiftedNum =0x01;
			for (j=0;j<38;j++)
			{
				write5Bytes(shiftedNum,shiftedNum>>8,shiftedNum>>16,shiftedNum>>24,shiftedNum>>32);
				shiftedNum = shiftedNum<<1;				   
				Wait();										
			}
		}
		if (mode == 3)
		{
			uint64_t shiftedNum =0x00;
			for (j=0;j<39;j++)
			{
				write5Bytes(shiftedNum,shiftedNum>>8,shiftedNum>>16,shiftedNum>>24,shiftedNum>>32);
				shiftedNum = shiftedNum<<1;
				shiftedNum = shiftedNum + 1;				   
				Wait();										
			}
			if (mode == 3)//check for exit condition
			{
				for (j=0;j<39;j++)
				{
					write5Bytes(shiftedNum,shiftedNum>>8,shiftedNum>>16,shiftedNum>>24,shiftedNum>>32);
					shiftedNum = shiftedNum<<1;
					Wait();
				}
			}			
		}
		if (mode == 4)
		{
			uint64_t shiftedNum =0x0F;
			while (mode == 4)
			{
				write5Bytes(shiftedNum,shiftedNum>>8,shiftedNum>>16,shiftedNum>>24,shiftedNum>>32);
				if ((shiftedNum & 0x002000000000) == 0x002000000000) //Bit at end
				{
					shiftedNum = shiftedNum<<1;
					shiftedNum = shiftedNum + 1;
				} 
				else
				{
					shiftedNum = shiftedNum<<1;
				}
				
				Wait();
			}
		}
		if (mode == 5)
		{
			uint64_t shiftedNum =0x00;
			for (shiftedNum=0;mode == 5;shiftedNum++)
			{
				write5Bytes(shiftedNum,shiftedNum>>8,shiftedNum>>16,shiftedNum>>24,shiftedNum>>32);
				Wait2();
			}
		}
		if (mode == 6)
		{
			write5Bytes(0xFF,0xFF,0xFF,0xFF,0xFF);
			_delay_ms(1000);
		}
		if (mode == 7)
		{
			mode = 0; //skip this mode
			write5Bytes(0,0,0,0,0);
			sleep_enable();
			sleep_cpu();
						
		}
		if (mode == 8)
		{
			//This mode is also skipped
			write5Bytes(0xFF,0xFF,0xFF,0xFF,0xFF);
			_delay_ms(1000);
		}
			 	 
	}
}
