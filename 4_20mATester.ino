/*
*
* Keck-Tronix BangDucer (Copywrite David Keck 1-29-2018)
*
*/

#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Keypad.h>

#define dacVoltsIn A6
#define dacAddress 0x64

#define OPAMP_GAIN			1.638	// Based on 4096 this would be 1.639344
#define VCC					5.00
#define REFERENCE_VOLTAGE	VCC		// VCC is currently the analog ref voltage
#define DAC_VOLTS_PER_COUNT	(REFERENCE_VOLTAGE / 4095.0)
#define CURRENT_4mA		((1/OPAMP_GAIN) / DAC_VOLTS_PER_COUNT)
#define CURRENT_2mA		(CURRENT_4mA / 2)
#define CURRENT_1mA		(CURRENT_4mA / 4)
#define CURRENT_100uA	(CURRENT_1mA / 10)
#define CURRENT_10uA	(CURRENT_1mA / 100)
#define CURRENT_125PCT	(CURRENT_4mA * 6)
#define CURRENT_100PCT	(CURRENT_4mA * 5)
#define CURRENT_25PCT	(CURRENT_4mA)
#define CURRENT_0PCT	(CURRENT_4mA)

#define KEY_HOLD_TIME	1000
#define KEY_REPEAT_RATE	250

const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns
// Define the Keymap
char keys[ROWS][COLS] = {
	{ '1','2','3','A' },
	{ '4','5','6','B' },
	{ '7','8','9','C' },
	{ '*','0','#','D' }
};
// Connect keypad ROW0, ROW1, ROW2 and ROW3 to Arduino pins.
byte rowPins[ROWS] = { 9, 8, 7, 6 };
// Connect keypad COL0, COL1, COL2 and COL3 to Arduino pins.
byte colPins[COLS] = { A3, A2, A1, A0 };

// Create the Keypad
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LCD pins
int lcdRSPin = 12;
int lcdEPin = 11;
int lcdD4Pin = 5;
int lcdD5Pin = 4;
int lcdD6Pin = 3;
int lcdD7Pin = 2;

// Initialize LCD constructor with the numbers of the interface pins
LiquidCrystal lcd(lcdRSPin, lcdEPin,
	lcdD4Pin, lcdD5Pin, lcdD6Pin, lcdD7Pin);

Adafruit_MCP4725 dac; // constructor

void setup()
{
	dac.begin(dacAddress); // The I2C Address of the MCP4725 DAC
	analogReference(DEFAULT);

	// set up serial
	// Serial.begin(9600);

	// set up the LCD's number of columns and rows: 
	lcd.begin(16, 2);

	// Print logo to the LCD.
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("  Keck-Tronix");
	lcd.setCursor(0, 1);
	lcd.print("   BangDucer");
	// Let display for 2 seconds
	delay(2000);
	
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
}

void loop(void) {

	char incrementText[3][4] = {".01", ".10", "1.0"};
	const byte numIncrements = 2;	// 0-2, total 3
	float incrementVal[] = { CURRENT_10uA, CURRENT_100uA, CURRENT_1mA };
	static int incrementIdx = 1;
	static int adcValueRead = 0;
	static float dacVoltage = 0;
	static float loopCurrent = 0;
	static float commandedCurrent = 0;
	static float filterVoltage = 0;
	static float dac_value = CURRENT_0PCT;
	static float dacExpectedVolts;
	static char holdKey;

	kpd.setHoldTime(KEY_HOLD_TIME);		// Time to wait until key repeats
	if (kpd.getState() == HOLD)	
		delay(KEY_REPEAT_RATE);

	char key = kpd.getKey();
	if (key || kpd.getState() == HOLD)  // Check for a valid key.
	{
		if(key)
			holdKey = key;	// Keypress is read only once if held down, so store it for key repeat
		switch (holdKey)
		{
		case '2':	// Increase current output by increment Value
			dac_value += incrementVal[incrementIdx];
			if (dac_value >= CURRENT_125PCT)
				dac_value = CURRENT_125PCT;
//			lcd.setCursor(9, 0);	// for future use with an LCD that has up/down arrows
//			lcd.print((char)0xff);
			break;
		case '8':	// Decrease current output by increment Value
			if (dac_value <= incrementVal[incrementIdx])
				dac_value = 0;
			else
				dac_value -= incrementVal[incrementIdx];
			break;
		case '4':	// Set current increment value to next higher value
			if (incrementIdx != numIncrements)
				incrementIdx++;
			lcd.setCursor(6, 0);
			lcd.print(incrementText[incrementIdx]);
			break;
		case '6':	// Set current increment value to next lower value
			if (incrementIdx != 0)
				incrementIdx--;
			lcd.setCursor(6, 0);
			lcd.print(incrementText[incrementIdx]);
			break;
		case 'A':	// Set current output to 100%, 20mA
			dac_value = CURRENT_100PCT;
			break;
		case 'B':	// Increase current output up 25%, max 125%, 24mA
			dac_value += CURRENT_25PCT;
			if (dac_value >= CURRENT_125PCT)
				dac_value = CURRENT_125PCT;
			break;
		case 'C':	// Current output down 25%, max down -25%, 0mA
			if (dac_value <= CURRENT_0PCT)
				dac_value = 0;
			else
				dac_value -= CURRENT_25PCT;
			break;
		case 'D':	// Set current output to 0%, 4mA
			dac_value = CURRENT_0PCT;
			break;
		default:
			// Serial.println(key);
			lcd.setCursor(0, 0);
			lcd.print(key);
		}
	}
	// Output to the DAC
	dacExpectedVolts = dac_value * DAC_VOLTS_PER_COUNT;
	dac.setVoltage(round(dac_value), false);
//	delay(100);	// It seems there is no delay needed for DAC to settle in this app

	// Serial.print("\tExpected Volts: ");
	// Serial.print(dacExpectedVolts, 3);

	// Serial.print("\tDAC Value: ");
	// Serial.println(dac_value, 2);
	// Serial.println(round(dac_value));

	adcValueRead = analogRead(A6);
	dacVoltage = (adcValueRead * REFERENCE_VOLTAGE) / 1024.0;

	// Serial.print("\tADC Value: ");
	// Serial.println(adcValueRead);

	// Serial.print("\tArduino Volts: ");
	// Serial.println(dacVoltage, 3);

	// display on LCD
	lcd.setCursor(12, 0);
	lcd.print(dacExpectedVolts, 2);

	// display on LCD
	lcd.setCursor(12, 1);
	lcd.print(dacVoltage, 2);

	// Display the commanded current on LCD
	commandedCurrent = (dacExpectedVolts / (.250 / OPAMP_GAIN));	// 250ohm resistor, ".250" for display of milli
	lcd.setCursor(0, 0);
	lcd.print(" ");	// Clear decimal artifacts
	if (commandedCurrent >= 10)	// A leading digit (10's) artifact occurs when display switches between 9 and 10
		lcd.setCursor(0, 0);
	else
		lcd.setCursor(1, 0);
	lcd.print(commandedCurrent, 2);

	// Display loop current on LCD
	loopCurrent = (dacVoltage / (.250 / OPAMP_GAIN));	// 250ohm resistor, ".250" for display of milli
	lcd.setCursor(0, 1);
	lcd.print(" ");	// Clear decimal artifacts
	if (loopCurrent >= 10)	// A leading digit (10's) artifact occurs when display switches between 9 and 10
		lcd.setCursor(0, 1);
	else
		lcd.setCursor(1, 1);
	lcd.print(loopCurrent, 2);
}