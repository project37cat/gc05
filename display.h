// функции для работы с дисплеем
//


#include "font.h"


#define DAT_H  PORTC|=(1<<PIN1)
#define DAT_L  PORTC&=~(1<<PIN1)

#define SCK_H  PORTD|=(1<<PIN3)
#define SCK_L  PORTD&=~(1<<PIN3)

#define DC_H   PORTD|=(1<<PIN4)
#define DC_L   PORTD&=~(1<<PIN4)

#define RST_H  PORTD|=(1<<PIN5)
#define RST_L  PORTD&=~(1<<PIN5)

#define CS_H   PORTC|=(1<<PIN2)
#define CS_L   PORTC&=~(1<<PIN2)


uint8_t GraphBuff[2][101];


void lcd_write(uint8_t mode, uint8_t data);
void lcd_data(uint8_t data);
void lcd_command(uint8_t command);
void lcd_init(void);
void lcd_goto(uint8_t line, uint8_t row);
void lcd_clear(void);
void lcd_char(uint8_t sign, uint8_t vPos, uint8_t hPos);
void lcd_string(uint8_t vPos, uint8_t hPos, char *str);

void graph_pixel(uint8_t hPos, uint8_t vPos);
void graph_row(uint8_t hPos, uint8_t val);
void graph_clear(void);
void graph_print(uint8_t vPos, uint8_t *buff, uint8_t dev);
void graph_write(uint8_t vPos);

void draw_char_line(uint8_t vPos, uint8_t Char);
void draw_marker(uint8_t vPos, uint8_t Set);
void draw_cursor(uint8_t vPos, uint8_t numStr);


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_write(uint8_t mode, uint8_t data)
{
uint8_t s=0x80;

DAT_L;
SCK_L;
if(mode) DC_L; //command
else DC_H; //data
CS_L;

if(mode) _delay_us(10);

for(uint8_t i=0; i<8; i++)
	{
	if(data & s) DAT_H;
	else DAT_L;
	s = s>>1;
	SCK_H;
	SCK_L;
	}
CS_H;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_command(uint8_t command)
{
lcd_write(1,command);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_data(uint8_t data)
{
lcd_write(0,data);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_init(void)
{
_delay_us(10); //сброс
RST_H;
_delay_us(10);
RST_L;
_delay_us(10);
RST_H;
CS_H;

lcd_command(0x21); // Function set: extended instruction set
lcd_command(0x14); // Bias System
lcd_command(0x0A); // HV-gen stages
lcd_command(0x05); // Temperature Control
lcd_command(0xCC); // Contrast
lcd_command(0x20); // Function set: standard instruction set
lcd_command(0x11); // VLCD programming range: high
lcd_command(0x0C); // Display control: normal (inverted = 0x0D)
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_goto(uint8_t line, uint8_t row)  //линия (строка) 0..7 //ряд (пикселей) 0..100
{
lcd_command(0x28);
lcd_command(0x40+line);
lcd_command(0x80+row);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_clear(void)
{
lcd_goto(0,0);
for(uint16_t i=0; i<((101*64)/8+8); i++) lcd_data(0x00);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_char(uint8_t sign, uint8_t vPos, uint8_t hPos) //рисуем символ //позиция - 1..8, 1..12
{
if((vPos>0 && vPos<9) && (hPos>0 && hPos<13))
	{
	lcd_goto(vPos-1, 100-hPos*8);
	if(sign<0x20) sign=0x20;
	uint16_t pos = 8*(sign-0x20);
	for(uint8_t y=0; y<8; y++) lcd_data(pgm_read_byte(&font8x8[pos++]));
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void lcd_string(uint8_t vPos, uint8_t hPos, char *str) //выводим строку //макс длина 12 символов
{
for(uint8_t c=0; ((c<12) && (*(str+c)!='\0')); c++) lcd_char(*(str+c), vPos, hPos+c);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void graph_pixel(uint8_t hPos, uint8_t vPos) //101..0, 0..15
{
if(vPos<=7)  SET_BIT(GraphBuff[0][hPos], vPos);
else
	{
	if(vPos>15) vPos=15;
	SET_BIT(GraphBuff[1][hPos], (vPos-8));
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void graph_row(uint8_t hPos, uint8_t val) //101..0, 1..16
{
while(val--) graph_pixel(hPos, val);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void graph_clear(void)
{
for(uint8_t i=0; i<2; i++)
	{
	for(uint8_t k=0; k<101; k++)
		{
		GraphBuff[i][k]=0;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void graph_print(uint8_t vPos, uint8_t *buff, uint8_t dev)
{
lcd_goto(vPos+1,0);
for(uint8_t i=0; i<101; i++)
	{
	switch(i)
		{
		default: lcd_data(0b11000000); break;
		case 19:
		case 20:
		case 39:
		case 40:
		case 59:
		case 60:
		case 79:
		case 80: lcd_data(0b10110000); break;
		}
	}
graph_clear();
for(uint8_t k=0; k<101; k++) graph_row(k,(*(buff+k))/dev);
//for(uint8_t k=0; k<101; k++)  graph_row(k,(*(buff+k/2))/dev);
graph_write(vPos);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void graph_write(uint8_t vPos)
{
lcd_goto(vPos,0);
for(uint8_t k=0; k<101; k++) lcd_data(GraphBuff[0][k]);
lcd_goto(vPos-1,0);
for(uint8_t k=0; k<101; k++) lcd_data(GraphBuff[1][k]);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void draw_char_line(uint8_t vPos, uint8_t Char) //линия из символов
{
for(uint8_t i=0; i<12; i++) lcd_char(Char,vPos,i+1);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void draw_marker(uint8_t vPos, uint8_t Set) //верт. позиция 1..8 //состояние 1..0
{
if(Set) lcd_char(165,vPos,12);
else lcd_char(164,vPos,12);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void draw_cursor(uint8_t vPos, uint8_t numStr) //текущая позиция 0..7 //кол-тво строк в меню 1..8
{
if(vPos>0) lcd_char(0,vPos,1); //затираем предыдущее положение курсора
else lcd_char(0,numStr,1);
if(vPos<numStr) lcd_char(154,vPos+1,1); //рисуем курсор-указатель меню
}
