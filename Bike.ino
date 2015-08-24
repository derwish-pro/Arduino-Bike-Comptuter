/*

Button (to change screens): 
S > 2
MID > 5V
- > GND

Joystic (gercon emulation) :
GND > GND
+5V > 5V
VRx > A0

Led:
> GND
> 11

OLED
SDA > SDA
SCL > SCL
GND > GND
VCC > 5V



*/




#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <sd.h>
#include "Time.h"

//--------------SETINGS------------
#define writeLog false //write log to SD
#define radiusInch  26 //wheel radius in inches
#define gerkonPin A0
#define modeButtonPin 2
#define ledPin 11  
#define buttonsDebounceDelay 100
#define gerkonDebounce 100
#define gerkonTreshold 5  // sensitivity of gerkon
#define inactiveTimeToSpeedZero 2000 //if no pulse for this time (ms), speed will be 0 
//#define inactiveTimeToTripEnd 900000 //if no pulse for this time (ms), trip will be stopped
#define inactiveTimeToTripEnd 5000 //if no pulse for this time (ms), trip will be stopped
#define inactiveTimeToStandBy 10000 //if no pulse and buttons press for this time (ms),  standby mode will activated
//---------------------------------



#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

//File myFile;



boolean modeButtonWas=0;
float modeButtonDebounceTimer=0;
bool modeButtonPressed=false;
byte currentMode = 0;

int gerkonMagnetVal = 0;
int gerkonVal = 0;    
unsigned long lastPulseTime=0;
int pulsesCount = 0;
unsigned long totalPulsesCount = 0;
bool isPulseNow = false;
boolean isMooving = false;

bool standByMode = false;
unsigned long standByModeTimer = 0;

boolean tripStarted = false;
unsigned long tripStartTime=0;
long stopTime=0;
unsigned long stoppedMilliseconds = 0; 

float wheelCircumference=0;
float tripDistance = 0;
float currentSpeed = 0;          
float averageSpeed = 0;     

   

      



void setup() {

	//display init
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
	display.clearDisplay();
	display.setTextColor(WHITE);

	//calculations
	float raduisMM = radiusInch * 25.4f;
	wheelCircumference = raduisMM * 2 * 3.14f;
	
	//sensors init
	pinMode(gerkonPin, INPUT);

	pinMode(ledPin, OUTPUT);  

	CalibrateGerkon();

	//InitSD();

	lastPulseTime = millis();     
	tripStartTime = millis();   
}

void loop(){
	CheckGerkon();
	CalculateSpeed();
	CheckModeButton();
	RefreshDisplay();
	digitalWrite(ledPin, isPulseNow);
}






//-------------------------------------------------------------------
//					GERKON
//-------------------------------------------------------------------


void CalculateSpeed()
{


	// if no pulse for long time, reset speed to 0
	if (millis() - lastPulseTime > inactiveTimeToSpeedZero)
		currentSpeed = 0;

	// if no pulse for long time, stop trip
	if (millis() - lastPulseTime > inactiveTimeToTripEnd) {

		tripStarted = false;
	}

	//if we stop mooving
	if (isMooving && currentSpeed == 0) {
		stopTime = millis();
		isMooving = false;
	}
	//if we start to move
	else if (!isMooving && currentSpeed != 0) {

		stoppedMilliseconds += millis() - stopTime;
		isMooving = true;

		//if new trip
		if (!tripStarted)
			StartNewTrip();
	}
}

void StartNewTrip()
{
	tripStarted = true;
	pulsesCount = 0;
	tripDistance = 0;
	averageSpeed = 0;
	tripStartTime = millis();
	stoppedMilliseconds = 0;
	stopTime = millis();
	//WriteTextToSD("---------------");	
}

void CalibrateGerkon()
{
	Print("Put magnet to gercon for calibrating...         ");
	delay(1000);
	gerkonMagnetVal = analogRead(gerkonPin);
	isPulseNow = true;
}


void CheckGerkon()
{
	gerkonVal = analogRead(gerkonPin);

	if (abs(gerkonMagnetVal - gerkonVal) <= gerkonTreshold) {
		if (!isPulseNow && millis() - lastPulseTime > gerkonDebounce) {
			isPulseNow = true;
			Pulse();
		}		
	}
	else{
		isPulseNow = false;			
	}	
}


void Pulse() {

	//if new trip
	if (!tripStarted)
		StartNewTrip();

	pulsesCount++;
	totalPulsesCount++;

	tripDistance = (float)wheelCircumference * pulsesCount * 0.001;

	unsigned long elapsedTime = millis() - lastPulseTime;
	currentSpeed = wheelCircumference / elapsedTime * 3600 * 0.001;

	unsigned long moovingMilliseconds = millis() - tripStartTime - stoppedMilliseconds;
	float moovingSeconds = (float)moovingMilliseconds * 0.001;

	if (moovingSeconds < 1) // first time it is 0 and we have bug in devide by 0
		averageSpeed = 0;
	else {
		averageSpeed = tripDistance / moovingSeconds;
		averageSpeed = averageSpeed * 3, 6;
	}

	lastPulseTime = millis();

	standByModeTimer = millis();
	standByMode = false;
}





//-------------------------------------------------------------------
//					DISPLAY
//-------------------------------------------------------------------




void RefreshDisplay()
{
	if (standByMode){
		PrintStandBy();
		return;
	}

	if (!standByMode && millis() - standByModeTimer > inactiveTimeToStandBy)
		standByMode = true;

	switch (currentMode)
	{
	case 0:
		PrintSpeedDistanceTime();
		break;
	case 1:
		PrintSpeedAvaragespeedTime();
		break;
	case 2:
		PrintSpeedTotaldistanceTime();
		break;
	case 3:
		PrintGerkonDebug();
		break;
	case 4:
		PrintTime();
		break;
	case 5:
		Print5();
		break;
	default:
		break;
	}
}

String GetCurrentTime()
{
	byte sec = second();
	byte min = minute();

	String secString = (String)sec;
	String minString = (String)min;

	if (sec < 10) secString = "0" + secString;
	if (min < 10) minString = "0" + minString;

	return minString + ":" + secString;
}

void Print(String text)
{
	int size;
	if (text.length() <= 4)size = 5;
	else if (text.length() <= 10)size = 4;
	else if (text.length() <= 14)size = 3;
	else if (text.length() <= 40)size = 2;
	else size = 1;

	display.clearDisplay();
	Print(text, size, 0, 0);
}

void Print(String text, int size, int x, int y)
{
	display.setTextSize(size);
	display.setTextColor(WHITE);
	display.setCursor(x, y);
	display.print(text);
	display.display();
}

void PrintVerticalScrollBar(int x, int y, int length, int value)
{
	int pointPosition = map(value, 0, 100, y, y + length - 5);
	display.drawFastVLine(x, y, length, WHITE);
	display.drawRect(x - 2, pointPosition, 6, 6, WHITE);
}




void PrintSpeedDistanceTime() {
	if (!modeButtonPressed){
		display.clearDisplay();

		//print speed
		if (tripStarted)
			PrintMainBig((String)(int)(currentSpeed));
		else
			PrintMainBig("P");

		//print distance
		if (tripDistance < 1000)
			PrintLeft((String)(int)(tripDistance));
		else
			PrintLeft((String)(float)(tripDistance * 0.001));

		//print time
		PrintRight(GetCurrentTime());

		PrintGrid();

		display.display();
	}
	else
	{
		display.clearDisplay();
		PrintMain("SPEED");
		PrintLeft("DIST");
		PrintRight("TIME");
		PrintGrid();
		PrintVerticalScrollBar(124, 5, 30, 0);
		display.display();
	}
}



void PrintSpeedAvaragespeedTime() {
	if (!modeButtonPressed){
		display.clearDisplay();

		//print speed
		if (tripStarted)
			PrintMainBig((String)(int)(currentSpeed));
		else
			PrintMainBig("P");

		//print averageSpeed
		PrintLeft((String)(float)(averageSpeed));

		//print time
		PrintRight(GetCurrentTime());

		PrintGrid();

		display.display();
	}
	else
	{
		display.clearDisplay();
		PrintMain("SPEED");
		PrintLeft("AVAR");
		PrintRight("TIME");
		PrintGrid();
		PrintVerticalScrollBar(124, 5, 30, 20);
		display.display();
	}
}


void PrintSpeedTotaldistanceTime() {
	if (!modeButtonPressed){
		display.clearDisplay();

		//print speed
		if (tripStarted)
			PrintMainBig((String)(int)(currentSpeed));
		else
			PrintMainBig("P");

		//print total distance
		display.setTextSize(2);
		display.setCursor(0, 50);
		float totalDistance = (float)totalPulsesCount*wheelCircumference * 0.000001;
		if (totalDistance<10)
			display.print(totalDistance, 3);
		else if (totalDistance<100)
			display.print(totalDistance, 2);
		else if (totalDistance<1000)
			display.print(totalDistance, 1);
		else display.print(totalDistance, 0);


		//print time
		PrintRight(GetCurrentTime());

		PrintGrid();

		display.display();
	}
	else
	{
		display.clearDisplay();
		PrintMain("SPEED");
		PrintLeft("TOTAL");
		PrintRight("TIME");
		PrintGrid();
		PrintVerticalScrollBar(124, 5, 30, 40);
		display.display();
	}
}

void PrintGerkonDebug() {
	if (!modeButtonPressed){
		display.clearDisplay();

		//print gerkon current value
		PrintMainBig((String)gerkonVal);

		//print gerkon magnet value
		PrintLeft((String)gerkonMagnetVal);

		//print gerkon pulse
		if (isPulseNow)
			PrintRight("PULSE");

		PrintGrid();

		display.display();
	}
	else
		{
			display.clearDisplay();
			PrintMain("GERKON");
			PrintLeft("MAGN");
			PrintRight("PULSE");
			PrintGrid();
			PrintVerticalScrollBar(124, 5, 30, 60);
			display.display();
		}
}



void PrintTime() {
	if (!modeButtonPressed){
		display.clearDisplay();

		//print time
		display.setTextSize(4);
		display.setCursor(5, 15);
		display.print(GetCurrentTime());
		
		display.display();
	}
	else
	{
		display.clearDisplay();
		PrintMain("TIME");
		PrintVerticalScrollBar(124, 5, 30, 80);
		display.display();
	}
}



void Print5() {
	if (!modeButtonPressed){
		display.clearDisplay();

		//print time
		display.setTextSize(4);
		display.setCursor(5, 15);
		display.print(GetCurrentTime());

		display.display();
	}
	else
	{
		display.clearDisplay();
		PrintMain("TIME");
		PrintVerticalScrollBar(124, 5, 30, 100);
		display.display();
	}
}


void PrintStandBy() {
		display.clearDisplay();

		//print time
		PrintRight(GetCurrentTime());

		display.display();

}


void PrintMain(String text)
{
	display.setTextSize(2);
	display.setCursor(0, 15);
	display.print(text);
}

void PrintMainBig(String text)
{
	display.setTextSize(5);
	display.setCursor(0, 0);
	display.print(text);
}

void PrintLeft(String text)
{
	display.setTextSize(2);
	display.setCursor(0, 50);
	display.print(text);
}

void PrintRight(String text)
{
	display.setTextSize(2);
	display.setCursor(68, 50);
	display.print(text);
}


void PrintGrid()
{
	display.drawFastHLine(0, 45, 128, WHITE);
	display.drawFastVLine(64, 45, 19, WHITE);
}


//-------------------------------------------------------------------
//					BUTTONS
//-------------------------------------------------------------------





void CheckModeButton()
{
	modeButtonPressed = !digitalRead(modeButtonPin);
	if (modeButtonPressed != modeButtonWas)
	{
		//debounce
		if (millis() - modeButtonDebounceTimer < buttonsDebounceDelay)return;

		modeButtonWas = modeButtonPressed;
		if (modeButtonPressed == false) return;

		modeButtonDebounceTimer = millis();
		
		standByModeTimer = millis();

		if (standByMode)
			standByMode = false;
		else
			ChangeModeToNext();
	}
}


void ChangeModeToNext()
{
	currentMode++;
	if (currentMode >= 6)
		currentMode = 0;
}




//-------------------------------------------------------------------
//					SD CARD
//-------------------------------------------------------------------






/*
void InitSD()
{
	if (!writeLog)return;
	
	print("Init SD...");
	if (!SD.begin(8)) {
	print("Init failed!");
	delay(20000);
	return;
	}
	else{
	delay(2000);
	print("Init OK!!");
	delay(4000);

	myFile = SD.open("setup.txt");
	if (myFile) {
	print("wheel:");
	char c[5];
	int count = 0;
	// read from the file until there's nothing else in it:
	while (myFile.available()) {
	c[count] = myFile.read();
	count++;
	}

	myFile.close(); // close the file:
	delay(200);
	circumference = 0;                          //reset the wheel diameter
	for (count = 0; count<4; count++){    //it supposes the diameter is expressed in mm and composed of 4 characters written in a text file on the sd card
	circumference = circumference + (c[count] - 48)*pow(10, (3 - count));  //each character reappresents a number. 0 is equal 48 in ascii code. From then on you add 1 to get the next numbers.
	}
	circumference = circumference + 1;    //magic number
	print((String)circumference,0,1);
	delay(200);
	}
	else {
	// if the file didn't open, print an error:
	print("error reading...");
	delay(20000);
	}
	}
	display.clearDisplay();
}

void WriteTextToSD(String text)
{
	if (!writeLog)return;

	File logFile = SD.open("bike_log.txt", FILE_WRITE);

	if (logFile) {
		logFile.print(text);
		logFile.println("");
		logFile.close();
	}
	else {
		print("SD FAILD");
		delay(2000);
	}	
}

*/