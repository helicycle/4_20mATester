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
#define voltsIn A0

// all of our LCD pins
int lcdRSPin = 12;
int lcdEPin = 11;
int lcdD4Pin = 5;
int lcdD5Pin = 4;
int lcdD6Pin = 3;
int lcdD7Pin = 2;

Adafruit_MCP4725 dac; // constructor
int pwmVoltageOutPin = 9;     // LED connected to digital pin 9

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(lcdRSPin, lcdEPin,
	lcdD4Pin, lcdD5Pin, lcdD6Pin, lcdD7Pin);

void setup()
{
	// set up the LCD's number of columns and rows: 
	lcd.begin(16, 2);

	// Print a message to the LCD.
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("  KECK-TRONICS");
	lcd.setCursor(0, 1);
	lcd.print(" PROCESS TESTER");

	dac.begin(0x62); // The I2C Address: Run the I2C Scanner if you're not sure  
	pinMode(pwmVoltageOutPin, OUTPUT);   // sets the pin as output

	// set up serial
	Serial.begin(9600);
}

void loop(void) {

	uint32_t dac_value;
	int adcValueRead = 0;
	float voltageRead = 0;

	float dac_expected_output;

	// Let logo from setup display for 1/2 second
	delay(2000);


//	for (dac_value = 0; dac_value < 4096; dac_value = dac_value + 15)
	for (dac_value = 0; dac_value < 256; dac_value = dac_value + 1)
		{
//		dac_expected_output = (5.0 / 4096.0) * dac_value;
		dac.setVoltage(dac_value, false);

		analogWrite(pwmVoltageOutPin, dac_value);  // analogRead values go from 0 to 1023, analogWrite values from 0 to 255

//		delay(250);
		delay(250);

		adcValueRead = analogRead(voltsIn);
		voltageRead = (adcValueRead * 5.0) / 1024.0;

		Serial.print("DAC Value: ");
		Serial.print(dac_value);

		Serial.print("\tExpected Voltage: ");
		Serial.print(dac_expected_output, 3);

		Serial.print("\tArduino ADC Value: ");
		Serial.print(adcValueRead);

		Serial.print("\tArduino Voltage: ");
		Serial.println(voltageRead, 3);

		// display it on LCD
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("ADC Val");
		lcd.setCursor(8, 0);
		lcd.print("ADC Volt");
		lcd.setCursor(0, 1);
		lcd.print(adcValueRead);
		lcd.print(" Raw");
		lcd.setCursor(10, 1);
		lcd.print(voltageRead);
		lcd.print("V");
	}
}