

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

#define MAX_BRIGHTNESS 255

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//Arduino Uno doesn't have enough SRAM to store 100 samples of IR led data and red led data in 32-bit format
//To solve this problem, 16-bit MSB of the sampled data will be truncated. Samples become 16-bit data.
uint16_t irBuffer[100]; //infrared LED sensor data
uint16_t redBuffer[100];  //red LED sensor data
#else
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
#endif

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

byte pulseLED = 11; //Must be on PWM pin
byte readLED = 13; //Blinks with each data read

#include <SoftwareSerial.h>

#define rxPin 2
#define txPin 3
SoftwareSerial sim800L(rxPin,txPin); 

String buff; //buff is typed as a void pointer, which means it points to memory without declaring anything about the contents.

void setup()
{
  Serial.begin(115200); // initialize serial communication at 115200 bits per second:

  pinMode(pulseLED, OUTPUT);
  pinMode(readLED, OUTPUT);

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while (1);
  }

  //Serial.println(F("Attach sensor to finger with rubber band. Press any key to start conversion"));
  //while (Serial.available() == 0) ; //wait until user presses a key
  //Serial.read();

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 1; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 1; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 50; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 69; //Options: 69, 118, 215, 411
  int adcRange = 2048; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

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
  bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample

    Serial.print(F("red="));
    Serial.print(redBuffer[i], DEC);
    Serial.print(F(", ir="));
    Serial.println(irBuffer[i], DEC);
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  while (1)
  {
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++)
    {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }

    //take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data

      digitalWrite(readLED, !digitalRead(readLED)); //Blink onboard LED with every data read

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample

      //send samples and calculation result to terminal program through UART
      Serial.print(F("red="));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", ir="));
      Serial.print(irBuffer[i], DEC);

      Serial.print(F(", HR="));
      Serial.print(heartRate, DEC);

      Serial.print(F(", HRvalid="));
      Serial.print(validHeartRate, DEC);

      Serial.print(F(", SPO2="));
      Serial.print(spo2, DEC);

      Serial.print(F(", SPO2Valid="));
      Serial.println(validSPO2, DEC);
    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
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
 if((validHeartRate> 100) || (validSPO2< 95))
 {
  send_Esms();
  make_call();
 }
}

void send_sms()
{
  sim800L.print("AT+CMGS=\"+254700532606\"\r"); //Sends SMS to the specified phone number. After this AT command any text message followed by ‘Ctrl+z’ character is treated as SMS.
  waitForResponse();
  
  sim800L.print("Hello from SIM800L");
  sim800L.print(validHeartRate, DEC);
  //sim800L.print(pox.getHeartRate());
  //sim800L.print("bpm / SpO2:");
  sim800L.print(validSPO2, DEC);
  //sim800L.println("%");
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
