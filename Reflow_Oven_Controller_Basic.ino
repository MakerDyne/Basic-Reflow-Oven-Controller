#include "Adafruit_MAX31855.h"
#include "Reflow_Parameters.h"

// DEFINE PINS
// Thermocouple breakout
int thermoDO = 3;
int thermoCS = 4;
int thermoCLK = 5;

// LEDs
int reflowInProgressLED = 10;
int ovenHeatingLED = 9;
int ovenTooCold = 8;
int ovenJustRight = 7;
int ovenTooHot = 6;

#define OVEN_TOO_COLD    1  // DO NOT
#define OVEN_JUST_RIGHT  2  // ALTER
#define OVEN_TOO_HOT     3  // THESE
#define OVEN_OFF         0  // VALUES

// Switches
int startSwitch = 12;
int stopSwitch = 11;
boolean startSwitchAlreadyPressed = false;
boolean stopSwitchAlreadyPressed = false;

// Powerswitch Tail
int ovenControl = 2;

// Struct to hold the reflow stage parameters.
typedef struct {//USER DEFINED VARIABLES (set according to the user-configurable #defines in Reflow_Parameters.h)
                double targetTemp;
                double tempError;
                double duration;
                double controlInterval;
                // INTERNAL CONTROL VARIABLES (set internally, no user decision of their values is required)
                double elapsedTime;  // relative to the current stage - not the whole process, in millis
                double startingTime; // in millis
                double currentTemp;
                double startingTemp;
              } ReflowStage;
              
// Array to hold the individual reflow stages together as a complete reflow profile
ReflowStage ReflowProfile[5];

unsigned int currentReflowStage = 5; // >= 5 for oven idle)
int currentOvenTemp = 0;
unsigned long processStartTime = 0;
unsigned long stageStartTime = 0;

unsigned long lastSerialLoggingTime = 0;
unsigned long lastControlTime = 0;

// Thermocouple object
Adafruit_MAX31855 thermocouple(thermoCLK, thermoCS, thermoDO);


void setup() { 
  // SETUP PINS
  // LEDs
  pinMode(reflowInProgressLED, OUTPUT);
  digitalWrite(reflowInProgressLED, LOW);
  pinMode(ovenHeatingLED, OUTPUT);
  digitalWrite(ovenHeatingLED, LOW);
  pinMode(ovenTooCold, OUTPUT);
  digitalWrite(ovenTooCold, LOW);
  pinMode(ovenJustRight, OUTPUT);
  digitalWrite(ovenJustRight, LOW);
  pinMode(ovenTooHot, OUTPUT);
  digitalWrite(ovenTooHot, LOW);
  
  // Switches
  // Switches are normally open type and use internal MCU pullup resistors, so when switch is PRESSED, digitalRead is LOW
  pinMode(startSwitch, INPUT_PULLUP);
  pinMode(stopSwitch, INPUT_PULLUP);
  
  // Powerswitch Tail (oven is OFF when output is HIGH)
  pinMode(ovenControl, OUTPUT);
  ovenOn(false);
  
  // SETUP SERIAL CONNECTION
  Serial.begin(9600);
  
  // Read reflow profile data into array
  initialiseReflowData();
  
  // wait for thermocouple breakout IC to stabilize
  delay(1000);
  
  // WELCOME MESSAGE
  Serial.println("*** Basic Reflow Oven Controller ***");
  Serial.println("Reflow Oven Controller ready for use\n");
}


void loop() {
  // START SWTITCH PRESSED
  if((digitalRead(startSwitch) == LOW) && (digitalRead(stopSwitch) == HIGH))
  {
    if(!startSwitchAlreadyPressed) {      
      // First must check starting temp (ambient) isn't too high (<30?). serial out an error if it is
      double testTemp = thermocouple.readCelsius();
      if(testTemp > MAX_STARTING_TEMP) {
        Serial.print("Cannot start reflow process. The oven temperature is too high (");
        Serial.print(testTemp);
        Serial.println(" Celsius).");
        Serial.print("Wait for the oven to cool to below ");
        Serial.print(MAX_STARTING_TEMP);
        Serial.println(" Celsius before starting the reflow process.\n");
        delay(1000);
      } else {
        // Print warning message
        Serial.println("\nWARNING: NEVER LEAVE OVEN UNATTENDED WHEN IN USE\n");
        // Print column headings for data reporting over serial
        Serial.println(
                       "Time(s), "
                       "ActualTemp(C), "
                       "ExpectedTemp(C), "
                       "ExpectedTempLowerBound(C), "
                       "ExpectedTempUpperBound(C), "
                       "OvenState(1/0), "
                       "ReflowStage(0-4)"
                       );
        // initialise ReflowProfile data
        initialiseReflowData(); 
        // seed ReflowProfile with starting values
        currentOvenTemp = testTemp;
        ReflowProfile[0].startingTime = millis();
        ReflowProfile[0].elapsedTime = 0;
        ReflowProfile[0].startingTemp = testTemp;
        ReflowProfile[0].currentTemp = testTemp;
        
        // acknowledge that process has started
        startSwitchAlreadyPressed = true;
        stopSwitchAlreadyPressed = false;
        // set current reflow stage to 0 (it should have been >=5, to indicate idle before)
        currentReflowStage = 0;
        // set reflowStart time from millis();
        processStartTime = millis();
        // turn reflowInProgressLED to ON
        digitalWrite(reflowInProgressLED, HIGH);
      }
    }   
  }
  
  // STOP SWITCH PRESSED
  if(digitalRead(stopSwitch) == LOW) {
    if(startSwitchAlreadyPressed && !stopSwitchAlreadyPressed)
      // ABORT REFLOW PROCESS
      // Print error message
      Serial.println("Reflow Process Aborted By User!\n");
      //zero currentOvenTemp value
      currentOvenTemp = 0;
      // acknowledge process has stopped
      startSwitchAlreadyPressed = false;
      stopSwitchAlreadyPressed = true;
      // set reflow stage to idle
      currentReflowStage = 5;
      // zero reflowStart time;
      processStartTime = 0;
      // turn off all LEDs
      digitalWrite(reflowInProgressLED, LOW);
      tempStatusLEDs(OVEN_OFF);
      // turn off the oven
      ovenOn(false);  
  }

  // CONTROL THE REFLOW PROCESS
  // Control of the process happens at intervals defined for each reflow stage
  if(startSwitchAlreadyPressed) {
    //has sufficient time elapsed since the last control interval for a new control decision to be required?
    if(lastControlTime + (ReflowProfile[currentReflowStage].controlInterval * 1000) < millis()) {
      lastControlTime = millis();
      // update ReflowProfile parameters      
      ReflowProfile[currentReflowStage].elapsedTime = lastControlTime - ReflowProfile[currentReflowStage].startingTime;
      ReflowProfile[currentReflowStage].currentTemp = thermocouple.readCelsius();   
      // Is the actual temperature within the permitted range?
      // calculate stage temperature gradient in degrees per millisecond
      double tempGradient = (ReflowProfile[currentReflowStage].targetTemp - ReflowProfile[currentReflowStage].startingTemp)/(ReflowProfile[currentReflowStage].duration*1000);
      // calculate expected temperature
      double expectedTemp = (tempGradient * ReflowProfile[currentReflowStage].elapsedTime) + ReflowProfile[currentReflowStage].startingTemp;
      if(ReflowProfile[currentReflowStage].targetTemp > ReflowProfile[currentReflowStage].startingTemp) {
        if(expectedTemp > ReflowProfile[currentReflowStage].targetTemp) {
          expectedTemp = ReflowProfile[currentReflowStage].targetTemp;
        }
      } else {
        if(expectedTemp < ReflowProfile[currentReflowStage].targetTemp) {
          expectedTemp = ReflowProfile[currentReflowStage].targetTemp;
        }
      }
      // Too hot -> turn oven off
      if(ReflowProfile[currentReflowStage].currentTemp > (expectedTemp + ReflowProfile[currentReflowStage].tempError)) {
        ovenOn(false);
        tempStatusLEDs(OVEN_TOO_HOT);
      }
      // Too cold -> turn oven on
      else if(ReflowProfile[currentReflowStage].currentTemp < (expectedTemp - ReflowProfile[currentReflowStage].tempError)) {
        ovenOn(true);
        tempStatusLEDs(OVEN_TOO_COLD);
      } else {
        // oven temperature is within desired range
        tempStatusLEDs(OVEN_JUST_RIGHT);
      }
      // Have time and temperature targets been hit for the current stage?
//    if((ReflowProfile[currentReflowStage].currentTemp > ReflowProfile[currentReflowStage].targetTemp) && 
//       (ReflowProfile[currentReflowStage].elapsedTime/1000 > ReflowProfile[currentReflowStage].duration)) {
      if(ReflowProfile[currentReflowStage].elapsedTime/1000 > ReflowProfile[currentReflowStage].duration) {
        if(
          ((ReflowProfile[currentReflowStage].targetTemp >= ReflowProfile[currentReflowStage].startingTemp) && 
          (ReflowProfile[currentReflowStage].currentTemp > ReflowProfile[currentReflowStage].targetTemp))
          ||
          ((ReflowProfile[currentReflowStage].targetTemp < ReflowProfile[currentReflowStage].startingTemp) && 
          (ReflowProfile[currentReflowStage].currentTemp < ReflowProfile[currentReflowStage].targetTemp))
          ) {
         // seed next stage with starting values and increment the stage number
          if(currentReflowStage < 4) {
            ReflowProfile[currentReflowStage + 1].startingTime = millis();
            ReflowProfile[currentReflowStage + 1].elapsedTime = 0;
            ReflowProfile[currentReflowStage + 1].startingTemp = ReflowProfile[currentReflowStage].currentTemp;
            ReflowProfile[currentReflowStage + 1].currentTemp = ReflowProfile[currentReflowStage].currentTemp;
          }
          currentReflowStage++;
          // handle reaching the end of the reflow process
          if(currentReflowStage >= 5) {
            int finalTemp = thermocouple.readCelsius();
            Serial.print("\nReflow process complete. The oven temperature is ");
            Serial.print(finalTemp);
            Serial.println(" Celsius.");
            Serial.println("It is now safe to remove the PCBs from the oven\n");
            stopSwitchAlreadyPressed = true;
            startSwitchAlreadyPressed = false;
            tempStatusLEDs(OVEN_OFF);
            digitalWrite(reflowInProgressLED, LOW);
            ovenOn(false);
          }
        }
      }
    }
    
    // SERIAL REPORTING OF REFLOW PROCESS
    // Reporting of the oven state happens every second regardless of the current stage's control interval.
    if(((lastSerialLoggingTime + 1000) < millis()) && currentReflowStage < 5) {
      lastSerialLoggingTime = millis();
      unsigned long timeInSeconds = (lastSerialLoggingTime - processStartTime)/1000;
      double tempGradient = (ReflowProfile[currentReflowStage].targetTemp - ReflowProfile[currentReflowStage].startingTemp)/(ReflowProfile[currentReflowStage].duration*1000);
      // calculate expected temperature
      double expectedTemp = (tempGradient * (lastSerialLoggingTime - ReflowProfile[currentReflowStage].startingTime)) + ReflowProfile[currentReflowStage].startingTemp;
      if(ReflowProfile[currentReflowStage].targetTemp > ReflowProfile[currentReflowStage].startingTemp) {
        if(expectedTemp > ReflowProfile[currentReflowStage].targetTemp) {
          expectedTemp = ReflowProfile[currentReflowStage].targetTemp;
        }
      } else {
        if(expectedTemp < ReflowProfile[currentReflowStage].targetTemp) {
          expectedTemp = ReflowProfile[currentReflowStage].targetTemp;
        }
      }
      // Need to report, in the following order:
      // Time(s), ActualTemp(C), ExpectedTemp(C), ExpectedTempLowerBound(C), ExpectedTempUpperBound(C), OvenState(1/0), ReflowStage(0-4)
      Serial.print(timeInSeconds);
      Serial.print(",\t");
      Serial.print(thermocouple.readCelsius());
      Serial.print(",\t");
      Serial.print(expectedTemp);
      Serial.print(",\t");
      Serial.print(expectedTemp - ReflowProfile[currentReflowStage].tempError);
      Serial.print(",\t");
      Serial.print(expectedTemp + ReflowProfile[currentReflowStage].tempError);
      Serial.print(",\t");
      if(digitalRead(ovenControl) == LOW) {
        Serial.print("1");
      } else {
        Serial.print("0");
      }
      Serial.print(",\t");
      switch(currentReflowStage) {
        case 0:
          Serial.println("Phase 1: Ramp To Soak");
          break;
        case 1:
          Serial.println("Phase 2: Soak");
          break;
        case 2:
          Serial.println("Phase 3: Ramp To Reflow");
          break;  
        case 3:
          Serial.println("Phase 4: Reflow");
          break;
        case 4:
          Serial.println("Phase 5: Cooling");
          break;
        default:
          Serial.print("Unknown Phase Number: ");
          Serial.println(currentReflowStage);
          break;
      }
    }
  }  
}


// Combine Powerswitch Tail and ovenLED control into a single convenience function 
void ovenOn(boolean power) {
  if(power) {
    digitalWrite(ovenControl, LOW);
    digitalWrite(ovenHeatingLED, HIGH);
  } else {
    digitalWrite(ovenControl, HIGH);
    digitalWrite(ovenHeatingLED, LOW);
  }
}


void tempStatusLEDs(unsigned int X) {
  switch(X) {
    case 1: // oven too cold
      digitalWrite(ovenTooCold, HIGH);
      digitalWrite(ovenJustRight, LOW);
      digitalWrite(ovenTooHot, LOW);
      break;
    case 2: // oven just right
      digitalWrite(ovenTooCold, LOW);
      digitalWrite(ovenJustRight, HIGH);
      digitalWrite(ovenTooHot, LOW);
      break;
    case 3:
      digitalWrite(ovenTooCold, LOW);
      digitalWrite(ovenJustRight, LOW);
      digitalWrite(ovenTooHot, HIGH);
      break;
    default:
      digitalWrite(ovenTooCold, LOW);
      digitalWrite(ovenJustRight, LOW);
      digitalWrite(ovenTooHot, LOW);
      break;
  }
}  
      

void initialiseReflowData() {
  ReflowProfile[0].targetTemp = RAMP_TO_SOAK_TARGET_TEMP;
  ReflowProfile[0].tempError = RAMP_TO_SOAK_TEMP_ERROR;
  ReflowProfile[0].duration = RAMP_TO_SOAK_DURATION;
  ReflowProfile[0].controlInterval = RAMP_TO_SOAK_CONTROL_INTERVAL;
  ReflowProfile[0].elapsedTime = 0;
  ReflowProfile[0].startingTime = 0;
  ReflowProfile[0].currentTemp = 0;
  ReflowProfile[0].startingTemp = 0;
  
  ReflowProfile[1].targetTemp = SOAK_TARGET_TEMP;
  ReflowProfile[1].tempError = SOAK_TEMP_ERROR;
  ReflowProfile[1].duration = SOAK_DURATION;
  ReflowProfile[1].controlInterval = SOAK_CONTROL_INTERVAL;
  ReflowProfile[1].elapsedTime = 0;
  ReflowProfile[1].startingTime = 0;
  ReflowProfile[1].currentTemp = 0;
  ReflowProfile[1].startingTemp = 0;
  
  ReflowProfile[2].targetTemp = RAMP_TO_REFLOW_TARGET_TEMP;
  ReflowProfile[2].tempError = RAMP_TO_REFLOW_TEMP_ERROR;
  ReflowProfile[2].duration = RAMP_TO_REFLOW_DURATION;
  ReflowProfile[2].controlInterval = RAMP_TO_REFLOW_CONTROL_INTERVAL;
  ReflowProfile[2].elapsedTime = 0;
  ReflowProfile[2].startingTime = 0;
  ReflowProfile[2].currentTemp = 0;  
  ReflowProfile[2].startingTemp = 0;
  
  ReflowProfile[3].targetTemp = REFLOW_TARGET_TEMP;
  ReflowProfile[3].tempError = REFLOW_TEMP_ERROR;
  ReflowProfile[3].duration = REFLOW_DURATION;
  ReflowProfile[3].controlInterval = REFLOW_CONTROL_INTERVAL;
  ReflowProfile[3].elapsedTime = 0;
  ReflowProfile[3].startingTime = 0;
  ReflowProfile[3].currentTemp = 0;
  ReflowProfile[3].startingTemp = 0;
  
  ReflowProfile[4].targetTemp = COOLING_TARGET_TEMP;
  ReflowProfile[4].tempError = COOLING_TEMP_ERROR;
  ReflowProfile[4].duration = COOLING_DURATION;
  ReflowProfile[4].controlInterval = COOLING_CONTROL_INTERVAL;
  ReflowProfile[4].elapsedTime = 0;
  ReflowProfile[4].startingTime = 0;
  ReflowProfile[4].currentTemp = 0;
  ReflowProfile[4].startingTemp = 0;
}
