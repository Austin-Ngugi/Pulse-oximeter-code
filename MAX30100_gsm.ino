#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

#define REPORTING_PERIOD_MS     1000

// PulseOximeter is the higher level interface to the sensor
// it offers:
//  * beat detection reporting
//  * heart rate calculation
//  * SpO2 (oxidation level) calculation
//gsm module notification and emergency alert
PulseOximeter pox;

uint32_t tsLastReport = 0;

// Callback (registered below) fired when a pulse is detected

#include <SoftwareSerial.h>

#define rxPin 2
#define txPin 3
SoftwareSerial sim800L(rxPin,txPin); 

String buff; //buff is typed as a void pointer, which means it points to memory without declaring anything about the contents.

void onBeatDetected()
{
    Serial.println("Beat!");
}

void setup()
{
    Serial.begin(115200);

    Serial.print("Initializing pulse oximeter..");

    // Initialize the PulseOximeter instance
    // Failures are generally due to an improper I2C wiring, missing power supply
    // or wrong target chip
    if (!pox.begin()) {
        Serial.println("FAILED");
        for(;;);
    } else {
        Serial.println("SUCCESS");
    }

    // The default current for the IR LED is 50mA and it could be changed
    //   by uncommenting the following line. Check MAX30100_Registers.h for all the
    //   available options.
    // pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);

    // Register a callback for the beat detection
    pox.setOnBeatDetectedCallback(onBeatDetected);

    //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(115200);
  
  //Begin serial communication with Arduino and SIM800L
   
   sim800L.begin(115200);
   Serial.println("Initializing...");
   sim800L.println("AT"); //This is the most basic AT command. It also initializes the Auto-bauder. 
   //If all is well, it sends the OK message, telling you that it is understanding you correctly. You can then send some commands to query the module and get information about it.
   waitForResponse();

   sim800L.println("ATE1"); //Turns OFF/ON the echo mode of the modem. ATE1 turns ON the echo of the modem
   //In telecommunications, echo is the local display of data, either initially as it is locally sourced and sent, or finally as a copy of it is received back from a remote destination. 
   //Local echo is where the local sending equipment displays the outgoing sent data.
   waitForResponse();

   sim800L.println("AT+CMGF=1");//AT+CMGF=1 – Selects the SMS message format as text. The default format is Protocol Data Unit (PDU).
   waitForResponse();

   sim800L.println("AT+CNMI=1,2,0,0,0"); //Specifies how incoming SMS messages should be handled. 
   //This way you can tell the SIM800L module to either forward incoming SMS messages directly to the PC, or save them to the message storage and then inform the PC about their locations in the message storage.
   waitForResponse();
}

void loop()
{
    // Make sure to call update as fast as possible
    pox.update();

    // Asynchronously dump heart rate and oxidation levels to the serial
    // For both, a value of 0 means "invalid"
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) 
    {
        Serial.print("Heart rate:");
        Serial.print(pox.getHeartRate());
        
        Serial.print("bpm / SpO2:");
        Serial.print(pox.getSpO2());
        Serial.println("%");

        tsLastReport = millis();
    
    }
    if((pox.getHeartRate() > 100) || (pox.getSpO2()< 95))
    {
      send_Esms();
      make_call();
    }
    while(sim800L.available())
    {
    buff = sim800L.readString();
    Serial.println(buff);
    }
    while(Serial.available())  
    {
      buff = Serial.readString();
      buff.trim();
      if(buff == "s")
      send_sms();
      else if(buff== "c")
      make_call();
      else
      sim800L.println(buff);
  }
}

void send_sms()
{
  sim800L.print("AT+CMGS=\"+254700532606\"\r"); //Sends SMS to the specified phone number. After this AT command any text message followed by ‘Ctrl+z’ character is treated as SMS.
  waitForResponse();
  
  sim800L.print("Hello from SIM800L");
  sim800L.print("Heart rate:");
  sim800L.print(pox.getHeartRate());
  sim800L.print("bpm / SpO2:");
  sim800L.print(pox.getSpO2());
  sim800L.println("%");
  sim800L.write(0x1A);
  waitForResponse();
}
void send_Esms()
{
  sim800L.print("AT+CMGS=\"+254700532606\"\r"); //Sends SMS to the specified phone number. After this AT command any text message followed by ‘Ctrl+z’ character is treated as SMS.
  waitForResponse();
  
  sim800L.print("Hello from PulseOximeter");
  sim800L.print("EMERGENCY, SEEK MEDICAL ASSISTANCE IMMEDIATELY");
  sim800L.write(0x1A);
  waitForResponse();
}
void make_call()
{
  sim800L.println("ATD++254700532606;");
  waitForResponse();
}

void waitForResponse()
{
  delay(1000);
  while(sim800L.available()){
  Serial.println(sim800L.readString());
  }
  sim800L.read();
}
