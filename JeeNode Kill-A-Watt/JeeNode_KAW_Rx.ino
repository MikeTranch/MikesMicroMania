// Version 0.707

// JeeNode_KAW_Rx
// receiver for JeeNode Kill-A-Watt, using JeeLib
// www.mikesmicromania.com

#include <RF12.h> // from jeelabs.org

const int AVG = 3; // # running averages

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

const int Node = 30;            // local node ID (range 1-30)
const int network = 210;        // network group (range 1-212)
const int myFreq = RF12_433MHZ; // must match RFM12B hardware
const int myCS = 9;             // RFM12B chip select, AKA SS
                                // 9 on my shields
                                // 10 on JeeNodes
void setup() {
  rf12_set_cs(myCS); // note this is before rf12_initialize()
  rf12_initialize(Node, myFreq, network); 
  Serial.begin(57600);
  //Serial.setTimeout(1000); // ms, timeout for Serial.readBytesUntil()
  Serial.println("\nJeeNode Kill-A-Watt Receiver");
}

int n=0;
float avgV=0, avgA=0, avgVA, avgW, AVGa=0, AVGva, AVGw;

void loop() { // listen for broadcast from JeeNode_KAW
  if (rf12_recvDone() && rf12_crc == 0 ) { // got data with good CRC?
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
  } 
}

