// индикатор ионизирующей радиации 05.8
//
// AVR Toolchain 3.4.2
// AVR Eclipse Plugin 2.3.4
//
// 2014-2015
//


#include "main.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
ISR(INT0_vect) //внешнее прерывание //импульс от счетчика
{
if(PWROFF_IS_CLR) //если флаг выключения не поднят
	{
	if(RadImp[0]!=65535) RadImp[0]++; //считаем импульсы
	if(GraphImp[0]!=255) GraphImp[0]++;
	//если общая сумма импульсов больше дозы 999999 мкР то переполнение
	if(++SumImp>999999UL*3600/GEIGER_TIME) SumImp=999999UL*3600/GEIGER_TIME;

	hvgen_impuls(); //подкачка преобразователя

	FailTmr=0; //сброс таймера отказа системы

	if(LIGHT_IS_ENABLED) FLASH_SET; //установка флага для запуска световой индикации импульса

	if(CLICK_IS_ENABLED && ALARM_IS_STOPPED && BUZZ_IS_STOPPED)
		{
		SET_BIT(PORTB,0); //звуковая индикация импульса
		delay_us(SoundVol*5);
		CLR_BIT(PORTB,0);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
ISR(PCINT0_vect) //внешнее прерывание по изменению состояния пинов
{
}


///////////////////////////////////////////////////////////////////////////////////////////////////
ISR(TIMER2_COMPA_vect)  //прерывание по совпадению таймера 2
{
TCNT2 = 0x00;

SET_BIT(PORTB,0); //импульс тока на пищалку
delay_us(SoundVol); //длительность импульса - громкость
CLR_BIT(PORTB,0);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
ISR(TIMER1_OVF_vect) //прерывание по переполнению таймера 1
{
TCNT1 = TMR1_PRELOAD; //предзагрузка таймера для переполнения с частотой 1Гц

SET_BIT(ADCSRA,ADSC); //запускаем АЦП

uint32_t RadBuff=0;
for(uint8_t i=0; i<GEIGER_TIME; i++) RadBuff+=RadImp[i]; //расчет фона мкР/ч

BackRad=RadBuff;
if(RadBuff>999999) RadBuff=999999; //переполнение

if(BackRad>MaxRad) MaxRad=BackRad; //фиксируем максимум фона

if(BackRad > AlarmLvl && ALARM_IS_ENABLED) ALARM_START; //запускаем сигнал тревоги

if(BackRad==0 && FailTmr!=255) FailTmr++; //если нулевой фон инкрементируем таймер отказа

for(uint8_t k=GEIGER_TIME-1; k>0; k--) RadImp[k]=RadImp[k-1]; //перезапись массива
RadImp[0]=0; //сбрасываем счетчик импульсов

DoseRad=(SumImp*GEIGER_TIME/3600); //рассчитаем дозу

if(++GraphTmr>GRAPH_FACT-1) //скорость гистограммы
	{
	GraphTmr=0;
	for(uint8_t k=100; k>0; k--) GraphImp[k]=GraphImp[k-1]; //перезапись массива
	GraphImp[0]=0;
	}

if(TimeHrs<99) //если таймер не переполнен
	{
	if(++TimeSec>59) //считаем секунды
		{
		if(++TimeMin>59) //считаем минуты
			{
			if(++TimeHrs>99) TimeHrs=99; //часы
			TimeMin=0;
			}
		TimeSec=0;
		}
	}

if(LightTmr>0) //таймер подсветки
	{
	LightTmr--; //декремент таймера
	if(LightTmr==0) DISP_LIGHT_OFF; //выключаем подсветку
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
ISR(TIMER0_OVF_vect) //прерывание по переполнению таймера 0
{
TCNT0 = 0x64; //~100Гц

//формируем импульсы для преобразователя напряжения
static uint8_t ConvTmr;

if(++ConvTmr > HVGEN_FREQ) //частота импульсов подкачки 100/HVGEN_FREQ
	{
	ConvTmr=0;
	hvgen_impuls(); //подкачка
	}

//формируем прерывистые звук. сигналы тревоги
if(ALARM_IS_STARTED) //если запущен сигнал о превышении фона
	{
	if(BUZZ_IS_STOPPED && SigTmr==0) BUZZ_START;
	SigTmr++;
	if(SigTmr==40) BUZZ_STOP; //длина сигналов
	if(SigTmr==100) SigTmr=0; //период сигналов
	}

//световая индикация импульса от счетчика
if(FLASH_IS_SET) //если поднят флаг индикации импульса
	{
	SET_BIT(PORTC,3); //включаем светодиод
	static uint8_t FlashTmr; //таймер для формирования вспышек светодиода
	if(++FlashTmr>3) //инкрементируем таймер
		{
		FlashTmr=0; //сброс таймера
		FLASH_CLR; //сброс флага
		CLR_BIT(PORTC,3); //выключаем светодиод
		}
	}

// обработка кнопок
if(BUTTON1_PRESS) //кнопка 1 нажата
	{
	if(++But1Cnt>But1Time) //верхняя граница счетчика - время удержания кнопки
		{
		But1Cnt=0; //сброс счетчика кнопки
		ButStat=2; //установка состояния кнопок, 2-удержание кн.1
		HOLD1_SET; //установка флага удержания кнопки
		}
	}
else //кнопка 1 отжата
	{  //нижняя граница счетчика - пауза антидребезга
	if(But1Cnt > BUTT_TIME_DEB && HOLD1_IS_CLR) ButStat=1; //1-короткое нажатие кн.1
	But1Cnt=0; //сброс счетчика кнопки
	HOLD1_CLR; //сброс флага удержания
	But1Time = BUTT_TIME_DEF; //установка времени удержания кнопки по умолчанию
	}

if(BUTTON2_PRESS) //кнопка 2 нажата
	{
	if(++But2Cnt>But2Time)
		{
		But2Cnt=0;
		ButStat=4; //установим состояние кнопок, 4-удержание кн.2
		HOLD2_SET;
		}
	}
else //кнопка 2 отжата
	{
	if(But2Cnt > BUTT_TIME_DEB && HOLD2_IS_CLR) ButStat=3; //3-короткое нажатие кн.2
	But2Cnt=0;
	HOLD2_CLR;
	But2Time = BUTT_TIME_DEF;
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////
ISR(ADC_vect) //прерывание по окончанию АЦП
{
if(ADMUX==0b11000000) //если регистр АЦП настроен на вход ADC0
	{
	uint16_t BattBuff = (ADC * VOLT_FACT_1 + ADC * VOLT_FACT_2 / 100) / 100; //вычисляем напряжение
	if(BattBuff>99) BattBuff=99;
	BattVolt=BattBuff;
	ADMUX=0b11001000; //переключаем АЦП на встроенный термодатчик ADC8
	}
else //иначе в ADC температура ядра
	{
	int16_t TempBuff = (int)ADC - TEMP_FACT; //вычисляем температуру
	if(TempBuff>99) TempBuff=99;
	else if(TempBuff<-99) TempBuff=-99;
	CurrTemp=TempBuff;
	ADMUX=0b11000000;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void system_init(void) //инициализация системы
{
DDRB=0b00100011; //5 светодиод //4-3 кнопки //1 преобразователь //0 динамик
PORTB=0b00011000; //подтягивающие резисторы для кнопок
DDRC=0b00001110; //3 светодиод //2-CS //1-DATA //0 вход вольтметра
DDRD=0b11111000; //7 подсветка //6 питание LCD //5-RES //4-DC //3-CLK //2 вход импульсов
PORTD=0b01000100;

TCCR0B=0b00000101; //2-0 предделитель
TCNT0=0x64;

TCCR1B=0b00000101; //2-0 предделитель
TCNT1=TMR1_PRELOAD;


TCCR2B=0b00000101; //101 Fcpu/128=125000 //100 Fcpu/64=250000
OCR2A=BUZZ_2K;

ACSR=0b10000000; //выключить компаратор
ADMUX=0b11000000; //внутренний опорный источник //канал 0 АЦП
ADCSRA=0b10001111; //включить АЦП //6 ADSC //разрешение прерываний //предделитель

sei(); //глобальное разрешение прерываний

#ifndef TEST_BUILD
if(PWROFF_IS_CLR) //если включаемся с флагом 0 то сразу уходим спать (первое включение)
	{
	system_sleep();
	return;
	}
#endif

lcd_init(); //инициализация дисплея

lcd_clear(); //очистка экрана

SET_BIT(ADCSRA,ADSC); //запуск АЦП
while(BIT_IS_SET(ADCSRA,ADSC)); //ждем завершения АЦП

check_battery(); //если батарея разряжена выключаемся

strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[4])); //выводим приветствие
lcd_string(2,2,StrBuff);
strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[5]));
lcd_string(4,5,StrBuff);
strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[6]));
lcd_string(7,3,StrBuff);

#ifndef TEST_BUILD
for(uint16_t k=1200; k>0; k--) //накачка преобразователя
	{
	hvgen_impuls();
	_delay_ms(1);
	}
#endif

short_beep(50,BUZZ_1K);

while(BUTTON_PRESS); //ждем если нажаты кнопки

FailTmr=0; //сброс таймера отказа
ButStat=0; //сброс состояния кнопок
reset_backrad(); //сброс фона при включении прибора

TIMSK0=0b00000001; //разрешаем прерывания для таймеров
TIMSK1=0b00000001;
EICRA=0b00000010; //настраиваем внешнее прерывание 0 по спаду
EIMSK=0b00000001; //разрешаем внешнее прерывание

lcd_clear();
light_set(); //подсветка
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void system_sleep(void) //выключение
{
PWROFF_SET; //поднимаем флаг выключения

EIMSK=0x00;
TIMSK0=0x00;
TIMSK1=0x00;
TIMSK2=0x00;
ADCSRA=0x00;

DDRB=0x00;
PORTB=0b00011000;
DDRC=0x00;
PORTC=0x00;
DDRD=0x00;
PORTD=0x00;

PCMSK0=0b00011000; //изменение состояния пинов 3 и 4 будет вызывать PCINT0
PCICR=0b00000001; //разрешаем прерывание PCINT0

SMCR=0b00000101; //выбираем режим сна и разрешаем спать
sleep_cpu();
//сон
cli(); //после пробуждения запретим все прерывания
SMCR=0x00; //запрещаем спать
PCICR=0x00; //запрещаем прерывания по изменению состояния пинов

_delay_ms(1000); //ждем секунду

if(BUTTON_PRESS) { system_init(); PWROFF_CLR; } //включаемся если нажата любая кнопка
else { sei(); system_sleep(); } //иначе возвращаемся спать
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void check_battery(void) //проверить батарею, если разряжена предупреждение и выкл. прибор
{
if(BattVolt < BATTERY_LOW) //если напряжение ниже рабочего порога
	{
	lcd_clear();

	draw_battery(1,48,0); //нарисуем значок разряженной батарейки

	strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[0])); //выводим сообщение о разряде
	lcd_string(4,3,StrBuff);
	strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[1]));
	lcd_string(6,1,StrBuff);

	SET_BIT(ErrReg,0); //ошибка 1

	_delay_ms(4000); //пауза перед выключением
	system_sleep(); //уходим спать
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void check_sysfail(void) //проверить отказ железа, если не работает выводим сообщение и выкл.
{
if(FailTmr > FAIL_TIME) //если таймер отказа переполнен
	{
	lcd_clear();

	strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[2])); //выводим сообщение об ошибке
	lcd_string(3,2,StrBuff);
	strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[3]));
	lcd_string(5,1,StrBuff);

	SET_BIT(ErrReg,1); //ошибка 2

	_delay_ms(4000); //пауза перед выключением
	system_sleep(); //уходим спать
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t get_button(void) //опрос кнопок
{
uint8_t Button = ButStat;

if(Button) //если была нажата кнопка
	{
	ButStat=0; //сброс состояния кнопок

	if(ALARM_IS_STOPPED) short_beep(20,BUZZ_2K); //если не включена тревога пищим кнопками

	if(LIGHT_IS_ENABLED) //если разрешен свет
		{
		if(LightTmr==0) Button=0; //если таймер подсветки вышел, сбрасываем состояние кнопок
		light_set(); //включаем подсветку экрана
		}

	if(Button && ALARM_IS_STARTED) //если активна тревога и есть нажатие кнопки
		{
		BUZZ_STOP; //запрет прерывания пищалки
		ALARM_STOP; //сбросить флаг активности сигнализации
		ALARM_DISABLE; //отключаем сигнализацию в настройках
		SigTmr=0; //сброс таймера пищалки
		Button=0; //сброс состояния кнопок
		}
	}
return (Button); //вернем состояние кнопок
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void short_beep(uint8_t del, uint8_t tmp)  //длительность 0..255мс  //BUZZ_1K, BUZZ_2K, BUZZ_4K
{
if(KBEEP_IS_ENABLED) //если разрешены сигналы кнопок
	{
	OCR2A = tmp; //установим частоту пищалки
	BUZZ_START; //запуск пищалки
	delay_ms(del);
	BUZZ_STOP; //стоп
	OCR2A = BUZZ_2K; //установим частоту по умолчанию
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void light_set(void) //включение/выключение подсветки экрана
{
if(LIGHT_IS_ENABLED) //если свет разрешен
	{
	DISP_LIGHT_ON; //включаем подсветку
	LightTmr = LIGHT_TIME; //запускаем таймер
	}
else //иначе
	{
	DISP_LIGHT_OFF; //выключаем подсветку
	LightTmr = 0; //сбрасываем таймер
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void delay_ms(uint8_t val) //0..255
{
while(val-->0) _delay_ms(1);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void delay_us(uint8_t val) //0..255
{
while(val-->0) _delay_us(1);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void hvgen_impuls(void) //импульс на преобразователь
{
SET_BIT(PORTB,1);
_delay_us(10);
CLR_BIT(PORTB,1);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void reset_backrad(void)
{
for(uint8_t i=0; i<GEIGER_TIME; i++) RadImp[i]=0;
BackRad=0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void system_menu(void) //меню настроек
{
uint8_t currStr = 0; //номер выбранной строки 0..7

lcd_clear();

while(1)
	{
	check_battery();
	check_sysfail();

	draw_cursor(currStr,8); //рисуем курсор-указатель для восьми строк

	for(uint8_t i=0; i<8; i++)
		{
		strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MenuStr[i])); //загружаем из памяти строку меню
		lcd_string(i+1,2,StrBuff); //выводим на экран
		}

	lcd_char('>',1,12); //exit
	sprintf(StrBuff,"%2u",SoundVol); //volume
	lcd_string(2,11,StrBuff);
	draw_marker(3,CLICK_IS_ENABLED); //clicks
	draw_marker(4,KBEEP_IS_ENABLED); //buttons
	draw_marker(5,LIGHT_IS_ENABLED); //light
	sprintf(StrBuff,"%3u",AlarmLvl); //level
	lcd_string(6,10,StrBuff);
	draw_marker(7,ALARM_IS_ENABLED); //alarm
	lcd_char('>',8,12); //reset

	delay_ms(10);

	switch(get_button()) //проверяем кнопки
		{
		case 4: But2Time=25; //уменьшаем удержание кнопки для быстрой прокрутки меню
		case 3: if(++currStr>7) currStr=0; break; //выбираем следующий пункт меню
		case 2: if(currStr==1 || currStr==5) But1Time=25; //уменьшаем удержание копки для пунктов
		case 1: //настраиваем выбранный пункт меню
			switch(currStr)
				{
				case 0: return; //exit
				case 1: if(++SoundVol>20) SoundVol=1; break; //vol
				case 2: CLICK_TOGGLE; break; //clicks
				case 3: KBEEP_TOGGLE; break; //buttons
				case 4: LIGHT_TOGGLE; light_set(); break; //backlight
				case 5: if((AlarmLvl=AlarmLvl+10)>200) AlarmLvl=10; break; //level
				case 6: ALARM_TOGGLE; break; //alarm
				case 7: reset_menu(); return; //reset
				}
		break;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void reset_menu(void) //подменю сброса
{
uint8_t currStr = 0;

lcd_clear();

while(1)
	{
	check_battery();
	check_sysfail();

	draw_cursor(currStr,4);

	for(uint8_t i=0; i<4; i++)
		{
		strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MenuStr[i+8])); //загружаем из памяти строки
		lcd_string(i+1,2,StrBuff);
		lcd_char('>',i+1,12); //рисуем символ ">" в конце каждой строки
		}

	delay_ms(10);

	switch(get_button()) //проверяем кнопки
		{
		case 4: But2Time=25; //удерживается кн.2 - уменьшаем время проверки на удержание
		case 3: if(++currStr>3) currStr=0; break;  //короткое нажатие кн.2 - выбор пункта
		case 2: //удержание кн.1
		case 1: //короткое нажатие кн.1 - сбросить выбранное и выйти из меню
			switch(currStr)
				{
				case 0: //reset errors
					ErrReg = 0;
					break;
				case 1: //reset dose
					DoseRad=0;
					SumImp=0;
					break;
				case 2: //reset maximum
					MaxRad=0;
					break;
				case 3: //reset all
					reset_backrad();
					for(uint8_t i=0; i<101; i++) GraphImp[i]=0;
					MaxRad=0;
					DoseRad=0;
					SumImp=0;
					TimeSec=0;
					TimeMin=0;
					TimeHrs=0;
					GraphTmr=0;
					ErrReg=0;
					break;
				}
			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
void main_screen(uint8_t dev) //рисуем главный экран
{
check_battery();
check_sysfail();

draw_battery(0,84,BattVolt); //индикатор батареи

sprintf(StrBuff,"%02u:%02u:%02u",TimeHrs,TimeMin,TimeSec); //время работы
lcd_string(1,5,StrBuff);

sprintf(StrBuff,"%u.%uV%6luµR",BattVolt/10,BattVolt%10,DoseRad); //напряжение и доза
lcd_string(2,1,StrBuff);

lcd_char(144,3,1);
sprintf(StrBuff,"%6lu Max.",MaxRad); //максимальный зарегистрированный фон
lcd_string(3,2,StrBuff);

if(ALARM_IS_STARTED) lcd_char(147,4,1); //если тревога рисуем значок "два восклицательных знака"
else lcd_char(144,4,1); //иначе значок "треугольник-указатель влево"
sprintf(StrBuff,"%6lu µR/h",BackRad); //фон
lcd_string(4,2,StrBuff);

graph_print(5, GraphImp, dev); //рисуем график-гистограмму

//нижняя строка главного экрана
if(ALARM_IS_ENABLED) //если тревога включена
	{
	lcd_char(150,8,1);
	sprintf(StrBuff,"%-3u",AlarmLvl); //установленный порог тревоги
	lcd_string(8,2,StrBuff);
	}
else lcd_string(8,1,"    ");

if(ErrReg) sprintf(StrBuff," %02Xh",ErrReg); //если есть ошибки, выводим
else sprintf(StrBuff,"%3d°",CurrTemp); //иначе выводим температуру воздуха
lcd_string(8,5,StrBuff);

if(KBEEP_IS_ENABLED) lcd_char(148,8,10);
else lcd_char(0,8,10);

if(LIGHT_IS_ENABLED) //значок подсветки
	{
	if(BIT_IS_SET(PIND,7)) lcd_char(129,8,12);
	else lcd_char(130,8,12);
	}
else lcd_char(0,8,12);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
int main(void) //самая главная
{
uint8_t graphDev = 1;
uint8_t refrTmr = 0;

system_init();

while(1)
	{
	if(refrTmr==0) { refrTmr=20; main_screen(graphDev); } //обновление экрана каждые 20*10мс
	refrTmr--;

	delay_ms(10); //опрос кнопок каждые 10мс

	switch(get_button()) //действие по нажатой кнопке
		{
		case 1: //короткое нажатие кнопки 1
			if((graphDev/=2)==0) graphDev=1; //уменьшить масштаб гистограммы
			refrTmr=0;
			break;

		case 2: //удерживается кнопка 1
			system_menu(); //уходим в меню
			lcd_clear();
			refrTmr=0;
			break;

		case 3: //короткое нажатие кнопки 2
			if((graphDev*=2)>16) graphDev=16; //увеличить масштаб гистограммы
			refrTmr=0;
			break;

		case 4: //удерживается кнопка 2
			lcd_clear();
			strcpy_P(StrBuff,(PGM_P)pgm_read_word(&MsgStr[7]));
			lcd_string(4,3,StrBuff);
			draw_char_line(2,161);
			draw_char_line(6,161);
			_delay_ms(2000);
			short_beep(50,BUZZ_1K);
			system_sleep(); //выключение прибора
			refrTmr=0;
			break;
		}
	}
}
