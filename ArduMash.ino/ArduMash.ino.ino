#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

// Button Values
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
int lcd_key     = 0;
int buttonDelay = 350;
unsigned long lastInput;

// Temprature Probe Settings
#define TEMPRATURE true
#define ONE_WIRE_BUS 2
//
// Display Setting
#define DISPLAY_PRESENT true

// Relay Switch Settings
#define RELAY true
#define RELAY_PIN 3

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
// arrays to hold device addresses
DeviceAddress insideThermometer, outsideThermometer;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

long minute = 60000; // 60000 milliseconds in a minute
long second =  1000; // 1000 milliseconds in a second

struct recipeStep {
  byte  recipe;
  byte  recipeStep;
  float targetTemp;
  unsigned long  duration;
  bool  autoContinue;
  unsigned long startTime;
  byte  stepStatus; // 0 = Initial, 1=Running, 2= Interrupted, 3=Finished.
  char message[17];
};

//recipeStep currentStep = (recipeStep){1, 1, 68, 60000, false, 0, 0};
recipeStep recipe[1][9];

// Heating State
bool heatingState = false;
float tempC;

// Recipe State
byte currentStep = 0;
byte currentRecipe = 0;

// Meue State

//System State
// 0 = Initial Nothing Started
// 1 = Recipe Started
// 2 = Recipe End
byte systemState = 0;
byte lastSystemState;
void setup(void)
{
  // start serial port
  Serial.begin(9600);
  Serial.println("ArduMash 0.1.1");

/*
  // Recipe can't be zero
  // Recipe, Step, Temp, Duration, AutoContinue, StartTime, Status
  recipe[0][0]  = (recipeStep){1, 0,  72.0,  0, false, 0, 0,"Einmaischen"}; // Signal zum Einmaischen, Malz zugabe bestätigen  
  recipe[0][1]  = (recipeStep){1, 1,  68.0, 60, true,  0, 0,""}; // Maishen
  recipe[0][2]  = (recipeStep){1, 2,  72.0,  0, false, 0, 0,"Jod Test"}; // Verzuckerungsrast, Nach Jod Test bestätigen
  recipe[0][3]  = (recipeStep){1, 3,  78.0,  0, true,  0, 0,""}; // Enzymdeaktivierung.
  recipe[0][4]  = (recipeStep){1, 4,   0.0,  0, false, 0, 0,"Läutern"}; // Läutern, Nach dem Läutern Bestätigen
  recipe[0][5]  = (recipeStep){1, 5, 100.0,  0, false, 0, 0,"1. Hopfen gabe"}; // Kochen, signal für die erste Hopfengabe bestätigen
  recipe[0][6]  = (recipeStep){1, 6, 100.0, 30, true,  0, 0,"2. Hopfen gabe"}; // Kochen, signal für die zweit Hopfengabe
  recipe[0][7]  = (recipeStep){1, 7, 100.0, 30, true,  0, 0,""}; // Kochen, signal Whirpool und umfüllen
  recipe[0][8]  = (recipeStep){1, 8,   0.0,  0, false, 0, 0,"Kochen Beendet"}; // Ende */


 // Recipe, Step, Temp, Duration, AutoContinue, StartTime, Status
  recipe[0][0]  = (recipeStep){1, 0,  72.0,  1, false, 0, 0,"Einmaischen"}; // Signal zum Einmaischen, Malz zugabe bestätigen  
  recipe[0][1]  = (recipeStep){1, 1,  68.0,  1, true,  0, 0,""}; // Maishen
  recipe[0][2]  = (recipeStep){1, 2,  72.0,  1, false, 0, 0,"1. Hopfen Gabe"}; // Verzuckerungsrast, Nach Jod Test bestätigen

// Setup the temprature probe  
  setupTemprature();
  Serial.println("Temprature Steup Done");
  
// set the digital pin as output:
  pinMode(RELAY_PIN, OUTPUT);
  Serial.print("Relay set to pin:");
  Serial.println(RELAY_PIN);

// set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.clear();
  Serial.println("Display Setup DONE");
  bootScreen();
  delay(3000);

} // End Setup


void loop(void)
{
 // call sensors.requestTemperatures() to issue a global temperature
 // request to all devices on the bus
 Serial.print("Requesting temperatures...");
 sensors.requestTemperatures();
 tempC = sensors.getTempC(insideThermometer);
 
 stepController(recipe[currentRecipe][currentStep]);  
 inputController();
 screenController(); 
 
 checkAlarm(insideThermometer);
} // End Loop

/////////////////////////////////////////////////////////////////////////////

void startStep(byte Step) {
  if (systemState == 2) return; // Don't start a step if system state is finished
  
  if (recipe[currentRecipe][Step].recipe == 0){ //Check if Recipe is finished and set system state to finished
    systemState = 2;
    Serial.println("Recipe 0 found, system state changed to Finished");
    return;
  }
  
  Serial.print("Starting Step:");
  Serial.println(Step);
  currentStep = Step;
  if (Step > 0) {
    for (int i = 0; i < Step; i++) {
      recipe[currentRecipe][i].stepStatus = 3;
      recipe[currentRecipe][i].startTime = 0;
    }
  }
  recipe[currentRecipe][currentStep].stepStatus = 1;
  recipe[currentRecipe][currentStep].startTime = millis();
} // End startStep


//------------ Controller Functions ------------------
void stepController(recipeStep Step){
  byte runTime; 

  if(systemState==2 or systemState == 0) return; //Recipe is done or not started no need to process steps further
  
  runTime = (millis() - Step.startTime) / minute;

  // Check if end is not reached
  if(Step.duration == 0) // Means step finishes when temprature is reached. 
  {
    if(tempC < Step.targetTemp) return; // Temprature not reached
  } else // Means step ends when time is reached 
  {
    if(runTime < Step.duration) return; // Time is not up
  }

  //When coming by here, the end is near
  if (Step.autoContinue) {
    startStep(currentStep +1);
    return;
  }
  
  //When coming here, just wait for Select.
 /* Serial.print("Step finished:");
  Serial.println(currentStep);
  lcd.clear();
  lcd.print("Step ");
  lcd.print(currentStep);
  lcd.print(" is done");
  lcd.setCursor(0,1);
  lcd.print("Press Select");*/
  lcd.clear();
  waitAtStepEnd();
  while(read_LCD_buttons() != btnSELECT){
    Serial.println("Waiting for Confirmation");
  }
  lcd.clear();
  startStep(currentStep +1);
}


void inputController() {
  
  lcd_key = read_LCD_buttons();  // read the buttons
  Serial.print("Button Pressed: ");
  Serial.println(lcd_key);
  switch(lcd_key){
    case btnRIGHT:  Serial.println("RIGHT");  break;
    case btnUP:     Serial.println("UP");     break; 
    case btnDOWN:   Serial.println("DOWN");   break; 
    case btnLEFT:   Serial.println("LEFT");   break; 
    case btnSELECT: Serial.println("SELECT"); break; 
    case btnNONE:   Serial.println("NONE");   break; 
  }
  
  if ((millis()-lastInput) > buttonDelay) { // Only accept buttons every buttonDelay, to prevent double inputs
    switch (systemState) {
      case 0: // Recipee and Step selections
        if (lcd_key == btnUP)    {currentRecipe = currentRecipe + 1; lastInput = millis();}
        if (lcd_key == btnDOWN and currentRecipe != 0)  {currentRecipe = currentRecipe - 1; lastInput = millis();}
        if (lcd_key == btnLEFT and currentStep != 0)  {currentStep = currentStep - 1; lastInput = millis();}
        if (lcd_key == btnRIGHT) {currentStep = currentStep + 1; lastInput = millis();}
        if (lcd_key == btnSELECT)  {   //Start the processing
          startStep(currentStep);      //Start at the selected step
          systemState = 1;             //Chaneg system state to running
          lastInput = millis();
          lcd.clear();
        } 
        break;
      case 1:  break;
      case 2:  break;
      default: break;
    }
  }
} //End inputController

void screenController() {

  // Clear LCD is a different Screen is needed, instead of doing it in th screeen, which causes flickering
  if (lastSystemState != systemState){
    lcd.clear();
  }
  lastSystemState = systemState;
  
  switch (systemState) {
    case 0:
      initScreen();
      break;
    case 1:
      displayStatusScreen(insideThermometer, recipe[currentRecipe][currentStep]);
      break;
    case 2:
      finishScreen();
      break;
    default: 
      break;
  }
} // End screenController

//------------ Screens ------------------
void waitAtStepEnd(){
//When coming here, just wait for Select.
  Serial.println(recipe[currentRecipe][currentStep].message);
  lcd.print(recipe[currentRecipe][currentStep].message);
  lcd.setCursor(0,1);
  lcd.print("Press Select");
}

void finishScreen(){
  lcd.setCursor(0,0);
  lcd.print("Recipe: ");
  lcd.print(currentRecipe);
  lcd.setCursor(0, 1);
  lcd.print("Finished");
  Serial.println("Recipe Finished");
}

void initScreen() {
  //Select Recipe
 // lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Select Recipe:");
  lcd.setCursor(14, 0);
  if (currentRecipe < 10) lcd.print(0);
  lcd.print(currentRecipe);
  lcd.setCursor(0, 1);
  lcd.print("Select Step  :");
  lcd.setCursor(14, 1);
  if (currentStep < 10) lcd.print(0);
  lcd.print(currentStep);
} //End initScreen

void displayStatusScreen(DeviceAddress deviceAddress, recipeStep Step) {
 // lcd.clear();
  
  // Current Temprature
  tempC = sensors.getTempC(deviceAddress);
  lcd.setCursor(0, 0);
  if (tempC > 0) {
    lcd.print(tempC);
  }
  else {
    lcd.print("No Temp");
  }

  // Target Temprature
  lcd.setCursor(0, 1);
  lcd.print(Step.targetTemp);

  // Step
  lcd.setCursor(6, 1);
  lcd.print("S");
  lcd.print(Step.recipeStep);

  // Auto Continue
  lcd.setCursor(9, 1);
  lcd.print("A:");
  if (Step.autoContinue) {
    lcd.print("Y");
  }
  else {
    lcd.print("N");
  }

  // Heater State
  lcd.setCursor(13, 1);
  lcd.print("H:");
  if (heatingState) {
    lcd.print("Y");
  }
  else {
    lcd.print("N");
  }

  //Time since Start
  lcd.setCursor(8, 0);
  //Seconds
  if ((millis() - Step.startTime) / minute < 10) {
    lcd.print("0");
  }
  lcd.print((millis() - Step.startTime) / minute); //Print Minutes
  Serial.print("Time ");
  Serial.print((millis() - Step.startTime) / minute);
  lcd.print(":");

  //Seconds
  if (((millis() - Step.startTime) / second) % 60 < 10) {
    lcd.print("0");
  }
  lcd.print(((millis() - Step.startTime) / second) % 60);
  lcd.print(" ");
  Serial.print(":");
  Serial.print(((millis() - Step.startTime) / second) % 60);
  
  if (Step.duration < 10) lcd.print("0");
  lcd.print(Step.duration);
} //End displayStatusScree

void bootScreen(){
  lcd.setCursor(0,0);
  lcd.print("ArduMash 0.1");
}

//------------ Button Functions ------------------
// read the buttons
int read_LCD_buttons()
{
  int adc_key_in  = 0;
  adc_key_in = analogRead(0);      // read the value from the sensor
  // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
  // we add approx 50 to those values and check to see if we are close
  if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
  if (adc_key_in < 50)   return btnRIGHT;
  if (adc_key_in < 250)  return btnUP;
  if (adc_key_in < 450)  return btnDOWN;
  if (adc_key_in < 650)  return btnLEFT;
  if (adc_key_in < 850)  return btnSELECT;
  return btnNONE;  // when all others fail, return this...
} //end read_LCD_buttons


//------------ Temprature Functions --------------
void setupTemprature() {
  // Start up the library
  sensors.begin();

  // locate devices on the bus
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // search for devices on the bus and assign based on an index.
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");

  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();

  Serial.println("Setting alarm temps...");

  // alarm when temp is higher than 30C
  sensors.setHighAlarmTemp(insideThermometer, 78);

  // alarm when temp is lower than -10C
  sensors.setLowAlarmTemp(insideThermometer, 77.5 );

  Serial.print("New Device 0 Alarms: ");
  printAlarms(insideThermometer);
  Serial.println();
} //end setupTemprature

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}//end printAddress

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
//  tempC = sensors.getTempC(deviceAddress);
  Serial.print("Current Temp. C:");
  Serial.println(tempC);
} //end printTemperature


void printAlarms(uint8_t deviceAddress[])
{
  char temp;
  temp = sensors.getHighAlarmTemp(deviceAddress);
  Serial.print("High Alarm: ");
  Serial.print(temp, DEC);
  Serial.print("C/");
  Serial.print("Low Alarm: ");
  temp = sensors.getLowAlarmTemp(deviceAddress);
  Serial.print(temp, DEC);
  Serial.println("C/");
} //end printAlarms

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
  Serial.print("Device Address: ");
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
} //end printData

void checkAlarm(DeviceAddress deviceAddress)
{
  if (sensors.hasAlarm(deviceAddress))
  {
    Serial.print("ALARM: ");
    printTemperature(deviceAddress);
    Serial.println();
    if (sensors.getTempC(deviceAddress) < sensors.getLowAlarmTemp(deviceAddress))
    {
      digitalWrite(RELAY_PIN, HIGH);
      heatingState = true;
      Serial.println("Heat turned on");
    }
    else if (sensors.getTempC(deviceAddress) > sensors.getHighAlarmTemp(deviceAddress))
    {
      digitalWrite(RELAY_PIN, LOW);
      heatingState = false;
      Serial.println("Heat turned off");
    }
  }
} // end checkAlarm


