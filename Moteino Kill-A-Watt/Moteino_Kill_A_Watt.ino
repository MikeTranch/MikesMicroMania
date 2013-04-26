// VERSION .707

// Moteino_Kill_A_Watt (using LowPowerLab RFM12b library)
// works with JeeNode or Moteino.  (Moteino easier to fit inside KAW)
// www.mikesmicromania.com
// activity LED on JN-DIO2 (Arduino D5)
// Kill-A-Watt coarse current sensor to JN-AIO1 (Arduino A0)
// Kill-A-Watt voltage sensor to JN-AIO2 (Arduino A1)
// Kill-A-Watt frequency signal to JN-INT (Arduino D3)
// Kill-A-Watt fine current sensor to JN-AIO4 (Arduino A3)

#include <RFM12B.h>    // www.lowpowerlab.com, modified to support SetCS()
#include <avr\sleep.h> // core libraries
#include <avr\delay.h>
#include <LowPower.h> // Writeup: http://www.rocketscream.com/blog/2011/07/04/lightweight-low-power-arduino-library/
		      // library: https://github.com/rocketscream/Low-Power

#define CALMODE false // true to xmit raw values (to establish cal factors)

struct {      // payload structure for RFM12B packet 
  int Status; // 0: OK, 1: first reading, -1: low voltage 
  int      N; // # of readings (over 1 line cycle)
  float    V; // AC RMS voltage
  float    A; // AC RMS current, coarse scale
  float    W; // average watts, coarse current scale
  float    a; // AC RMS current, fine scale
  float    w; // average watts, fine current scale
} KAW; 

// cal factors for map() 
const float CFrawVlo = 250.5; // raw value from A2D 
const float CFrawVhi = 270.7; 
const float CFcalVlo = 116.1; // scaled Voltage
const float CFcalVhi = 125.1; 
const float CFrawAlo = 35.8;  // A2D
const float CFrawAhi = 126.8; 
const float CFcalAlo = 2.18;  // Amps (coarse scale current)
const float CFcalAhi = 7.83;  
const float CFrawalo = 57.1;  // A2D
const float CFrawahi = 359.2; 
const float CFcalalo = 0.34;  // amps (fine scale current)
const float CFcalahi = 2.18;  

// radio settings
const int NODEID    =  2; // this node ID
const int GATEWAYID =  1; // send to node 31: broadcast
const int NETWORKID = 99; // Rx must be on same group to receive
const int myCS = 10;      // RFM12B chip select, AKA SS. 9 on my UNO shields. 10 on JeeNodes, Moteinos

RFM12B radio;             // instance of RFM12B class

// pin assignments    JeeNode   Arduino
const int LED    = 6; // DIO3 = D6
const int ASENS  = 0; // AI01 = A0 (Yellow wire from pin 1, coarse current)
const int VSENS  = 1; // AI02 = A1 (White wire from pin 14, voltage)
const int aSENS  = 3; // AI04 = A3 (Blue wire from pin 8, fine current)
const int HzSENS = 3; // IRQ  = D3 (Red wire from Q3 collector, 60Hz)
const int HzINT  = 1; //        INT1
const int MaxN  = 65; // max # readings (good down to 48 Hz)

void setup () { 
  // go to sleep ASAP to give KAW a chance to boot up
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_ON);
  radio.Initialize(NODEID, RF12_433MHZ, NETWORKID, 0, 8, RF12_2v75, myCS);
  //radio.Initialize(NODEID, RF12_433MHZ, NETWORKID);
  radio.Sleep();
  pinMode(HzSENS,INPUT);    // D3 is IRQ1 
  pinMode(LED,OUTPUT);
  digitalWrite(LED,LOW);    // off
  // speed up ADC to minimize phase measurement error
  bitClear(ADCSRA,ADPS0);
  bitClear(ADCSRA,ADPS1);
  bitSet(ADCSRA,ADPS2);
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
  while(measState < 2);  // skip partial cycle
  //digitalWrite(LED,HIGH);
  while((measState < 3) && (n < MaxN)) { // measure 1 cycle
    //digitalWrite(LED,HIGH);
    A[n] = analogRead(ASENS); // 20.4 us
    V[n] = analogRead(VSENS); // 20.4 us
    a[n] = analogRead(aSENS); // 20.4 us
    //digitalWrite(LED,LOW);
    n++;
    // time difference between voltage and current readings is about 20.4 us
    // 20.4 us is about 0.44 degrees at 60 Hz, good enough!
    // target 50 readings over 1 cycle @ 60Hz
    // reading time: 3 * 20.4 us * 50 = 3.06 ms
    // total dead time: (1/60) - 3.06 ms = 13.61 ms
    // dead time between readings: 13.61 ms / 50 = 272 us
    delayMicroseconds(280); // fine tuned for 50 readings @ 60 Hz
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
  
  radio.Wakeup();
#if CALMODE
  radio.Send(GATEWAYID, &KAW, sizeof(KAW));
#else
  radio.Send(GATEWAYID, &KAW, sizeof(KAW)-8);
#endif
  radio.Sleep();
  
  digitalWrite(LED,HIGH); // flash LED so we know we're alive
  delay(10);              // 10 ms visable
  digitalWrite(LED,LOW);  // save power!

  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_ON);
}

void AC60HZ(){ 
  //digitalWrite(LED,HIGH);
  measState++;
  //digitalWrite(LED,LOW);
}

// overload Arduino integer map() function to support floats, doubles
double map(double x, double in_min, double in_max, double out_min, double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

