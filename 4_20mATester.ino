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

//#define VOLTS_PER_COUNT	.001221	// 5.00 VOLTS MAX
#define VOLTS_PER_COUNT	.001208	// 4.95 VOLTS MAX
#define CURRENT_4mA		1/VOLTS_PER_COUNT
#define CURRENT_2mA		CURRENT_4mA / 2
#define CURRENT_1mA		CURRENT_4mA / 4
#define CURRENT_125PCT	CURRENT_4mA * 5
#define CURRENT_100PCT	CURRENT_4mA * 4
#define CURRENT_25PCT	CURRENT_4mA
#define CURRENT_0PCT	CURRENT_4mA


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
	lcd.setCursor(0, 0);
	lcd.print("Exp Volts ");
	lcd.setCursor(0, 1);
	lcd.print("Volts In ");
	lcd.cursor();
	lcd.setCursor(0, 1);
}

void loop(void) {

//	uint32_t dac_value = CURRENT_0PCT;
	int adcValueRead = 0;
	float dacVoltage = 0;
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
		case '4':	// Set current output to
			dac_value = CURRENT_100PCT;
			break;
		case '6':	// Set current output to
			dac_value = CURRENT_100PCT;
			break;
		case 'A':	// Set current output to 100%, 20mA
			digitalWrite(ledpin, LOW);
			dac_value = CURRENT_100PCT;
			break;
		case 'B':	// Current output up 25%, max 24mA
			digitalWrite(ledpin, HIGH);
			dac_value += CURRENT_25PCT;
			if (dac_value >= CURRENT_125PCT)
				dac_value = CURRENT_125PCT;
			break;
		case 'C':	// Current output down 25%, down to 0mA
			digitalWrite(ledpin, LOW);
			if (dac_value <= CURRENT_0PCT)
				dac_value = 0;
			else
				dac_value -= CURRENT_25PCT;
			break;
		case 'D':	// Set current output to 0%, 4mA
			digitalWrite(ledpin, HIGH);
			dac_value = CURRENT_0PCT;
			break;
		default:
			Serial.println(key);
			lcd.clear();
			lcd.print(key);
		}
	}
//		dacExpectedVolts = (5.0 / 4096.0) * dac_value;	// .001221 Volts/count
	dacExpectedVolts = (4.95 / 4096.0) * dac_value; // .001208 Volts/count
	dac.setVoltage(dac_value, false);
	delay(250);

	Serial.print("\tExpected Voltage: ");
	Serial.print(dacExpectedVolts, 3);
	// display it on LCD
	lcd.setCursor(10, 0);
	lcd.print(dacExpectedVolts, 3);

	adcValueRead = analogRead(A6);
//		dacVoltage = (adcValueRead * 5.0) / 1024.0;
	dacVoltage = (adcValueRead * 4.95) / 1024.0;
	// display it on LCD
	lcd.setCursor(10, 1);
	lcd.print(dacVoltage, 3);

	Serial.print("DAC Value: ");
	Serial.print(dac_value);

	Serial.print("\tArduino ADC Value: ");
	Serial.print(adcValueRead);

	Serial.print("\tArduino Voltage: ");
	Serial.println(dacVoltage, 3);
}