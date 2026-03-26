#include <HX711.h>

// Pin definitions
#define DOUT_PIN  10  // Load cell data pin
#define SCK_PIN   11  // Load cell clock pin
#define COMM_PIN_OUT 12  // Output pin to master
#define COMM_PIN_IN 13  // Input pin from master

// Pin definitions
struct LoadCellTask { 
  HX711 scale; 
  float calibration_factor = 20948.9199f;
  float ADCI = 1000000.0f;
  float W_bottle = 0.0f;
  float W_tot = 0.0f;
  float n_pills = 0.0f;
  float weight = 0.0f;
  float instWeight = 0.0f;
  float reading = 0.0f;
  float instReading = 0.0f;
  bool bottleRecorded = false;
  bool totalRecorded = false;
  float avgPillWeight = 0.2028f;
//  bool countingComplete = false;
//  bool go = true;
};

struct LoadCellTask loadCell;

void setup() {
  Serial.begin(9600);
  loadCell.scale.begin(DOUT_PIN, SCK_PIN);
  Serial.println("Pill Counter System Ready.");
  runCalibration();
  pinMode(COMM_PIN_OUT, OUTPUT);
  pinMode(COMM_PIN_IN, INPUT);
}

void loop() {
  loadCell.instReading = loadCell.scale.read_average(1); 
  loadCell.instWeight = (loadCell.instReading - loadCell.ADCI) / loadCell.calibration_factor;
  Serial.println(loadCell.instWeight);

  if (loadCell.instWeight > 1.0f && !loadCell.bottleRecorded) { 
    digitalWrite(COMM_PIN_OUT, HIGH); // Full System Code completes its conveyance turn, then checks if high. If High, cont to pill stuff. Else ignore and cont.
    // At this point, the system code will wait for 1/10 of a second for a response. Set to low if commPin doesn't change i.e. while rotating the rotary table, don't measure
    delay(400); // Wait for stabilization
    loadCell.reading = loadCell.scale.read_average(10);
    loadCell.W_bottle = (loadCell.reading - loadCell.ADCI) / loadCell.calibration_factor;
    Serial.print("Bottle Weight: ");
    Serial.println(loadCell.W_bottle);
    loadCell.bottleRecorded = true; // must only be recorded if there is a bottle (if W_bottle >= 14) 
  }
  else if (loadCell.instWeight < 1.0f && loadCell.bottleRecorded) {
    loadCell.bottleRecorded = false;
  }
  
  if (digitalRead(COMM_PIN_IN) == HIGH && loadCell.bottleRecorded == true && loadCell.W_tot == 0.0f && !loadCell.totalRecorded){ 
    // Master has requested the pill count and must wait a set time for a response. Can be done with millis()
    delay(400); // Wait for stabilization
    loadCell.W_tot = (loadCell.scale.read_average(10) - loadCell.ADCI) / loadCell.calibration_factor; // Record total weight
    Serial.print("Total Weight: ");
    Serial.println(loadCell.W_tot, 3);
    loadCell.totalRecorded = true;

    if (loadCell.W_bottle > 0.0f && loadCell.n_pills == 0.0f) {
      // Calculate the number of pills based on the weight difference
      loadCell.n_pills = (loadCell.W_tot - loadCell.W_bottle) / loadCell.avgPillWeight;
      if (loadCell.n_pills != 30){
        digitalWrite(COMM_PIN_OUT, LOW);
      }
      Serial.print("Number of pills: ");
      Serial.println(loadCell.n_pills, 0);  // Print as integer
      Serial.print("To 3 Decimal Places: ");
      Serial.println(loadCell.n_pills, 3); // Print to 3 decimal places
    }
  }

  if ((loadCell.instWeight <= (loadCell.W_tot - 3) && loadCell.totalRecorded == true && digitalRead(COMM_PIN_IN) == LOW)) {
    Serial.println("Resetting values...");
    delay(500);
    loadCell.W_bottle = 0.0f;
    loadCell.W_tot = 0.0f;
    loadCell.n_pills = 0.0f;
    loadCell.weight = 0.0f;
    loadCell.reading = 0.0f;
    loadCell.totalRecorded = false;
    loadCell.bottleRecorded = false;
    digitalWrite(COMM_PIN_OUT, LOW);
  }
}

void runCalibration() {
  Serial.println("\n--- Calibration Mode ---");
  Serial.println("Remember to Remove all weight...");
  delay(1000);

  loadCell.ADCI = loadCell.scale.read_average(30);

  Serial.println("Place known weight (e.g., 50g) on scale.");
  delay(5000); // make it wait until a certain ammount of weight has been added

  long ADCF = loadCell.scale.read_average(30);

  long Delta_ADC = ADCF - loadCell.ADCI;
  Serial.print("ADC Change: ");
  Serial.println(Delta_ADC);

  if ( Delta_ADC >= 1000) {  
    float knownGrams = 50.0f;  // Use 50g as the known weight for calibration
    loadCell.calibration_factor = (float)Delta_ADC / knownGrams;

    Serial.print("New calibration factor: ");
    Serial.println(loadCell.calibration_factor, 4);

    Serial.println("Calibration complete. Remove All Weight\n");
    delay(5000); // wait until 50 grams unloaded}
  } else {
    Serial.println("Too Slow");
  }
}
