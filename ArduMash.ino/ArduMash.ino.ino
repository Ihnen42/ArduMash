#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

// Temprature Probe Settings
//#define TEMPRATURE true  // currently not used
#define ONE_WIRE_BUS 2   // Pin the Temprature probe is attached to 
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.
DeviceAddress insideThermometer, outsideThermometer; // arrays to hold device addresses

// Display Setting
//#define DISPLAY_PRESENT true
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

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

// Relay Switch Settings
//#define RELAY true
#define RELAY_PIN 3 // Pin the relay is attached to 

// Recipe Structure
struct recipeStep {
  byte          recipe;       // currently not used, 0 is reserved, and should not be used.
  byte          recipeStep;   // currently not used
  float         targetTemp;   // Temprature that the system trys to hold, cool down is currently not possible
  unsigned long duration;     // Duration of the step, if 0 the step ends when Temprature is reached
  bool          autoContinue; // User input needed, to go to the next step
  unsigned long startTime;    // filled furing execution when step is started
  byte          stepStatus;   // 0 = Initial, 1=Running, 2= Interrupted, 3=Finished.
  char          message[17];  // To be shown when waiting for the confirmation
};  
recipeStep recipe[1][5];  //Shoule be one Step larger then actual used steps

// ------- States -----------
// Heating State
bool  heatingState = false;
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
byte lastSystemState = -1;

void setup(void)
{
  // start serial port
  Serial.begin(9600);
  Serial.println();
  Serial.println("ArduMash 0.1.1");


  // Recipe can't be zero
  // Recipe, Step, Temp, Duration, AutoContinue, StartTime, Status
  recipe[0][0]  = (recipeStep){1, 0,  72.0,  0, false, 0, 0,"Einmaischen"}; // Signal zum Einmaischen, Malz zugabe bestätigen  
  recipe[0][0]  = (recipeStep){1, 1,  68.0, 60, true,  0, 0,""}; // Maishen
  recipe[0][1]  = (recipeStep){1, 2,  72.0,  0, false, 0, 0,"Jod Test"}; // Verzuckerungsrast, Nach Jod Test bestätigen
 // recipe[0][2]  = (recipeStep){1, 3,  78.0,  0, true,  0, 0,""}; // Enzymdeaktivierung.
 // recipe[0][3]  = (recipeStep){1, 4,   0.0,  0, false, 0, 0,"Läutern"}; // Läutern, Nach dem Läutern Bestätigen
 // recipe[0][5]  = (recipeStep){1, 5, 100.0,  0, false, 0, 0,"1. Hopfen gabe"}; // Kochen, signal für die erste Hopfengabe bestätigen
 // recipe[0][6]  = (recipeStep){1, 6, 100.0, 30, true,  0, 0,"2. Hopfen gabe"}; // Kochen, signal für die zweit Hopfengabe
 // recipe[0][7]  = (recipeStep){1, 7, 100.0, 30, true,  0, 0,""}; // Kochen, signal Whirpool und umfüllen
 // recipe[0][8]  = (recipeStep){1, 8,   0.0,  0, false, 0, 0,"Kochen Beendet"}; // Ende 

/*
 // Recipe, Step, Temp, Duration, AutoContinue, StartTime, Status
  recipe[0][0]  = (recipeStep){1, 0,  72.0,  1, false, 0, 0,"Mash in"}; // Signal zum Einmaischen, Malz zugabe bestätigen  
  recipe[0][1]  = (recipeStep){1, 1,  68.0,  1, true,  0, 0,""}; // Maishen
  recipe[0][2]  = (recipeStep){1, 2,  72.0,  1, false, 0, 0,"1. add hops"}; // Verzuckerungsrast, Nach Jod Test bestätigen
*/
// Setup the temprature probe  
  Serial.println();
  Serial.println("Starting Temprature Probe Steup");
  setupTemprature();
  Serial.println("Temprature Probe Steup DONE");
  
// set the digital pin as output:
  Serial.println();
  Serial.println("Starting Relay Steup");
  pinMode(RELAY_PIN, OUTPUT);
  Serial.print("Relay set to pin:");
  Serial.println(RELAY_PIN);
  Serial.println("Relay Steup DONE");

// set up the LCD's number of columns and rows:
  Serial.println();
  Serial.println("Starting Display Steup");
  lcd.begin(16, 2);
  lcd.clear();
  Serial.println("Display Setup DONE");
  bootScreen();
  delay(3000);
  Serial.println("SETUP COMPLETE");
  Serial.println();

} // End Setup


void loop(void)
{
// Serial.print("Requesting temperatures...");
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
  Serial.print(Step);
  Serial.print("; Target Temp:");
  Serial.print(recipe[currentRecipe][Step].targetTemp);
  Serial.print("; Duration:");
  Serial.print(recipe[currentRecipe][Step].duration);
  Serial.print("; AutoContinue:");
  Serial.println(recipe[currentRecipe][Step].autoContinue);
  
  currentStep = Step;
  if (Step > 0) {
    for (int i = 0; i < Step; i++) {
      recipe[currentRecipe][i].stepStatus = 3;
      recipe[currentRecipe][i].startTime = 0;
    }
  }
  recipe[currentRecipe][currentStep].stepStatus = 1;
  recipe[currentRecipe][currentStep].startTime = millis();

  Serial.println("Setting alarm temps...");

  // alarm when temp is higher than 30C
  //sensors.setHighAlarmTemp(insideThermometer, recipe[currentRecipe][Step].targetTemp);

  // alarm when temp is lower than -10C4
  //sensors.setLowAlarmTemp(insideThermometer, recipe[currentRecipe][Step].targetTemp-0.5 );

  Serial.print("New Device 0 Alarms: ");
  //printAlarms(insideThermometer);
  
} // End startStep


//------------ Controller Functions ------------------
void stepController(recipeStep Step){
  byte runTime; 

  if(systemState==2 or systemState == 0) return; //Recipe is done or not started no need to process steps further
  
  runTime = (millis() - Step.startTime) / 60000;

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
  Serial.println("Waiting for Confirmation");
  while(read_LCD_buttons() != btnSELECT){
    
  }
  lcd.clear();
  startStep(currentStep +1);
}


void inputController() {
  
  lcd_key = read_LCD_buttons();  // read the buttons

  switch(lcd_key){
    case btnRIGHT:  Serial.print("Button Pressed: "); Serial.println("RIGHT");  break;
    case btnUP:     Serial.print("Button Pressed: "); Serial.println("UP");     break; 
    case btnDOWN:   Serial.print("Button Pressed: "); Serial.println("DOWN");   break; 
    case btnLEFT:   Serial.print("Button Pressed: "); Serial.println("LEFT");   break; 
    case btnSELECT: Serial.print("Button Pressed: "); Serial.println("SELECT"); break; 
    case btnNONE:   break; 
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
  
  switch (systemState) {
    case 0:
      if (lastSystemState != systemState){
        Serial.println ("Recipe Selection Screen");
      }
      initScreen();
      break;
    case 1:
      displayStatusScreen(insideThermometer, recipe[currentRecipe][currentStep]);
      break;
    case 2:
      finishScreen();
      if (lastSystemState != systemState){
        Serial.println ("Recipe Finished");
      }
      break;
    default: 
      break;
  }
  lastSystemState = systemState;
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
}

void initScreen() {
  //Select Recipe 
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
  if ((millis() - Step.startTime) / 60000 < 10) {
    lcd.print("0");
  }
  lcd.print((millis() - Step.startTime) / 60000); //Print Minutes
//  Serial.print("Time: ");
//  Serial.print((millis() - Step.startTime) / 60000);
  lcd.print(":");

  //Seconds
  if (((millis() - Step.startTime) / 1000) % 60 < 10) {
    lcd.print("0");
  }
  lcd.print(((millis() - Step.startTime) / 1000) % 60);
  lcd.print(" ");
//  Serial.print(":");
//  Serial.print(((millis() - Step.startTime) / 1000) % 60);
  
  if (Step.duration < 10) lcd.print("0");
  lcd.print(Step.duration);
} //End displayStatusScree

void bootScreen(){
  lcd.setCursor(0,0);
  lcd.print("ArduMash 0.1.1");
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
  sensors.setHighAlarmTemp(insideThermometer, 68);

  // alarm when temp is lower than -10C
  sensors.setLowAlarmTemp(insideThermometer, 67.5 );

  Serial.print("New Device 0 Alarms: ");
  printAlarms(insideThermometer);
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
  Serial.print("C ");
  Serial.print("Low Alarm: ");
  temp = sensors.getLowAlarmTemp(deviceAddress);
  Serial.print(temp, DEC);
  Serial.println("C");
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


