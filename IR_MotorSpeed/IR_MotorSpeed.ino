/*
 IR_MotorSpeed.pde miketranch comcast.net V0.4 Feb 8, 2013
 10K pot sets desired speed. 
 IR detector measures fan speed, adjusts PWM voltage to regulate speed
 Arduino UNO pins used:
   D3  IR detector, interrupt 1
   D5  PWM output drives motor
   A0  10K pot
*/

const int potPin = 0;    // 10K pot 5V to ground, wiper to pin A0 
const int PWMLED = 5;    // PWM output to transistor to motor
const int IR_Pin = 3;    // Hall Sensor Output on Pin 3
const int IR_Int = 1;    // Int #1 on Pin 3
volatile int count = 0;  // count interrupts from IR Sensor

void setup()
{
  pinMode(IR_Pin, INPUT_PULLUP);          // open collector IR detector
  Serial.begin(9600);
  Serial.println("\n\nInfrared Fan Motor Speed Control");
}

void loop() { 
  // static variables "remember" their values each time loop() executes
  static int PWMval;             // motor speed and LED brightness
  
  // regular variables are initialized each time loop() executes
  int potVal = analogRead(potPin);              // read the voltage on the pot
  int SetSpeed = map(potVal, 0, 1023, 50, 400); // map to desired speed range
  
  if (count == 0) {                   // motor stopped, kick start it
    PWMval = 254;                     // full voltage and power
    Serial.println("Full Power Start"); 
  } else {    // adjust speed
    float delta = (SetSpeed - count); // speed error
    if (delta > 0 )                   // too slow, increase voltage
      PWMval = PWMval + sqrt(delta);  // by square root of speed error
    else                              // too fast, decrease voltage 
      PWMval = PWMval - sqrt(-delta); // by square root of speed error
  } 
  
  PWMval = constrain(PWMval, 0, 255); // in any event PWM must be 0 to 255
  
  Serial.print("Set speed: "); Serial.print(SetSpeed);
  Serial.print(" PWM: ");      Serial.print(PWMval);
  
  analogWrite(PWMLED, PWMval);  // set the motor voltage level
  delay(1000);                  // give motor time to settle to new speed
  
  count = 0;
  attachInterrupt(IR_Int, IR_ISR, CHANGE); // count each fan blade twice
  delay(1000);                             // measure for 1 second
  detachInterrupt(IR_Int);                 // disable interrupt 
  
  Serial.print(" Speed: "); Serial.println(count);
}
     
void IR_ISR() {
  count++;  
}

