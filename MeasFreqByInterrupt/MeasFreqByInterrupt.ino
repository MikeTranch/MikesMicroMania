/*
  MeasFreqByInterrupt.ino
  miketranch@comcast.net
  open source, use at will
 */
 
const int encoderPin = 3; // digital input pin
const int intNum = 1;     // corresponding interrupt number
const int LEDpin = 13;    // standard LED pin
boolean toggle = false;   // toggle LED on/off 
volatile long count = 0;  // total interrupts

void setup()
{
  pinMode(encoderPin, INPUT);              // set digit pin to input mode
  digitalWrite(encoderPin, HIGH);          // enable pull up
  pinMode(LEDpin, OUTPUT);                 // set LED pin to output mode
  Serial.begin(9600);                      // init serial monitor
  Serial.println("\nMeasFreqByInterrupt\n");
}

void loop()
{                
  count = 0;
  attachInterrupt(intNum, tally, FALLING); // enable interrupt handler
  delay(1000);                             // measure for 1 second 
  detachInterrupt(intNum);                 // disable interrupt handler
  Serial.print(count);    
  Serial.println(" Hz");
  toggle = !toggle;
  digitalWrite(LEDpin, toggle); 
}

void tally()
{
  count++;  // increment total
}
      
    
