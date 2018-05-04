/*
*
* Keck-Tronix BangDucer (Copywrite David Keck 3-15-2018)
*
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MCP4725.h>


#define KEY_PAD_PIN			A0
#define LM35_PIN			A1
#define LOOP_DAC_VOLTS_PIN	A2
#define LOOP_DAC_I2C_ADDRESS	0x63
#define THERM_DAC_I2C_ADDRESS   0x62
#define SERIAL_EEPROM_ADDRESS   0x50
	
#define OPAMP_GAIN_TEMPTR	50		// OP AMP gain is negative fifty, -50
#define OPAMP_GAIN_CURRENT	1.638	// Based on 4096 this would be 1.639344
#define VCC					4.94
#define PRECISION_REFERENCE	5.00	// AD586 High Precision 5V reference
#define ADC_REFERENCE		PRECISION_REFERENCE
#define DAC_REFERENCE		PRECISION_REFERENCE 
#define DAC_VOLTS_PER_COUNT	(DAC_REFERENCE / 4095.0)
#define CURRENT_4mA		((1/OPAMP_GAIN_CURRENT) / DAC_VOLTS_PER_COUNT)	// 25%
#define CURRENT_1mA		(CURRENT_4mA / 4)
#define CURRENT_100uA	(CURRENT_1mA / 10)
#define CURRENT_10uA	(CURRENT_1mA / 100)
#define CURRENT_125PCT	(CURRENT_4mA * 6)
#define CURRENT_100PCT	(CURRENT_4mA * 5)
#define CURRENT_25PCT	(CURRENT_4mA)
#define CURRENT_0PCT	(CURRENT_4mA)

// Analog keypad setup
#define NUM_KEYS		16
#define KEY_HOLD_TIME	1000
#define KEY_REPEAT_RATE	275
#define KEY_DEBOUNCE_TIME 200

// Set I2C addr and Arduino Nano pins for I2C LCD
#define LCD_I2C_ADDR       0x3F
#define BACKLIGHT_PIN      3
#define En_pin             2
#define Rw_pin             1
#define Rs_pin             0
#define D4_pin             4
#define D5_pin             5
#define D6_pin             6
#define D7_pin             7

#define DEGREE_SYMBOL 223

#define EEPROM_ADR_LOW_BLOCK 0x50 //0b.101.0000 0x50
#define EEPROM_ADR_HIGH_BLOCK 0x54 //0b.101.0100 0x54

enum modes {NO_MODE, TEMPERATURE, MILLI_AMPS };
enum {HELD, PRESSED, RELEASED};
// Global variables
modes Mode;
modes PreviousMode;
uint16_t KeyHoldTime;
uint16_t KeyRepeatRate;
uint16_t DebounceTime;
uint8_t  KeyState = RELEASED;

char keys[] = {
	'1','2','3','A',
	'4','5','6','B',
	'7','8','9','C',
	'*','0','#','D'
};
uint16_t rawKeys[16]
{
	1000, 906, 830, 789,
	673, 630, 592, 558,
	507, 482, 459, 438,
	406, 389, 374, 361
/*
	1023, 932, 855, 789,	// Original values
	695, 652, 613, 578,
	529, 503, 479, 458,
	424, 408, 392, 378
*/
	};
//	Prototypes
char getKey(); 
void currentMode(void);
void temperatureMode(void);

// Initialise the LCD
LiquidCrystal_I2C	lcd(LCD_I2C_ADDR, En_pin, Rw_pin, Rs_pin, D4_pin, D5_pin, D6_pin, D7_pin);
//LiquidCrystal_I2C	lcd(LCD_I2C_ADDR, 16, 2);

// Initialize the DAC
Adafruit_MCP4725 loopDac;
Adafruit_MCP4725 thermoDac;

void setup()
{
	loopDac.begin(LOOP_DAC_I2C_ADDRESS);	// The I2C Address of the current loop DAC
	thermoDac.begin(THERM_DAC_I2C_ADDRESS);	// The I2C Address of the temperature DAC
	
	// Setup the AREF pin
//	analogReference(INTERNAL);		// Using VCC for reference
	analogReference(EXTERNAL);		// Using external precision reference
	analogRead(LOOP_DAC_VOLTS_PIN);	// Initialize the AREF pin

	Mode = MILLI_AMPS;
	PreviousMode = MILLI_AMPS;

	// set up serial
	Serial.begin(9600);

	//Switch on LCD backlight
	lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
	lcd.setBacklight(HIGH);
	delay(500);
	lcd.setBacklight(LOW);

	// set up the LCD's number of columns and rows: 
	lcd.begin(16, 2);

	// Print logo to the LCD.
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("  KeckTronix");
	lcd.setCursor(0, 1);
	lcd.print("  Bang-Ducer");
	// Let display for a moment
	delay(500);

	setHoldTime(KEY_HOLD_TIME);		// Time to wait until key repeats
	setDebounceTime(KEY_DEBOUNCE_TIME);
}

void loop(void) 
{
	if (Mode == MILLI_AMPS) 
	{
		// Setup initial LCD screen
		lcd.clear();
		lcd.setCursor(6, 0);
		lcd.print("mA");
		lcd.setCursor(10, 0);
		lcd.print("Ex");
		lcd.setCursor(6, 1);
		lcd.print("mA");
		lcd.setCursor(10, 1);
		lcd.print("In ");
		lcd.noCursor();
		PreviousMode = MILLI_AMPS;
		currentMode();
	}
	else if (Mode == TEMPERATURE)
	{
		// Setup initial LCD screen
		lcd.clear();
		lcd.noCursor();
		PreviousMode = TEMPERATURE;
		temperatureMode();
	}
}

void currentMode(void)
{
	const char incrementText[3][4] = { ".01", ".10", "1.0" };
	const byte numIncrements = 2;	// 0-2, total 3
	const float incrementVal[] = { CURRENT_10uA, CURRENT_100uA, CURRENT_1mA };
	static int incrementIdx = 1;
	static int rawAdcValue = 0;
	static float dacVoltage = 0;
	static int rawKeyValue = 0;
	static float keyVoltage = 0;
	static float loopCurrent = 0;
	static float commandedCurrent = 0;
	static float loopDacValue = CURRENT_0PCT;
	static float dacExpectedVolts;
	static char holdKey;
	char key = 0;

	while (Mode == MILLI_AMPS)
	{
		/*
			if (KeyState == HELD)
			{
				Serial.println(" HELD ");
				delay(KEY_REPEAT_RATE);
			}
		*/
		key = getKey();

		if (key)  // Check for a valid key.
		{
			switch (key)
			{
			case NULL:
				break;
			case '3':
				Mode = TEMPERATURE;
				break;
			case '2':	// Increase output by increment Value
				loopDacValue += incrementVal[incrementIdx];
				if (loopDacValue >= CURRENT_125PCT)
					loopDacValue = CURRENT_125PCT;
				break;
			case '8':	// Decrease output by increment Value
				if (loopDacValue <= incrementVal[incrementIdx])
					loopDacValue = 0;
				else
					loopDacValue -= incrementVal[incrementIdx];
				break;
			case '4':	// Set increment value to next higher value
				if (incrementIdx != numIncrements)
					incrementIdx++;
				lcd.setCursor(6, 0);
				lcd.print(incrementText[incrementIdx]);
				break;
			case '6':	// Set increment value to next lower value
				if (incrementIdx != 0)
					incrementIdx--;
				lcd.setCursor(6, 0);
				lcd.print(incrementText[incrementIdx]);
				break;
			case 'A':	// Set current output to 100%, 20mA
				loopDacValue = CURRENT_100PCT;
				break;
			case 'B':	// Increase current output up 25%, max 125%, 24mA
				loopDacValue += CURRENT_25PCT;
				if (loopDacValue >= CURRENT_125PCT)
					loopDacValue = CURRENT_125PCT;
				break;
			case 'C':	// Current output down 25%, max down -25%, 0mA
				if (loopDacValue <= CURRENT_0PCT)
					loopDacValue = 0;
				else
					loopDacValue -= CURRENT_25PCT;
				break;
			case 'D':	// Set current output to 0%, 4mA
				loopDacValue = CURRENT_0PCT;
				break;
			default:
				break;
			}
		}
		// Output to the DAC
		///////////////////////////////////////////
//		loopDacValue = 4095;	// DEBUG DEBUG DEBUG //
		///////////////////////////////////////////
		
		dacExpectedVolts = loopDacValue * DAC_VOLTS_PER_COUNT;
		loopDac.setVoltage(round(loopDacValue), false);
		
		/*
		Serial.print("\tExpected Volts: ");
		Serial.println(dacExpectedVolts, 3);
		
		Serial.print("\tDAC Value: ");
		Serial.println(loopDacValue, 2);
		*/
		rawAdcValue = analogRead(LOOP_DAC_VOLTS_PIN);
		dacVoltage = (rawAdcValue * ADC_REFERENCE) / 1024.0;

		//	Serial.print("\tADC Value: ");
		//	Serial.println(rawAdcValue);

		//	Serial.print("\tArduino Volts: ");
		//	Serial.println(dacVoltage, 3);

		// display on LCD
		lcd.setCursor(12, 0);
		lcd.print(dacExpectedVolts, 2);

		// display on LCD
		lcd.setCursor(12, 1);
		lcd.print(dacVoltage, 2);

		// Display the commanded current on LCD
		commandedCurrent = (dacExpectedVolts / (.250 / OPAMP_GAIN_CURRENT));	// 250ohm resistor, ".250" for display of milli
		lcd.setCursor(0, 0);
		lcd.print(" ");	// Clear decimal artifacts
		if (commandedCurrent >= 10)	// A leading digit (10's) artifact occurs when display switches between 9 and 10
			lcd.setCursor(0, 0);
		else
			lcd.setCursor(1, 0);
		lcd.print(commandedCurrent, 2);

		// Display loop current on LCD
		loopCurrent = (dacVoltage / (.250 / OPAMP_GAIN_CURRENT));	// 250ohm resistor, ".250" for display of milli
		lcd.setCursor(0, 1);
		lcd.print(" ");	// Clear decimal artifacts
		if (loopCurrent >= 10)	// A leading digit (10's) artifact occurs when display switches between 9 and 10
			lcd.setCursor(0, 1);
		else
			lcd.setCursor(1, 1);
		lcd.print(loopCurrent, 2);
	}
	return;
}

void temperatureMode(void)
{
	char incrementText[3][4] = { "1", "10", "25" };	// Degrees "C"
	const byte numIncrements = 2;	// 0-2, total 3
	float incrementVal[] = { 1, 10, 25 };	// Degrees "C"
	static int incrementIdx = 1;
	static int rawAdcValue = 0;
	static float dacVoltage = 0;
	static float loopCurrent = 0;
	static int16_t commandedDegrees = 0;
	static float thermoDacValue = 0;
	static float dacExpectedVolts;
	static int16_t thermoValue, coldValue;
	static float tableValue, coldTableValue;
	static char holdKey;
	static boolean dataRead;
	float coldJunctionTemp;
	int lm35Value;
	char key = 0;
	static int useColdComp = 0;

	while (Mode == TEMPERATURE)
	{
		if (KeyState == HELD)
		{
			Serial.println(" HELD ");
			delay(KEY_REPEAT_RATE);
		}

		key = getKey();

		if (key)  // Check for a valid key.
		{
			switch (key)
			{
			case NULL:
				break;
			case '1':
				Mode = MILLI_AMPS;
				break;
			case '2':	// Increase output by increment Value
				dataRead = false;
				commandedDegrees += incrementVal[incrementIdx];
				if (commandedDegrees >= 1372)
					commandedDegrees = 1372;
				break;
			case '8':	// Decrease output by increment Value	// Use this if lowest temperature is '-270'
				dataRead = false;
				commandedDegrees -= incrementVal[incrementIdx];
				if (commandedDegrees <= -270)
					commandedDegrees = -270;
				break;
//			case '8':	// Decrease output by increment Value	// Use this if lowest temperature is '0'
//				dataRead = 0;
				//				if (commandedDegrees <= incrementVal[incrementIdx])
//					commandedDegrees = 0;
//				else
//					commandedDegrees -= incrementVal[incrementIdx];
//				break;
			case '4':	// Set increment value to next higher value
				if (incrementIdx != numIncrements)
					incrementIdx++;
				break;
			case '6':	// Set increment value to next lower value
				if (incrementIdx != 0)
					incrementIdx--;
				Serial.println("6");
				break;
			case 'A':	// Use Cold Junction compensation
				useColdComp = 1;
				Serial.println("Use cold comp");
				break;
			case 'B':	// Do not use cold junction compesation
				useColdComp = 1;
				Serial.println("Use cold comp");
				break;
			case 'C':	// Set increment value to next lower value
				break;
			case 'D':	// Set increment value to next lower value
				break;
			default:
				break;
			}
		}
//		if (useColdComp == 1)
//			commandedDegrees -= coldJunctionTemp;

		//
		// Read two bytes from EEPROM.  The temperature is the index to the data
		// and the data is two bytes wide, therefore the extra address math.
		// The data in the EEPROM is "mVolts * 1000".  That was done to get rid
		// of the decimal place and not have to use 'float' which would have used 
		// up a twice as much EEPROM space
		// Refer to thermocouple reference tables at www.omega.com
		//
		if (dataRead == false)
		{
			// read LM35 temperature sensor for cold junction compensation
			lm35Value = analogRead(LM35_PIN);
			// convert the 10bit analog value to Celcius
			coldJunctionTemp = float(lm35Value) / 1023;
			coldJunctionTemp = coldJunctionTemp * 500;	// Temperature is in degrees 'C'...
			Serial.print("Cold Junction Temp: ");
			Serial.print(coldJunctionTemp);
			Serial.println("C");

			// Read temperature value from EEPROM and compensate for cold junction temperature.
			//
			// Cold Junction temperature compensation is not going to be exact because the sensor is not
			// located exactly at the cold junction, i.e. inside the meter or other 
			// measuring device
//			coldValue = readEEPROM((((int16_t)coldJunctionTemp + 270) * 2) + 1);	// Read MSB
//			coldValue = coldValue << 8;
//			coldValue = coldValue + (readEEPROM(((int16_t)coldJunctionTemp + 270) * 2));	// Read LSB
//			Serial.print("reading cold junction temp from EEPROM... 0x");
//			Serial.println(coldValue, HEX);

			thermoValue = readEEPROM(((commandedDegrees + 270) * 2) + 1);	// Read MSB
			thermoValue = thermoValue << 8;
			thermoValue = thermoValue + (readEEPROM((commandedDegrees + 270) * 2));	// Read LSB
			Serial.print("reading commanded degrees from EEPROM... 0x");
			Serial.println(thermoValue, HEX);
			//
			// We need to preserve the sign bit for negative temperatures but
			// ignore the sign bit (use it for the value) for positive temperatures.  This is due to 
			// the data stored in the EEPROM being only 16 bits wide
			//
//			if ((int16_t)coldValue < 0)
//				coldTableValue = (float)coldValue / 1000;
//			else
//				coldTableValue = (float)((uint16_t)coldValue) / 1000;

			if (commandedDegrees < 0)
				tableValue = (float)thermoValue / 1000;
			else
				tableValue = (float)((uint16_t)thermoValue) / 1000;
		}

		//
		// Send temperature value to DAC
		// In order to get increased resolution from the DAC, the millivolt value
		// from the thermocouple table is multiplied by 50 and the opamp gain is 
		// set to negative 50 to compensate.  This sets the correct millivolt value 
		// on the simulated temperature output
		//
		thermoDacValue = ((tableValue * OPAMP_GAIN_TEMPTR));
///////////////		thermoDacValue = 4095;	///////////////////// DEBUG DEBUG DEBUG
		// Output to the DAC
		dacExpectedVolts = thermoDacValue * DAC_VOLTS_PER_COUNT;
		if (dataRead == false)	// Update the DAC
		{
			Serial.print("DAC Expected volts: ");
			Serial.print(dacExpectedVolts, 4);
			Serial.println(" Volts");
			Serial.print("Setting DAC...DAC Value (rounded) = ");
			Serial.println(round(thermoDacValue));
			thermoDac.setVoltage(round(thermoDacValue), false);
			dataRead = true;
		}

		// Display user feedback on LCD
		lcd.setCursor(0, 0);
		lcd.print(tableValue, 3);
		lcd.print("mV ");
		lcd.print("Amb ");
		lcd.print(coldJunctionTemp, 1);
		lcd.print(char(DEGREE_SYMBOL));
		lcd.setCursor(0, 1);
		lcd.print(commandedDegrees, DEC);
		lcd.print(char(DEGREE_SYMBOL));
		lcd.print("C  ");
		lcd.print(char(0x7f));
		lcd.print(char(0x7e));
		lcd.print(incrementText[incrementIdx]);
		lcd.print(char(DEGREE_SYMBOL));
		lcd.print("C  ");

	}
	return;
}

char getKey()
{
	uint16_t rawKeyValue;
	static unsigned long keyDownTimeStamp = 0;
	char heldKey;
	int SensVal = 0;

	rawKeyValue = analogRead(KEY_PAD_PIN);
	Serial.print("\tRawVal: ");
	Serial.print(rawKeyValue);	if (rawKeyValue <= 100)	// False reading
	{
		KeyState = RELEASED;
		return NULL;
	}
	// Debounce the switch
	if ((millis() - keyDownTimeStamp) < DebounceTime)
		return NULL;
	/*
	Serial.print("\tDown time: ");
	Serial.print(keyDownTimeStamp);
	Serial.print("\tmillis(): ");
	Serial.print(millis());
	Serial.print("\tdownTime - millis(): ");
	Serial.println(millis() - keyDownTimeStamp);
	*/
	if ((millis() - keyDownTimeStamp) > KeyHoldTime)
	{
		KeyState = HELD;
/////////////////		return NULL;
	}
	else
		KeyState = PRESSED;

	keyDownTimeStamp = millis();

	// Decode the keypress
	for (int i = 0; i <= NUM_KEYS-1; i++)
	{
			SensVal = constrain(rawKeyValue, rawKeys[i] - 12, rawKeys[i] + 12);
		if (SensVal == rawKeyValue)
		{
/*
			Serial.print("\tRawVal: ");
			Serial.print(rawKeyValue);
			Serial.print("\tSensVal: ");
			Serial.print(SensVal);
*/			
			return keys[i];
		}
		else if (i == (NUM_KEYS - 1))
		{
/*			
			Serial.print("\tRawVal: ");
			Serial.print(rawKeyValue);
			Serial.print("\tSensVal: ");
			Serial.print(SensVal);
*/
			return NULL;
		}
	}
}

void setHoldTime(uint16_t holdTime)
{
	KeyHoldTime = holdTime;
	return;
}
void setDebounceTime(uint16_t debounceTime)
{
	DebounceTime = debounceTime;
	return;
}

byte readEEPROM(long eeaddress)
{
	if (eeaddress < 65536)
		Wire.beginTransmission(EEPROM_ADR_LOW_BLOCK);
	else
		Wire.beginTransmission(EEPROM_ADR_HIGH_BLOCK);

	Wire.write((int)(eeaddress >> 8)); // MSB
	Wire.write((int)(eeaddress & 0xFF)); // LSB
	Wire.endTransmission();

	if (eeaddress < 65536)
		Wire.requestFrom(EEPROM_ADR_LOW_BLOCK, 1);
	else
		Wire.requestFrom(EEPROM_ADR_HIGH_BLOCK, 1);

	byte rdata = 0xFF;
	if (Wire.available()) rdata = Wire.read();
	return rdata;
}