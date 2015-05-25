// индикатор ионизирующей радиации
//


//#define TEST_BUILD //сборка для тестирования


///////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/delay.h>


#define BIT_IS_SET  bit_is_set
#define BIT_IS_CLR  bit_is_clear

#define SET_BIT(reg, bit)  reg |= (1<<bit)
#define CLR_BIT(reg, bit)  reg &= (~(1<<bit))
#define INV_BIT(reg, bit)  reg ^= (1<<bit)


#define BATT_LEVEL_3 90 //9,0В //уровни индикатора раззаряда батареи
#define BATT_LEVEL_2 84 //8,4В
#define BATT_LEVEL_1 78 //7,8В


#include "display.h"


#define BATTERY_LOW 60 //минимальное напряжение 6.0В
#define VOLT_FACT_1 11 //поправки для напряжения
#define VOLT_FACT_2 65
#define TEMP_FACT 315 //поправка для температуры
#define LIGHT_TIME 10 //время автовыключения подсветки 10 секунд
#define FAIL_TIME 40 //нулевой фон более 40 секунд значит отказ железа
#define HVGEN_FREQ 20 //частота подкачки 100/20=5Гц
#define GEIGER_TIME 75 //время измерения 75 секунд для СИ29БГ
#define GRAPH_FACT 30 //скорость движения гистограммы 1..30 секунд
#define BUZZ_1K 120 //125k/120 ~1kHz
#define BUZZ_2K 59 //125k/59 ~2kHz основная резонансная частоа пищалки
#define BUZZ_4K 14 //125k/29 ~4kHz
#define BUTT_TIME_DEF 80 //время удержания кнопки по умолчанию ~100х10мс=1с
#define BUTT_TIME_DEB 5 //время антидребезга ~5x10ms=50ms

#define TMR1_PRELOAD 49911 //65535-49911=15624  15625/15624  //1Гц

#define DISP_LIGHT_ON    SET_BIT(PORTD,7) //включаем подсветку экрана
#define DISP_LIGHT_OFF   CLR_BIT(PORTD,7) //выключаем подсветку
#define DISP_LIGHT_IS_ON BIT_IS_SET(PIND,7) //подсветка дисплея включена

#define BUZZ_START       SET_BIT(TIMSK2,OCIE2A); //разрешение прерывания пищалки
#define BUZZ_STOP        CLR_BIT(TIMSK2,OCIE2A); //запрет прерывания пищалки
#define BUZZ_IS_STOPPED  BIT_IS_CLR(TIMSK2,OCIE2A) //прерывание пищалки запрещено

#define BUTTON_PRESS     (BIT_IS_CLR(PINB,4) || BIT_IS_CLR(PINB,3)) //нажата любая кнопка
#define BUTTON1_PRESS    (BIT_IS_CLR(PINB,4) && BIT_IS_SET(PINB,3)) //кнопка 1 нажата
#define BUTTON2_PRESS    (BIT_IS_CLR(PINB,3) && BIT_IS_SET(PINB,4)) //кнопка 2 нажата


///////////////////////////////////////////////////////////////////////////////////////////////////

char StrBuff[13]; //буфер для строк на 13 символов

int8_t CurrTemp; //температура

uint32_t DoseRad; //доза
uint32_t BackRad; //текущий фон (мощность дозы)
uint32_t MaxRad; //максимальный фон
uint32_t SumImp; //общее количество импульсов

uint16_t RadImp[GEIGER_TIME]; //счетчик импульсов для расчета фона
uint8_t GraphImp[101]; //счетчик импульсов для гистограммы

uint8_t BattVolt; //измеренное напряжение

uint8_t TimeSec; //секунды //счетчики времени
uint8_t TimeMin; //минуты
uint8_t TimeHrs; //часы

uint8_t FailTmr; //таймер отказа устройства
uint8_t GraphTmr; //таймер для формирования времени гистограммы
uint8_t LightTmr; //таймер подсветки
uint8_t SigTmr; //таймер для формирования звуковых сигналов

uint8_t ButStat; //состояние кнопок
uint8_t But1Time = BUTT_TIME_DEF; //длительность проверки на удержание кнопки
uint8_t But2Time = BUTT_TIME_DEF;

uint8_t SoundVol = 5; //громкость звуковых сигналов
uint8_t AlarmLvl = 50; //порог тревоги

uint8_t FastDiv=1; //делитель для режима быстрых измерений, время - 75/FastDiv секунд


///////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t serviceReg = 0b00000000; //регистр служебных флагов

//bit 0  //флаг - активна сигнализация о превышении фона
#define ALARM_START       SET_BIT(serviceReg,0)
#define ALARM_STOP        CLR_BIT(serviceReg,0)
#define ALARM_IS_STARTED  BIT_IS_SET(serviceReg,0)
#define ALARM_IS_STOPPED  BIT_IS_CLR(serviceReg,0)

//bit 1  //флаг - нужна световая индикация импульса
#define FLASH_SET         SET_BIT(serviceReg,1)
#define FLASH_CLR         CLR_BIT(serviceReg,1)
#define FLASH_IS_SET      BIT_IS_SET(serviceReg,1)

//bit 2  //флаг - идет процесс выключения питания
#define PWROFF_SET        SET_BIT(serviceReg,2)
#define PWROFF_CLR        CLR_BIT(serviceReg,2)
#define PWROFF_IS_CLR     BIT_IS_CLR(serviceReg,2)

//bit 3  //флаг удержания кнопки 1
#define HOLD1_SET         SET_BIT(serviceReg,3)
#define HOLD1_CLR         CLR_BIT(serviceReg,3)
#define HOLD1_IS_CLR      BIT_IS_CLR(serviceReg,3)

//bit 4  //флаг удержания кнопки 2
#define HOLD2_SET         SET_BIT(serviceReg,4)
#define HOLD2_CLR         CLR_BIT(serviceReg,4)
#define HOLD2_IS_CLR      BIT_IS_CLR(serviceReg,4)


///////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t settingsReg = 0b00001111; //регистр флагов настройки

//bit 0  //флаг - включена сигнализация о превышении фона
#define ALARM_DISABLE     CLR_BIT(settingsReg,0)
#define ALARM_TOGGLE      INV_BIT(settingsReg,0)
#define ALARM_IS_ENABLED  BIT_IS_SET(settingsReg,0)

//bit 1  //флаг - включены щелчки динамиком при импульсах
#define CLICK_TOGGLE      INV_BIT(settingsReg,1)
#define CLICK_IS_ENABLED  BIT_IS_SET(settingsReg,1)

//bit 2  //флаг - включены звуковые сигналы кнопок
#define KBEEP_TOGGLE      INV_BIT(settingsReg,2)
#define KBEEP_IS_ENABLED  BIT_IS_SET(settingsReg,2)

//bit 3  //флаг - включена подсветка и световые сигналы
#define LIGHT_TOGGLE      INV_BIT(settingsReg,3)
#define LIGHT_IS_ENABLED  BIT_IS_SET(settingsReg,3)


///////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t ErrReg = 0b00000000; //регистр состояния ошибки


///////////////////////////////////////////////////////////////////////////////////////////////////
const char MenuStr01[] PROGMEM = "Exit/Reset"; // массив строк меню в программной памяти мк
const char MenuStr02[] PROGMEM = "Volume";
const char MenuStr03[] PROGMEM = "Clicks";
const char MenuStr04[] PROGMEM = "Buttons";
const char MenuStr05[] PROGMEM = "Light";
const char MenuStr06[] PROGMEM = "Level";
const char MenuStr07[] PROGMEM = "Alarm";
const char MenuStr08[] PROGMEM = "Fast";

PGM_P const MenuStr[] PROGMEM =
{ MenuStr01, MenuStr02, MenuStr03, MenuStr04, MenuStr05, MenuStr06, MenuStr07, MenuStr08 };


///////////////////////////////////////////////////////////////////////////////////////////////////
const char MsgStr01[] PROGMEM = "WARNING"; // массив строк сообщений
const char MsgStr02[] PROGMEM = "BATTERY LOW";
const char MsgStr03[] PROGMEM = "H/W ERROR";
const char MsgStr04[] PROGMEM = "NO IMPULSES";
const char MsgStr05[] PROGMEM = "Geiger";
const char MsgStr06[] PROGMEM = "Counter";
const char MsgStr07[] PROGMEM = "v05 2015";
const char MsgStr08[] PROGMEM = "shutdown";

PGM_P const MsgStr[] PROGMEM =
{ MsgStr01, MsgStr02, MsgStr03, MsgStr04, MsgStr05, MsgStr06, MsgStr07, MsgStr08 };


///////////////////////////////////////////////////////////////////////////////////////////////////

void system_init(void);
void system_shutdown(void);
void check_battery(void);
void check_sysfail(void);
uint8_t get_button(void);
void short_beep(uint8_t del, uint8_t tmp);
void light_set(void);
void delay_ms(uint8_t val);
void delay_us(uint8_t val);
void hvgen_impuls(void);
void reset_backrad(void);
void system_menu(void);
void main_screen(uint8_t dev);
