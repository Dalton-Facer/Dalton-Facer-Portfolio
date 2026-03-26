#include <Servo.h>
#include <HX711.h>

#define COMM_PIN_OUT 13  // Output pin to master
#define COMM_PIN_IN 12  // Input pin from master

unsigned long currentTime = 0;
unsigned long now3 = 0;
bool bbl = false;
const int motor1Pin = 10;
const int motor2Pin = 11;
const int ENApin = 53;
float actuatorTime = 0;
int slotOne = 0;
int slotTwo = 0;
int slotThree = 0;

struct StepperTask {
  int stepPin; 
  int dirPin;
  bool run = false;
  bool stepState = LOW;
  unsigned long lastStepTime = 0;
  int stepCount = 0;
  int maxSteps = 2000;
  int stepDelay = 500;
  bool complete = false;
}; 

struct ServoTask {
  Servo servo;
  int pin;
  int angleStart, angleEnd;
  bool run = false;
  bool returning = false;
  unsigned long startTime = 0;
  int waitTime1 = 1500;
  int waitTime2 = 3000;
  bool finished = false;
  bool complete = false;
};
unsigned long pillServoCompleteTime = 0;

struct StepperTask botRot;
bool botRotdelay = false;
struct StepperTask conv;
struct StepperTask pillDesc;
struct StepperTask topRot;

struct ServoTask pillServo;
struct ServoTask rejectServo;
// struct LoadCellTask loadCell;

void setup() {
  Serial.begin(9600);
  // Serial.println("Starting");
  botRot.stepPin = 2;
  botRot.dirPin = 3;
  botRot.maxSteps = 1600; //805
  conv.stepPin = 4;
  conv.dirPin = 5;
  pillDesc.stepPin = 6;
  pillDesc.dirPin = 7;
  pillDesc.maxSteps = 10000;
  topRot.stepPin = 8;
  topRot.dirPin = 9;

  pillServo.servo = Servo();
  pillServo.pin = 31;
  pillServo.angleStart = 70;
  pillServo.angleEnd = 0;
  pillServo.finished = false;
  rejectServo.servo = Servo();
  rejectServo.pin = 33;
  rejectServo.angleStart = 0;
  rejectServo.angleEnd = 180;
  rejectServo.finished = true;
  rejectServo.complete = true;
  //rejectServo.waitTime1 = 1000;
  //rejectServo.waitTime2 = 1500;

  pinMode(botRot.stepPin, OUTPUT);
  pinMode(botRot.dirPin, OUTPUT);

  pinMode(conv.stepPin, OUTPUT);
  pinMode(conv.dirPin, OUTPUT);

  pinMode(pillDesc.stepPin, OUTPUT);
  pinMode(pillDesc.dirPin, OUTPUT);

  pinMode(topRot.stepPin, OUTPUT);
  pinMode(topRot.dirPin, OUTPUT);

  digitalWrite(botRot.dirPin, LOW);

  pinMode(22, INPUT_PULLUP); // Start
  pinMode(24, INPUT_PULLUP); // Stop
  pinMode(26, INPUT_PULLUP); // Reset

  // Attach servo
  pillServo.servo.attach(pillServo.pin);
  pillServo.servo.write(pillServo.angleStart); // Default pos

  rejectServo.servo.attach(rejectServo.pin);
  rejectServo.servo.write(rejectServo.angleStart); // Default pos

  pinMode(ENApin, OUTPUT);
  pinMode(motor1Pin, OUTPUT);
  pinMode(motor2Pin, OUTPUT);

  digitalWrite(motor1Pin, LOW);
  digitalWrite(motor2Pin, LOW);

  digitalWrite(ENApin, HIGH);

  pinMode(COMM_PIN_OUT, OUTPUT);
  pinMode(COMM_PIN_IN, INPUT);
}

void loop() {
  currentTime = millis();
  rejectServo.run = true;
  handleButtons();
  if (pillServo.complete && (currentTime - pillServoCompleteTime >= 3000)) { // waiting, 
    
    //store values in slots (high = 30) (low = not 30)
    if (digitalRead(COMM_PIN_IN) == HIGH) {
      digitalWrite(COMM_PIN_OUT, LOW);
      botRot.run = true;
      pillDesc.run = true;
      pillServo.complete = false;
      rejectServo.complete = false;
      slotOne = 2;
    }
    else {
      digitalWrite(COMM_PIN_OUT, LOW);
      botRot.run = true;
      pillDesc.run = true;
      pillServo.complete = false;
      rejectServo.complete = false;
    }
    pillServo.finished = true;
  }

  if (pillServo.finished == true && rejectServo.finished == true) {
    botRot.run = true;
    pillServo.finished = false;
    rejectServo.finished = false;
  }

  if (botRot.run) {
     runStepper(botRot);
  }

  if (topRot.run && !bbl) {
    runStepper(topRot);
  }

  if (conv.run) {
    runStepper(conv);
  } 

  if (pillDesc.run) {
    runStepper(pillDesc);
  }
  
  if (pillServo.run) {
    // Serial.println(pillServo.run);
    runServo(pillServo);

  }
    
  if (rejectServo.run) {
    runServo(rejectServo);
  }

  runActuator();
}

void handleButtons() {
  if (digitalRead(22) == HIGH) { // Start button pressed
    //Serial.println("Start pressed");
    botRot.run = true;
    botRot.stepCount = 0;
    conv.run = true;
    conv.stepCount = 0;
    pillDesc.run = true;
    pillDesc.stepCount = 0;
    topRot.run = true;
    topRot.stepCount = 0;

    pillServo.run = false;
    pillServo.startTime = millis();
    pillServo.returning = false;
    pillServo.servo.write(pillServo.angleStart);

    rejectServo.run = true;
    rejectServo.startTime = millis();
    rejectServo.returning = false;
    rejectServo.servo.write(rejectServo.angleStart);
  }

  if (digitalRead(24) == HIGH) { // Stop button pressed
    //Serial.println("Stop pressed");
    botRot.run = false;
    conv.run = false;
    pillDesc.run = false;
    topRot.run = false;

    pillServo.run = false;
    rejectServo.run = false;
  }
}

void runActuator() {
  unsigned long now2 = millis();
  if ((now2 - actuatorTime) >= 1500 && (now2 - actuatorTime) <= 4500){
    digitalWrite(motor1Pin, HIGH);
    digitalWrite(motor2Pin, LOW);
  }
  else if ((now2 - actuatorTime) >= 4500 && (now2 - actuatorTime < 7500)) {
    digitalWrite(motor2Pin, HIGH);
    digitalWrite(motor1Pin, LOW);
  }
  else if ((now2 - actuatorTime) >= 7500) {
    actuatorTime = now2;
  }
}

void runStepper(StepperTask &stepper) {
  now3 = micros();
  if ((now3 - stepper.lastStepTime) >= stepper.stepDelay) {
    botRot.stepDelay = 500;
    stepper.stepState = !stepper.stepState;
    digitalWrite(stepper.stepPin, stepper.stepState);
    //delayMicroseconds(stepper.stepDelay);
    //digitalWrite(stepper.stepPin, LOW);
    stepper.lastStepTime = now3;
    stepper.stepCount++;

    if (stepper.stepCount >= stepper.maxSteps) {
      if (stepper.stepPin == botRot.stepPin) {
        slotThree = slotTwo;
        slotTwo = slotOne;
        delay(500); // Wait for load cell to calculate
        if (digitalRead(COMM_PIN_IN) == HIGH) {
          botRot.run = false;
          botRotdelay = true;
          pillDesc.run = false;
          pillServo.run = true;
          slotOne = 1;
        }
        else {
          slotOne = 0;
        }
      }
      stepper.stepCount = 0;
      //Serial.println("Stepper done");
    }
  }
}

void runServo(ServoTask &servoTask) {
  unsigned long now = millis();

  if (!servoTask.returning && ((now - servoTask.startTime) >= servoTask.waitTime1)) {
    
    if (pillServo.pin == servoTask.pin) {
      servoTask.servo.write(servoTask.angleEnd);
      servoTask.returning = true;
      servoTask.startTime = now;
    }
    if (rejectServo.pin == servoTask.pin) {
      if (slotThree == 1 && servoTask.finished) {
        servoTask.servo.write(servoTask.angleEnd);
        servoTask.returning = true;
        servoTask.startTime = now;
        //Serial.println("i be rejectin");
      }
    }
  } 
  else if (servoTask.returning && ((now - servoTask.startTime) >= servoTask.waitTime2)) {
    servoTask.servo.write(servoTask.angleStart);

    if (pillServo.pin == servoTask.pin) {
      digitalWrite(COMM_PIN_OUT, HIGH);
      servoTask.complete = true;
      pillServoCompleteTime = millis();
      servoTask.returning = false;
      servoTask.run = false;

    }
    if (rejectServo.pin == servoTask.pin) {
      servoTask.returning = false;
      servoTask.complete = true;
      //Serial.println("i should be returnin");   
      rejectServo.finished = true;
    }
  }
}
