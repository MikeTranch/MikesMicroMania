// Version 0.707

// Moteino_KAW_Rx
// receiver for JeeNode or Moteino Kill-A-Watt
// www.mikesmicromania.com

#include <RFM12B.h> // customized version to fix SetCS()

const int AVG = 5; // # running averages

#define CALMODE false // true to xmit raw values (to establish cal factors)

struct payload { // payload structure for RFM12B packet 
  int Status;    // 0: OK, 1: first reading, -1: low line voltage 
  int      N;    // # of readings (over one line cycle)
  float    V;    // AC RMS voltage
  float    A;    // AC RMS current, coarse scale
  float    W;    // average watts, coarse current scale
#if CALMODE
  float    a;    // AC RMS current, fine scale
  float    w;    // average watts, fine current scale
#endif
} KAW;  

const int NODEID = 1;      // local node ID (range 1-30)
const int NETWORKID = 99;  // network group (range 1-212)
const int myCS = 10;       // RFM12B chip select, AKA SS. 9 on my Uno shields. 10 on JeeNodes, Moteinos
                                
RFM12B radio; // an instance of the RFM12B class

void setup() {
  Serial.begin(57600);
  Serial.println("\nMoteino KAW Receiver");
  radio.Initialize(NODEID, RF12_433MHZ, NETWORKID, 0, 8, RF12_2v75, myCS);
  //radio.Initialize(NODEID, RF12_433MHZ, NETWORKID);
}

int n=0;
float avgV=0, avgA=0, avgVA, avgW, AVGa=0, AVGva, AVGw;

void loop() { // listen for broadcast from KAW
  if (radio.ReceiveComplete()) { 
    if (radio.CRCPass()) {  
      KAW = *(payload*)rf12_data;
      n = min(n+1, AVG);
      if(n==1){ // first readings received
        avgV = KAW.V;
        avgA = KAW.A;
        avgW = KAW.W;
#if CALMODE        
        AVGa = KAW.a;
        AVGw = KAW.w; 
#endif
      }else{ // calc running average
        avgV = (avgV * (n-1) + KAW.V) / n;
        avgA = (avgA * (n-1) + KAW.A) / n;
        avgW = (avgW * (n-1) + KAW.W) / n;
#if CALMODE 
        AVGa = (AVGa * (n-1) + KAW.a) / n;
        AVGw = (AVGw * (n-1) + KAW.w) / n;
#endif        
      }
      avgVA = avgV * avgA; // Volt-Amps, ignoring Power Factor
#if CALMODE 
      AVGva = avgV * AVGa; // same, but on the fine current scale
#endif      
      Serial.print(KAW.Status); Serial.print(" Status, ");
      Serial.print(KAW.N);      Serial.print(" N, ");
      Serial.print(avgV);       Serial.print(" V, ");
      Serial.print(avgA);       Serial.print(" A, ");
      Serial.print(avgVA);      Serial.print(" VA, ");
      Serial.print(avgW);       Serial.print(" W, ");
      Serial.print(AVGa);       Serial.print(" a");
#if CALMODE       
                                Serial.print(", ");
      Serial.print(AVGva);      Serial.print(" Va, ");
      Serial.print(AVGw);       Serial.print(" w");
#endif      
      Serial.println();
    } else {
      Serial.println("Bad CRC!");
    } 
  }
}

