/*
*
* The circuit:
* - LCD RS pin to digital pin 12
* - LCD Enable pin to digital pin 11
* - LCD D4 pin to digital pin 5
* - LCD D5 pin to digital pin 4
* - LCD D6 pin to digital pin 3
* - LCD D7 pin to digital pin 2
* - LCD R/W pin to ground
* - 10K potentiometer divider for LCD pin VO:
* - 330ohm resistor betweenm LCD pin A and 5v
* - LCD pin K to ground
*/

#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Keypad.h>

#define dacVoltsIn A6
#define dacAddress 0x64

#define OPAMP_GAIN			2.00
#define VCC					4.93
#define REFERENCE_VOLTAGE	VCC		// VCC is currently the analog ref voltage
#define DAC_VOLTS_PER_COUNT	(VCC / 4095.0)
#define CURRENT_4mA		((1/OPAMP_GAIN) / DAC_VOLTS_PER_COUNT)
#define CURRENT_2mA		(CURRENT_4mA / 2)
#define CURRENT_1mA		(CURRENT_4mA / 4)
#define CURRENT_100uA	(CURRENT_1mA / 10)
#define CURRENT_10uA	(CURRENT_1mA / 100)
#define CURRENT_125PCT	(CURRENT_4mA * 6)
#define CURRENT_100PCT	(CURRENT_4mA * 5)
#define CURRENT_25PCT	(CURRENT_4mA)
#define CURRENT_0PCT	(CURRENT_4mA)

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
	Serial.begin(9600);

	// set up the LCD's number of columns and rows: 
	lcd.begin(16, 2);

	// Print logo to the LCD.
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("  KECK-TRONICS");
	lcd.setCursor(0, 1);
	lcd.print(" PROCESS TESTER");
	// Let display for 2 seconds
	delay(500);
	
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
	const byte maxIncrements = 3;
	float incrementVal[maxIncrements] = { CURRENT_10uA, CURRENT_100uA, CURRENT_1mA };
	static int incrementIdx = 3;
	static int adcValueRead = 0;
	static float dacVoltage = 0;
	static float loopCurrent = 0;
	static float commandedCurrent = 0;
	static float filterVoltage = 0;
	static int16_t dac_value = CURRENT_0PCT;
	static float dacExpectedVolts;

	kpd.setHoldTime(100);

	char key = kpd.getKey();
	if (key)  // Check for a valid key.
	{
		switch (key)
		{
		case '2':	// Increment output Current by increment Value
			dac_value += incrementVal[incrementIdx];
			if (dac_value >= CURRENT_125PCT)
				dac_value = CURRENT_125PCT;
			break;
		case '8':	// Decrement output Current by increment Value
			if ((dac_value <= 0) || (dac_value > CURRENT_125PCT))
				dac_value = 0;
			else
				dac_value -= incrementVal[incrementIdx];
			break;
		case '4':	// Set current increment value to next higher value
			if (incrementIdx != maxIncrements)
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
			lcd.noCursor();
			dac_value = CURRENT_100PCT;
			break;
		case 'B':	// Increase current output up 25%, max 125%, 24mA
			lcd.noCursor();
			dac_value += CURRENT_25PCT;
			if (dac_value >= CURRENT_125PCT)
				dac_value = CURRENT_125PCT;
			break;
		case 'C':	// Current output down 25%, down to 0mA
			lcd.noCursor();
			if (dac_value <= CURRENT_0PCT)
				dac_value = 0;
			else
				dac_value -= CURRENT_25PCT;
			break;
		case 'D':	// Set current output to 0%, 4mA
			lcd.noCursor();
			dac_value = CURRENT_0PCT;
			break;
		default:
			Serial.println(key);
			lcd.setCursor(0, 0);
			lcd.print(key);
		}
	}
	// Output to the DAC
	dacExpectedVolts = dac_value * DAC_VOLTS_PER_COUNT;
	dac.setVoltage(dac_value, false);
	////////////////delay(100);

	Serial.print("\tExpected Volts: ");
	Serial.print(dacExpectedVolts, 3);

	Serial.print("DAC Value: ");
	Serial.print(dac_value);

	adcValueRead = analogRead(A6);
	dacVoltage = (adcValueRead * REFERENCE_VOLTAGE) / 1024.0;

	Serial.print("\tADC Value: ");
	Serial.print(adcValueRead);

	Serial.print("\tArduino Volts: ");
	Serial.println(dacVoltage, 3);

	// display on LCD
	lcd.setCursor(12, 0);
	lcd.print(dacExpectedVolts, 2);

	// display on LCD
	lcd.setCursor(12, 1);
	lcd.print(dacVoltage, 2);

	// Display the commanded current on LCD
	commandedCurrent = (dacExpectedVolts / (.250 / OPAMP_GAIN));	// 250ohm resistor, ".250" for display of milli
	lcd.setCursor(0, 0);
	lcd.print("      ");	// Clear decimal artifacts
	if (round(commandedCurrent) >= 10)	// The decimal location shifts and artifacts occur when display switches between 9 and 10
		lcd.setCursor(0, 0);
	else
		lcd.setCursor(1, 0);
	lcd.print(commandedCurrent, 2);

	// Display loop current on LCD
	loopCurrent = (dacVoltage / (.250 / OPAMP_GAIN));	// 250ohm resistor, ".250" for display of milli
	lcd.setCursor(0, 1);
	lcd.print("      mA");	// Clear decimal artifacts
	if (round(loopCurrent) >= 10)	// The decimal location shifts and artifacts occur when display switches between 9 and 10
		lcd.setCursor(0, 1);
	else
		lcd.setCursor(1, 1);
	lcd.print(loopCurrent, 2);
}