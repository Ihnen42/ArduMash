#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <DB.h>

// Temprature Probe Settings
#define ONE_WIRE_BUS 2   // Pin the Temprature probe is attached to 
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.
DeviceAddress insideThermometer; // arrays to hold device addresses

// Display Setting
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Button Values
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
#define buttonDelay 350
unsigned long lastInput;

// Relay Switch Settings
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
  byte          messageNo;  // To be shown when waiting for the confirmation
} recStep;  
recipeStep recipe[1][10];  //Shoule be one Step larger then actual used steps

// ------- States -----------
// Heating State
bool  heatingState = false;
float tempC;

// Recipe State
byte currentStep = 0;
byte currentRecipe = 0;

//System State
#define ST_INIT    0 //Initial Nothing Started
#define ST_RUNNING 1 // Running
#define ST_FINISH  2 // Finished
#define ST_WAIT    3 // Wait at step end
byte systemState = ST_INIT;
byte lastSystemState = -1;

// DB Settings
DB db;
#define MY_TBL 1

void setup(void)
{
  // start serial port
  Serial.begin(9600);
  Serial.println();
  Serial.println(F("ArduMash 0.1.3"));

  Serial.print(F("Creating Table..."));
  db.create(MY_TBL,sizeof(recipeStep));
  db.open(MY_TBL);
  Serial.println(F("DONE"));

  // Recipe zero is reserved for system function
  // Recipe, Step, Temp, Duration, AutoContinue, StartTime, Status
  recipe[0][0]  = (recipeStep){1, 0,  72.0,  0, false, 0, 0, 7}; // Signal zum Einmaischen, Malz zugabe bestätigen  
  recipe[0][1]  = (recipeStep){1, 1,  68.0,  60, true,  0, 0, 0}; // Maishen
  recipe[0][2]  = (recipeStep){1, 2,  72.0,  0, false, 0, 0, 1}; // Verzuckerungsrast, Nach Jod Test bestätigen
  recipe[0][3]  = (recipeStep){1, 3,  78.0,  0, true,  0, 0, 0}; // Enzymdeaktivierung.
  recipe[0][4]  = (recipeStep){1, 4,   0.0,  0, false, 0, 0, 2}; // Läutern, Nach dem Läutern Bestätigen
  recipe[0][5]  = (recipeStep){1, 5, 100.0,  0, false, 0, 0, 3}; // Kochen, signal für die erste Hopfengabe bestätigen
  recipe[0][6]  = (recipeStep){1, 6, 100.0, 30, true,  0, 0, 4}; // Kochen, signal für die zweit Hopfengabe
  recipe[0][7]  = (recipeStep){1, 7, 100.0, 30, true,  0, 0, 0}; // Kochen, signal Whirpool und umfüllen
  recipe[0][8]  = (recipeStep){1, 8,   0.0,  0, false, 0, 0, 6}; // Ende 
/*
 // Recipe, Step, Temp, Duration, AutoContinue, StartTime, Status
  recipe[0][0]  = (recipeStep){1, 0,  72.0,  1, false, 0, 0, 7}; // Signal zum Einmaischen, Malz zugabe bestätigen  
  recipe[0][1]  = (recipeStep){1, 1,  68.0,  1, true,  0, 0, 0}; // Maishen
  recipe[0][2]  = (recipeStep){1, 2,  72.0,  1, false, 0, 0, 3}; // Verzuckerungsrast, Nach Jod Test bestätigen
*/
// Setup the temprature probe  
  Serial.println();
  Serial.println(F("Starting Temprature Probe Steup"));
  sensors.begin();

// locate devices on the bus
  Serial.print(F("Found "));
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(F(" devices."));

// search for devices on the bus and assign based on an index.
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println(F("Unable to find address for Device 0"));
  
  Serial.println(F("Temprature Probe Steup DONE"));
  
// set the relay digital pin as output:
  Serial.println();
  Serial.println(F("Starting Relay Steup"));
  pinMode(RELAY_PIN, OUTPUT);
  Serial.print(F("Relay set to pin:"));
  Serial.println(RELAY_PIN);
  Serial.println(F("Relay Steup DONE"));

// set up the LCD's number of columns and rows:
  Serial.println();
  Serial.println(F("Starting Display Steup"));
  lcd.begin(16, 2);
  lcd.clear();
  Serial.println(F("Display Setup DONE"));
  bootScreen(); //Bullshit screen with no function
  delay(3000);  //Everybody needs time to read the screen 
  Serial.println(F("SETUP COMPLETE"));
  Serial.println();

} // End Setup


void loop(void)
{
 sensors.requestTemperatures(); //Get a new temprature reading from all sensors
 tempC = sensors.getTempC(insideThermometer); // Update Mash Temprature
// run through all controller
 stepController(recipe[currentRecipe][currentStep]);
 heaterController(recipe[currentRecipe][currentStep]);
 inputController();
 screenController(); 
} // End Loop

/////////////////////////////////////////////////////////////////////////////
//------------ Controller Functions ------------------

void heaterController(recipeStep Step){
  // control the heater
  if(Step.targetTemp < tempC ) // turn heater off
  {
    heatingState = false;
  }
  else if (Step.targetTemp > (tempC + 0.5) )
  {
    heatingState = true;
  }
  switch (systemState) {
    //case ST_INIT:  // Commented out to turn on heat control during init phase.
    //  break;
    case ST_FINISH:
      heatingState = false;
      digitalWrite(RELAY_PIN, LOW);
      break;
    default: 
      if(heatingState) {
        digitalWrite(RELAY_PIN, HIGH);
      }
      else {
        digitalWrite(RELAY_PIN, LOW);
      }
      break;
  }
}

void stepController(recipeStep Step){
  byte runTime; 

  if(systemState==ST_FINISH or systemState == ST_INIT) return; //Recipe is done or not started no need to process steps further
  
  runTime = (millis() - Step.startTime) / 60000; // Get Runtime in Minutes

  // Check if end is not reached
  if(Step.duration == 0) // Means step finishes when temprature is reached. 
  {
    if(tempC < Step.targetTemp) return; // Temprature not reached, return
  } else // Means step ends when time is reached 
  {
    if(runTime < Step.duration) return; // Time is not up, return
  }

  //When coming by here, the end is near
  if (Step.autoContinue) {
    startStep(currentStep +1);
    return;
  }

  //When coming here, just wait.
  systemState = ST_WAIT;
  
}

void inputController() {
  byte lcd_key     = 0;
  lcd_key = read_LCD_buttons();  // read the buttons

  if ((millis()-lastInput) > buttonDelay) { // Only accept buttons every buttonDelay, to prevent double inputs
    switch (systemState) {
      case ST_INIT: // Recipee and Step selections
        if (lcd_key == btnUP)                          {currentRecipe = currentRecipe + 1; lastInput = millis();}
        if (lcd_key == btnDOWN and currentRecipe != 0) {currentRecipe = currentRecipe - 1; lastInput = millis();}
        if (lcd_key == btnLEFT and currentStep != 0)   {currentStep   = currentStep   - 1; lastInput = millis();}
        if (lcd_key == btnRIGHT)                       {currentStep   = currentStep   + 1; lastInput = millis();}
        if (lcd_key == btnSELECT)  {   //Start the processing
          startStep(currentStep);      //Start at the selected step
          systemState = ST_RUNNING;    //Change system state to running
          lastInput = millis();
          lcd.clear();
        } 
        break;
      case ST_RUNNING:  break;
      case ST_FINISH:  break;
      case ST_WAIT:  
        if (lcd_key == btnSELECT)  {   //Start the processing
          startStep(currentStep +1);
          systemState = ST_RUNNING; //running
          break;
        } 
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
    case ST_INIT:
      if (lastSystemState != systemState){
        Serial.println (F("Recipe Selection Screen"));
      }
      initScreen();
      break;
    case ST_RUNNING:
      displayStatusScreen(insideThermometer, recipe[currentRecipe][currentStep]);
      break;
    case ST_FINISH:
      finishScreen();
      if (lastSystemState != systemState){
        Serial.println (F("Recipe Finished"));
      }
      break;
    case ST_WAIT:
      if (lastSystemState != systemState){
        waitAtStepEnd();
      }
      break;
    default: 
      break;
  }
  lastSystemState = systemState;
} // End screenController


//------------  Functions ------------------
// read the buttons
byte read_LCD_buttons()
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

// Message Class, to reduce Memory Consuption
char* getMessage(byte messageNo){
    switch (messageNo) {
    case 0: return "Einmaischen";    break;
    case 1: return "Jod Test";       break;
    case 2: return "Laeutern";        break;
    case 3: return "1. Hopfen gabe"; break;
    case 4: return "2. Hopfen gabe"; break;
    case 5: return "2. Hopfen gabe"; break;
    case 6: return "Kochen Beendet"; break;
    case 7: return "Einmaischen";    break;
    case 8: break;
    
    default: 
      break;

    }

}

void startStep(byte Step) {
  if (systemState == ST_FINISH) return; // Don't start a step if system state is finished
  
  if (recipe[currentRecipe][Step].recipe == 0){ //Check if Recipe is finished and set system state to finished
    systemState = ST_FINISH;
    Serial.println(F("Recipe 0 found, system state changed to Finished"));
    return;
  }
  
  Serial.print(F("Starting Step:"));
  Serial.print(Step);
  Serial.print(F("; Target Temp:"));
  Serial.print(recipe[currentRecipe][Step].targetTemp);
  Serial.print(F("; Duration:"));
  Serial.print(recipe[currentRecipe][Step].duration);
  Serial.print(F("; AutoContinue:"));
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
  
} // End startStep

//------------ Screens ------------------
void waitAtStepEnd(){
//When coming here, just wait for Select.
  Serial.println(getMessage(recipe[currentRecipe][currentStep].messageNo));
  lcd.print(getMessage(recipe[currentRecipe][currentStep].messageNo));
  lcd.setCursor(0,1);
  lcd.print(F("Press Select"));
}

void finishScreen(){
  lcd.setCursor(0,0);
  lcd.print(F("Recipe: "));
  lcd.print(currentRecipe);
  lcd.setCursor(0, 1);
  lcd.print(F("Finished"));
}

void initScreen() {
  //Select Recipe 
  lcd.setCursor(0,0);
  lcd.print(F("Select Recipe:"));
  lcd.setCursor(14, 0);
  if (currentRecipe < 10) lcd.print(0);
  lcd.print(currentRecipe);
  lcd.setCursor(0, 1);
  lcd.print(F("Select Step  :"));
  lcd.setCursor(14, 1);
  if (currentStep < 10) lcd.print(0);
  lcd.print(currentStep);
} //End initScreen

void displayStatusScreen(DeviceAddress deviceAddress, recipeStep Step) {
  
  // Current Temprature
  tempC = sensors.getTempC(deviceAddress);
  lcd.setCursor(0, 0);
  if (tempC > 0) {
    lcd.print(tempC);
  }
  else {
    lcd.print(F("No Temp"));
  }

  // Target Temprature
  lcd.setCursor(0, 1);
  lcd.print(Step.targetTemp);

  // Step
  lcd.setCursor(6, 1);
  lcd.print(F("S"));
  lcd.print(Step.recipeStep);

  // Auto Continue
  lcd.setCursor(9, 1);
  lcd.print(F("A:"));
  if (Step.autoContinue) {
    lcd.print(F("Y"));
  }
  else {
    lcd.print(F("N"));
  }

  // Heater State
  lcd.setCursor(13, 1);
  lcd.print(F("H:"));
  if (heatingState) {
    lcd.print(F("Y"));
  }
  else {
    lcd.print(F("N"));
  }

  //Time since Start
  lcd.setCursor(8, 0);
  if ((millis() - Step.startTime) / 60000 < 10) lcd.print(F("0"));       // add a 0 if less then 10 minutes
  lcd.print((millis() - Step.startTime) / 60000);                     // Print Minutes
  lcd.print(":");
  if (((millis() - Step.startTime) / 1000) % 60 < 10) lcd.print(F("0")); // add a 0 if less then 10 seconds
  lcd.print(((millis() - Step.startTime) / 1000) % 60);               // Print Seconds
  lcd.print(" ");

  // Step Duration
  if (Step.duration < 10) lcd.print(F("0")); // add a 0 if less then 10 minutes
  lcd.print(Step.duration);
} //End displayStatusScree

void bootScreen(){
  lcd.setCursor(0,0);
  lcd.print(F("ArduMash 0.1.2"));
}




