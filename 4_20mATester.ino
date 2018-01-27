/*
* Adjust the LCDs contrast with the Potentiometer until you
* can see the characters on the LCD.
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

// include the library
#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Keypad.h>

#define dacVoltsIn A6
#define dacAddress 0x64
#define ledpin 13

#define OPAMP_GAIN			2.00
#define VCC					4.94
#define REFERENCE_VOLTAGE	VCC		// VCC is currently the ref voltage
#define DAC_VOLTS_PER_COUNT	(VCC / 4095.0)
#define CURRENT_4mA		(.5 / DAC_VOLTS_PER_COUNT)
#define CURRENT_2mA		(CURRENT_4mA / 2)
#define CURRENT_1mA		(CURRENT_4mA / 4)
#define CURRENT_100uA	(CURRENT_1mA / 10)
#define CURRENT_10uA	(CURRENT_1mA / 100)
#define CURRENT_125PCT	(CURRENT_4mA * 6)
#define CURRENT_100PCT	(CURRENT_4mA * 5)
#define CURRENT_25PCT	(CURRENT_4mA)
#define CURRENT_0PCT	(CURRENT_4mA)
// Cursor positions
#define CURSOR_ROW		0
#define CURSOR_1mA		1
#define CURSOR_01mA		3
#define CURSOR_001mA	4

const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns
					 // Define the Keymap
char keys[ROWS][COLS] = {
	{ '1','2','3','A' },
	{ '4','5','6','B' },
	{ '7','8','9','C' },
	{ '*','0','#','D' }
};
// Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins.
byte rowPins[ROWS] = { 9, 8, 7, 6 };
// Connect keypad COL0, COL1 and COL2 to these Arduino pins.
byte colPins[COLS] = { A3, A2, A1, A0 };

// Create the Keypad
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// all of our LCD pins
int lcdRSPin = 12;
int lcdEPin = 11;
int lcdD4Pin = 5;
int lcdD5Pin = 4;
int lcdD6Pin = 3;
int lcdD7Pin = 2;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(lcdRSPin, lcdEPin,
	lcdD4Pin, lcdD5Pin, lcdD6Pin, lcdD7Pin);

Adafruit_MCP4725 dac; // constructor
uint32_t dac_value = CURRENT_0PCT;
byte x = CURSOR_1mA;

void setup()
{
	dac.begin(dacAddress); // The I2C Address: Run the I2C Scanner if you're not sure  
	analogReference(DEFAULT);

	//TEMP
	pinMode(ledpin, OUTPUT);
	digitalWrite(ledpin, HIGH);
	
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
	lcd.clear();
	lcd.setCursor(5, 0);
	lcd.print("mA");
	lcd.setCursor(9, 0);
	lcd.print("Ex");
	lcd.setCursor(5, 1);
	lcd.print("mA");
	lcd.setCursor(9, 1);
	lcd.print("In ");
	lcd.noCursor();
}

void loop(void) {

	char incrementText[3][1] = { "1.0", "0.1", ".01" };
	float incrementVal[3] = { CURRENT_1mA, CURRENT_100uA, CURRENT_10uA };
	int incrementIdx = 1;
	int adcValueRead = 0;
	float dacVoltage = 0;
	float loopCurrent = 0;
	float filterVoltage = 0;

	float dacExpectedVolts;

	char key = kpd.getKey();
	if (key)  // Check for a valid key.
	{
		switch (key)
		{
		case '2':	// 
			dac_value += CURRENT_1mA;
			if (dac_value >= CURRENT_125PCT)
				dac_value = CURRENT_125PCT;
			break;
		case '8':	// Set current output to 
			if (dac_value <= CURRENT_1mA)
				dac_value = 0;
			else
				dac_value -= CURRENT_1mA;
			break;
		case '4':	// Move cursor left
			lcd.setCursor(5, 0);
			lcd.print("1.0mA");
			lcd.print("0.1mA");
			lcd.print(".01mA");
			break;
		case '6':	// Move cursor right
			lcd.setCursor(5, 0);
			lcd.print("1.0mA");
			lcd.print("0.1mA");
			lcd.print(".01mA");
			break;
		case 'A':	// Set current output to 100%, 20mA
			lcd.noCursor();
			dac_value = CURRENT_100PCT;
			break;
		case 'B':	// Current output up 25%, max 24mA
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
			lcd.clear();
			lcd.print(key);
		}
	}
	dacExpectedVolts = dac_value * DAC_VOLTS_PER_COUNT;
	dac.setVoltage(dac_value, false);
	delay(250);

	Serial.print("\tExpected Voltage: ");
	Serial.print(dacExpectedVolts, 3);

	Serial.print("DAC Value: ");
	Serial.print(dac_value);

	adcValueRead = analogRead(A6);
	dacVoltage = (adcValueRead * REFERENCE_VOLTAGE) / 1024.0;

	Serial.print("\tArduino ADC Value: ");
	Serial.print(adcValueRead);

	Serial.print("\tArduino Voltage: ");
	Serial.println(dacVoltage, 3);

	// display on LCD
	lcd.setCursor(12, 0);
	lcd.print(dacExpectedVolts, 2);

	// display on LCD
	lcd.setCursor(12, 1);
	lcd.print(dacVoltage, 2);

	loopCurrent = (dacVoltage / (.250 / OPAMP_GAIN));	// 250ohm resistor, ".250" for display of milli
	// Display loop current on LCD
	lcd.setCursor(0, 1);
	lcd.print("     mA");	// Clear decimal artifacts
	if (round(loopCurrent) >= 10)
		lcd.setCursor(0, 1);
	else
		lcd.setCursor(1, 1);
	lcd.print(loopCurrent, 2);
	Serial.print(" round ");
	Serial.print(round(loopCurrent));
}