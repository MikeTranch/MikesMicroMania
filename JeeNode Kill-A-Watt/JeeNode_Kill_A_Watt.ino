// VERSION 1.414

// JeeNode_Kill_A_Watt (using JeeLib)
// works with JeeNode or Moteino.  (Moteino easier to fit inside KAW)
// www.mikesmicromania.com
// activity LED on DIO2 (Arduino Digital 5)
// Kill-A-Watt coarse current sensor to AIO1 (Arduino A0)
// Kill-A-Watt voltage sensor to AIO2 (Arduino A1)
// Kill-A-Watt frequency signal to INT (Arduino D3)
// Kill-A-Watt fine current sensor to AIO4 (Arduino A3)

#include <JeeLib.h> // jeelabs.org

#define CALMODE false // true to xmit raw values (to establish cal factors)

typedef struct { // payload structure for RFM12B packet 
  int Status;    // 0: OK, 1: first reading, -1: low voltage 
  int      N;    // # of readings
  float    V;    // average AC RMS voltage
  float    A;    // average AC RMS current, coarse scale
  float    W;    // average watts, coarse current scale
  float    a;    // average AC RMS current, fine scale 
  float    w;    // average watts, fine current scale
} payload; // typedef

payload KAW; 

// cal factors for map() 
const float CFrawVlo = 248.2; // raw value from A2D 
const float CFrawVhi = 270.9; 
const float CFcalVlo = 115.2; // scaled Voltage
const float CFcalVhi = 125.1; 
const float CFrawAlo = 36.5;  // A2D
const float CFrawAhi = 131.9; 
const float CFcalAlo = 2.18;  // Amps (coarse scale current)
const float CFcalAhi = 8.07;  
const float CFrawalo = 49.7;  // A2D
const float CFrawahi = 361.0; 
const float CFcalalo = 0.29;  // amps (fine scale current)
const float CFcalahi = 2.18;  

// radio settings
const int NODE      =  1; // this node ID
const int BROADCAST = 31; // send to node 31: broadcast
const int GROUP    = 210; // Rx must be on same group to receive
const int myCS = 10;      // RFM12B chip select, AKA SS
                          // 9 on my shields
                          // 10 on JeeNodes, Moteinos

// pin assignments    JeeNode   Arduino
const int LED    = 6; // DIO3 = D6
const int ASENS  = 0; // AI01 = A0 (Yellow wire from pin 1, coarse current)
const int VSENS  = 1; // AI02 = A1 (White wire from pin 14, voltage)
const int aSENS  = 3; // AI04 = A3 (Blue wire from pin 8, fine current)
const int HzSENS = 3; // IRQ  = D3 (Red wire from Q3 collector, 60Hz)
const int HzINT  = 1; //        INT1
const int MaxN  = 65; // max # readings (good down to 48 Hz)
const int NAP  = 925; // nap time in milliseconds

// boilerplate for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup () {
  // turn the radio off in the most power-efficient manner
  Sleepy::loseSomeTime(32);
  rf12_set_cs(myCS); // note this is before rf12_initialize()
  rf12_initialize(NODE, RF12_433MHZ, GROUP);
  rf12_sleep(RF12_SLEEP);
  pinMode(HzSENS,INPUT);    // IRQ1 input, D3 
  pinMode(LED,OUTPUT);
  digitalWrite(LED,LOW);    // off
  // speed up ADC to minimize phase measurment error
  bitClear(ADCSRA,ADPS0);
  bitClear(ADCSRA,ADPS1);
  bitSet(ADCSRA,ADPS2);
  //wait another 2s for the power supply to settle
  Sleepy::loseSomeTime(2000);
}

boolean First = true;            // the very 1st reading
int V[MaxN], A[MaxN], a[MaxN];   // waveform arrays
volatile int unsigned measState; // state machine, clocked by line frequency
boolean toggle;                  // alternate maeasurements on + or - cycle

void loop () {
  // acquire voltage and current samples
  int n = 0;
  measState = 0; // 0 is partial half cycle
  toggle = !toggle;
  if (toggle){
    attachInterrupt(HzINT,AC60HZ,RISING);
  }else{
    attachInterrupt(HzINT,AC60HZ,FALLING);
  }
  while(measState < 2);              // skip partial cycle
  //digitalWrite(LED,HIGH);
  while(measState < 3 && n < MaxN) { // measure 1 cycle
    //digitalWrite(LED,HIGH);
    A[n] = analogRead(ASENS); // 20.4 us
    V[n] = analogRead(VSENS); // 20.4 us
    a[n] = analogRead(aSENS); // 20.4 us
    //digitalWrite(LED,LOW);
    n++;
    // 20.4 us is about 0.44 degrees at 60 Hz
    // 3 * 20.4 us * 100 = 6.12 ms
    // (2/60) - 6.12 ms = 27.2 ms
    // 27.2 ms / 100 = 272 us
    // so delay about 272 us
    delayMicroseconds(280); // fine tuned for 50 or 100 readings
  } 
  //digitalWrite(LED,LOW);
  detachInterrupt(HzINT);
  
  KAW.N = n; // number of readings
  
  // compute offset levels
  float VSum=0, ASum=0, aSum=0, WSum=0, wSum=0;
  for(int i=0; i<n; i++){
    VSum += V[i];
    ASum += A[i];  
    aSum += a[i];
  }
  float aV, aA, aa; // averages
  aV = VSum/n;
  aA = ASum/n;
  aa = aSum/n;
  
  // compute std.dev. = AC RMS
  VSum = ASum = aSum = 0;
  for(int i=0; i<n; i++){
    VSum += (V[i]-aV)*(V[i]-aV);
    ASum += (A[i]-aA)*(A[i]-aA);
    aSum += (a[i]-aa)*(a[i]-aa);
    WSum += (V[i]-aV)*(A[i]-aA);
    wSum += (V[i]-aV)*(a[i]-aa);
  }
  KAW.V = sqrt(VSum/n);
  KAW.A = sqrt(ASum/n);
  KAW.a = sqrt(aSum/n);
  KAW.W = WSum/n;
  KAW.w = wSum/n;

#if not CALMODE  // apply cal factors
  // voltage
  KAW.V = map(KAW.V, CFrawVlo, CFrawVhi, CFcalVlo, CFcalVhi); 
  
  // coarse current scale, above 2 A
  KAW.A = map(KAW.A, CFrawAlo, CFrawAhi, CFcalAlo, CFcalAhi);  
  
  // coarse wattage scale, Current above 2 A
  KAW.W = map(KAW.W, CFrawVlo*CFrawAlo, CFrawVhi*CFrawAhi, CFcalVlo*CFcalAlo, CFcalVhi*CFcalAhi);
  
  // fine current scale, below 2 A
  KAW.a =  map(KAW.a, CFrawalo, CFrawahi, CFcalalo, CFcalahi);  
  
  // fine wattage scale, Current below 2 A
  KAW.w = map(KAW.w, CFrawVlo*CFrawalo, CFrawVhi*CFrawahi, CFcalVlo*CFcalalo, CFcalVhi*CFcalahi);
 
  // prevent negative values (e.g. no low load current, noise)
  KAW.a = max(KAW.a, 0);
  KAW.w = max(KAW.w, 0);
  KAW.A = max(KAW.A, 0);
  KAW.W = max(KAW.W, 0);  
 
  // use fine current scale for 2A or less load current
  if(KAW.a <= 2.0) {
    KAW.A = KAW.a;
    KAW.W = KAW.w;
  } 
#endif
  
  if (First) {
    First = false;
    KAW.Status = 1;          // 1st reading     
  } else if (KAW.V < 115 ) { // KAW analog supply sags
    KAW.Status = -1;         // accuracy warning
  } else {
    KAW.Status = 0;          // OK
  }
  
  rf12_sleep(RF12_WAKEUP);  
  while (!rf12_canSend())                        // wait for xmit ready
    rf12_recvDone();                             // advance RF12 driver state machine
  rf12_sendStart(BROADCAST, &KAW, sizeof(KAW));  // send payload
  rf12_sendWait(0);                              // wait (2 = in standby) for xmit done
  rf12_sleep(RF12_SLEEP);

  digitalWrite(LED,HIGH); // flash LED so we know we're alive
  delay(10);              // 10 ms visable
  digitalWrite(LED,LOW);  // save power!
  
  Sleepy::loseSomeTime(NAP);
}

void AC60HZ(){ 
  measState++;
}

// overload Arduino int map() function to support floats
float map(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

