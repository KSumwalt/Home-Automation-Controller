//CAN Message format
//  canMsg.can_id  = unitID;          //CAN id - Use for sending device ID
//  canMsg.can_dlc = 4;               //CAN data length
//  canMsg.data[0] = btn->getID();    //Device ID message is intended for 0xFF = Any
//  canMsg.data[1] = 1;               //Message Content 1=button press 2=LED Control 3=?
//  canMsg.data[2] = 1;               //If button press: button number
//  canMsg.data[3] = 1;               //If button press: pressed type 1=single 2=double 3=long
//  canMsg.data[2] = 1;               //If LED control: LED number
//  canMsg.data[3] = 1;               //If LED control:  0=Off 1=On
////  canMsg.data[4] = 0x00;          //Rest all with 0
////  canMsg.data[5] = 0x00;
////  canMsg.data[6] = 0x00;
////  canMsg.data[7] = 0x00;

#include <SPI.h>
#include <mcp2515.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Unit ID is used to know which Button Panel is sending the message.
//  This should start at 0 and go up by 1 for each added Button Panel
#define unitID 0xAA

//Enter your wifi credentials
const char* ssid = "Discovery One";  
const char* password = "madidie01";

#define I2Cbme 0x76

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define WORKSTATION_NOTDEFINED      0
#define WORKSTATION_DUSTCOLLECTOR   1
#define WORKSTATION_VACUUM          2
#define WORKSTATION_AIRFILTER       3
#define WORKSTATION_AIRCOMPRESSOR   4
#define WORKSTATION_WORKBENCH_BOTH  5
#define WORKSTATION_WORKBENCH_CANS  6
#define WORKSTATION_WORKBENCH_STRIP 7
#define WORKSTATION_TABLESAW        8
#define WORKSTATION_BANDSAW         9
#define WORKSTATION_ROUTERTABLE     10
#define WORKSTATION_BACKBENCH       11
#define WORKSTATION_FRONTBENCH      12
#define WORKSTATION_CENTRAL         13
#define WORKSTATION_GARAGEDOOR      14
#define WORKSTATION_ALL             99

#define UNIT_NOTDEFINED          0
#define UNIT_DUSTCOLLECTOR       1
#define UNIT_WORKBENCHSTRIP      2
#define UNIT_BANDSAWSTRIP        3
#define UNIT_BACKBENCHSTRIP      4
#define UNIT_FRONTBENCHSTRIP     5
#define UNIT_ROUTERTABLESTRIP    6

#define UNIT_AIRFILTER          14
#define UNIT_TABLESAWCANS       15
#define UNIT_WORKBENCHCANS      16

#define BYMQTT_VACUUM            100
#define BYMQTT_AIRCOMPRESSOR     101
#define BYMQTT_CENTRALLIGHT      102
#define BYMQTT_GARAGEDOOR        103

const int relaysControlled_arraysize = 17;
const int externalDevicesControlled_arraysize = 4;
const int devicesControlled_arraysize = 20;
const int buttonPanels = 6;
const int buttonPanelMaxButtons = 6;
//const int devicesControlled_arraysize = relaysControlled_arraysize + externalDevicesControlled_arraysize - 1;
const int workstations_arraysize = 16;
bool workstationsStatus[workstations_arraysize] = {false, false, false, false, false, false, false, false, false, false, false, false};
bool relaysStatus[relaysControlled_arraysize] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
int relaysControlled[workstations_arraysize][devicesControlled_arraysize] = {
              {UNIT_DUSTCOLLECTOR},
              {BYMQTT_VACUUM},
              {UNIT_AIRFILTER},
              {BYMQTT_AIRCOMPRESSOR},
              {UNIT_WORKBENCHCANS,UNIT_WORKBENCHSTRIP},
              {UNIT_WORKBENCHCANS},
              {UNIT_WORKBENCHSTRIP},
              {UNIT_TABLESAWCANS,BYMQTT_CENTRALLIGHT},
              {UNIT_BANDSAWSTRIP, UNIT_TABLESAWCANS, BYMQTT_CENTRALLIGHT},
              {UNIT_ROUTERTABLESTRIP, BYMQTT_CENTRALLIGHT},
              {UNIT_BACKBENCHSTRIP,UNIT_WORKBENCHSTRIP},
              {UNIT_FRONTBENCHSTRIP,UNIT_WORKBENCHCANS},
              {BYMQTT_CENTRALLIGHT},
              {BYMQTT_GARAGEDOOR},
              {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}
};

//Array to save the association between a Button and the Workstation it controls
int buttonPanelsSinglePressWorkstations[buttonPanels][buttonPanelMaxButtons] = {
  {1,2,6,4,5,6},
  {2,5,1,5,6,5},
  {3,4,2,6,1,2},
  {4,5,3,1,2,3},
  {5,6,4,2,3,4},
  {6,1,5,3,4,5}
};
//int buttonPanelsSinglePressWorkstations[buttonPanels][buttonPanelMaxButtons] = {
//  {WORKSTATION_CENTRAL, WORKSTATION_AIRCOMPRESSOR, WORKSTATION_AIRFILTER},
//  {WORKSTATION_TABLESAW, WORKSTATION_ROUTERTABLE, WORKSTATION_BANDSAW, WORKSTATION_WORKBENCH_BOTH, WORKSTATION_DUSTCOLLECTOR, WORKSTATION_VACUUM},
//  {WORKSTATION_BANDSAW, WORKSTATION_FRONTBENCH, WORKSTATION_WORKBENCH_BOTH, WORKSTATION_CENTRAL, WORKSTATION_DUSTCOLLECTOR, WORKSTATION_VACUUM},
//  {WORKSTATION_CENTRAL, WORKSTATION_AIRCOMPRESSOR, WORKSTATION_GARAGEDOOR},
//  {WORKSTATION_BACKBENCH, WORKSTATION_WORKBENCH_BOTH, WORKSTATION_FRONTBENCH, WORKSTATION_DUSTCOLLECTOR, WORKSTATION_VACUUM, WORKSTATION_AIRCOMPRESSOR},
//  {WORKSTATION_WORKBENCH_BOTH, WORKSTATION_FRONTBENCH, WORKSTATION_BACKBENCH, WORKSTATION_DUSTCOLLECTOR, WORKSTATION_VACUUM, WORKSTATION_AIRCOMPRESSOR}
//};
int buttonPanelsDoublePressWorkstations[buttonPanels][buttonPanelMaxButtons] = {
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_WORKBENCH_CANS, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_WORKBENCH_STRIP, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL}
};
int buttonPanelsLongPressWorkstations[buttonPanels][buttonPanelMaxButtons] = {
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_WORKBENCH_CANS, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL},
  {WORKSTATION_WORKBENCH_STRIP, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL, WORKSTATION_ALL}
};
//For the display
//#define sclk D4
//#define mosi D5
//#define rst  D6
//#define dc   D7
//#define cs   D8
#define sclk 22
#define mosi 21
#define cs   14
#define rst  33
#define dc   27

static unsigned long lastMillis = 0; // holds the last read millis()
static int timer_1 = 0; // a repeating timer max time 32768 mS = 32sec use a long if you need a longer timer
// NOTE timer MUST be a signed number int or long as the code relies on timer_1 being able to be negative
// NOTE timer_1 is a signed int
#define TIMER_INTERVAL_1 10000  // 10 second interval

const char* mqttPayloadRoot = "1st_Floor/Garage/";
const char* mqttInternalPayloads = "Workstations";
const char* mqttStatusTopicSubstring = "Status";
const char* mqttCommandTopicSubstring = "Cmnd";
const char* mqttSetRelaysTopicSubstring = "SetRelays";
const char* mqttSubscription = "1st_Floor/Garage/#";
//const char* myPayloadDevice01 = "l01";
//const char* myPayloadDevice02 = "l02";
//const char* myPayloadDevice03 = "l03";
//const char* myPayloadDevice04 = "l04";
//const char* myPayloadDevice05 = "l05";
//const char* myPayloadDevice06 = "l06";
//const char* myPayloadDevice07 = "l07";
//const char* myPayloadDevice08 = "l08";
//const char* myPayloadDevice09 = "l09";
//const char* myPayloadDevice10 = "l10";
//const char* myPayloadDevice11 = "l11";
const char* myPayloadStatus = "Status";
const char* myPayloadOn = "On";
const char* myPayloadOff = "Off";

char *mqttPayloadDevices[] = {
    "01",
    "02",
    "03",
    "04",
    "05",
    "06",
    "07",
    "08",
    "09",
    "10",
    "11",
    "12",
    "13",
    "14",
    "15",
    "16"
  };

const int mqttExternalPayloads_arraysize = 4;
char *mqttExternalPayloads[] = {
    "Vacuum/",
    "Air_Compressor/",
    "Ceiling_Light_TEST/"
  };


WiFiClient espClient;


/* MQTT Settings */
const char* mqttServer = "192.168.1.2";    //Enter Your mqttServer address
const int mqttPort = 1883;       //Port number
const char* mqttUser = ""; //User
const char* mqttPassword = ""; //Password
//const char* statusTopic  = "remote01";    // MQTT topic to publish status reports
char statusTopic[128];

char messageBuffer[128];
char topicBuffer[128];
char clientBuffer[64];

PubSubClient client(espClient);


struct can_frame canMsg;
MCP2515 mcp2515(5);

//For the MCP23017 i2c module which controls the relays
Adafruit_MCP23017 mcpRelays;

Adafruit_BME280 bme; // I2C


void setup() {
  
  //Set up Display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(1000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  
  //used by the Timer which is used to turn off the display
  timer_1 = TIMER_INTERVAL_1; // timeout in first loop
    
  uint16_t time = millis();
//  display.fillScreen(BLACK);
  time = millis() - time;

  //Define the full value of statusTopic
  strcpy(statusTopic, mqttPayloadRoot);
  strcat(statusTopic, mqttStatusTopicSubstring);


//  display.begin();
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
//  display.setCursor(0, 5);
  display.println("Setting CAN bus");
  display.display();
  
  canMsg.can_id  = unitID;
  canMsg.can_dlc = 4;
  canMsg.data[0] = 0xFF; //Messages out from the Button Panel are intended for any device which wants to use them
  canMsg.data[1] = 0x00;
  canMsg.data[2] = 0x00;
  canMsg.data[3] = 0x00;
  canMsg.data[4] = 0x00;
  canMsg.data[5] = 0x00;
  canMsg.data[6] = 0x00;
  canMsg.data[7] = 0x00;

  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();
  
  Serial.begin(115200);
  delay(10);

//  i2cSetup ();
  //Set the MCP23017 pins to outputs to control relays
  display.println("Setting Relays");
  display.print(".");
  display.display();
  mcpRelays.begin(0x27);      // use address found via the i2c_scanner code
  for (byte a=0; a<16; a++) {
    display.print(".");
    display.display();
    mcpRelays.pinMode(a, OUTPUT);
    delay(150);
  }

  display.clearDisplay();
  display.display();



  //Set up the BMP sensor
  if (!bme.begin(I2Cbme)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
//    display.fillScreen(BLACK);
    display.clearDisplay();
//    display.setTextColor(RED);
    display.setCursor(0,0);
    display.print(F("Could not find a valid BMP280 (temperature) sensor"));
    display.display();
    while (1);
  }




  WiFi.begin(ssid, password);

  bool wifConnected = true;
  int count = 0;

  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("Connecting to WiFi"));
  display.display();
 
  while (WiFi.status() != WL_CONNECTED) {
    count += 1;
    delay(500);
    display.print(".");
    display.display();
    if (count > 600) {
      // It has been longer than 5 minutes so stop trying to connect to WiFi
      // 1 minute in millliseconds = 60000
      // 5 minutes in milliseconds = 300000
      // Divid by the delay of 500 = 600
      wifConnected = false;
      break;
    }
  }
//  Serial.println("");
  if (wifConnected) {
    Serial.println("WiFi connected");
//    display.fillScreen(BLACK);
    display.clearDisplay();
    display.setCursor(0, 0);
//    display.setTextColor(GREEN);
    display.print("WiFi connected");
    display.display();
  } else {
    Serial.println("WiFi not found");
//    display.fillScreen(BLACK);
    display.clearDisplay();
    display.setCursor(0, 0);
    //    display.setTextColor(RED);
    display.print("WiFi not found");
    display.display();
  }


  client.setServer(mqttServer, mqttPort);
  client.setCallback(MQTTcallback);
 
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
//    display.fillScreen(BLACK);
//    display.setTextColor(BLUE);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Connecting to MQTT...");
      display.display();
 
//    if (client.connect("ESP8266", mqttUser, mqttPassword )) {
    if (client.connect(statusTopic, mqttUser, mqttPassword )) {
 
      Serial.println("connected");
//      display.fillScreen(BLACK);
//      display.setTextColor(BLUE);
//      display.setCursor(0,0);
//      display.print("MQTT connected");  
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("MQTT connected");
      display.display();
 
    } else {
      Serial.println("MQTT failed with state ");
      Serial.print(client.state());  //If you get state 5: mismatch in configuration
//      display.fillScreen(BLACK);
//      display.setTextColor(RED);
//      display.setCursor(0,0);
//      display.print("MQTT failed with state ");
//      display.print(client.state());  //If you get state 5: mismatch in configuration
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("MQTT failed with state "));
      display.print(client.state());  //If you get state 5: mismatch in configuration
      display.display();
      
      delay(2000); 
    }
  }

  // Subscribe to the appropriate MQTT Topic
  client.subscribe(mqttSubscription);
  
  // Request the current status of devices
  strcpy(topicBuffer, mqttPayloadRoot);
  strcat(topicBuffer, myPayloadStatus);
  client.publish(topicBuffer, "requestCurrent");
  
}


 
/**
  * MQTT callback
  *    mqttPayloadRoot = "1st_Floor/Garage/"
  *    mqttInternalPayloads = "Workstations"
  *    mqttStatusTopicSubstring = "Status"
  *    mqttCommandTopicSubstring = "Cmnd"
  *    mqttSetRelaysTopicSubstring = "SetRelays"
  *    myPayloadStatus = "Status"
  *    myPayloadOn = "On"
  *    myPayloadOff = "Off"
*/
 void MQTTcallback(char* topic, byte* payload, unsigned int length) {

  if(strcmp(topic, mqttPayloadRoot) >= 0){

    payload[length] = '\0';
    String message = (char*)payload;
      
    if (strstr(topic, mqttStatusTopicSubstring)) {
      //A Status message was received, so handle it


      
    } else if (strstr(topic, mqttCommandTopicSubstring)) {
      //A Command topic was received
      if (strstr(topic, mqttInternalPayloads)) {
        // Set Workstation item
        if (strstr(topic, mqttSetRelaysTopicSubstring)) {
          //Set the relay settings for the given workstation  Note that this also includes external devices controlled by MQTT
          //1st_Floor/Garage/Workstations/01/Cmnd/SetRelays
          char* workstationToEnd;
          char* workstation;
          workstationToEnd = strstr(topic, mqttInternalPayloads) + strlen(mqttInternalPayloads) + 1;
          int workstationID_Int = atoi(strtok(strstr(topic, mqttInternalPayloads) + strlen(mqttInternalPayloads) + 1, "/"));
          setWorkstationRelaysByMQTT(workstationID_Int - 1, payload, length);
          
//          controlButtonLEDs(workstationID_Int - 1);
        }
      }
    } else if (strstr(topic, mqttInternalPayloads)) {
      //An Internal Workstation message was received which is not a Command to set a value
      int i1 = strlen(topic);
      char* lastPayloadPart = strrchr(topic, '/');
      char* topicID = topic + (strlen(topic) - 2);
      int topicID_Int = atoi(topicID);
  
      boolean setState = LOW;
      if (message == myPayloadOn) {
        setState = HIGH;
      }

//      Serial.println("MQTT Message Received: ");
//      Serial.println(topic);
    
      controlDevices(topicID_Int, setState);
    
    }// else {
//      //It is something else
//
//
//
//    }

    //Reset the timer
    timer_1 = 0;

  }

}

/**
 *Set the devices to be controlled for a given Workstation 
 */
void setWorkstationRelaysByMQTT (unsigned int workstationGroup, byte* payload, unsigned int length) {
  
  payload[length] = '\0';
  char* ptr = strtok((char*)payload, ",");

  byte i = 0;
  //Set all the Items to be controlled for this station
  while (ptr) {
    relaysControlled[workstationGroup][i] = atoi(ptr);
    ptr = strtok(NULL, ",");
    i++;
  }

  //Fill the rest of the array with 0
  for (byte s = i; s < devicesControlled_arraysize + 1; s ++ ) {
    relaysControlled[workstationGroup][s] = 0;
  }
    
}


/**
 *Control the appropriate Relays and send the appropriate MQTT and CAN Bus messages 
 */
void controlDevices(unsigned int workstationGroup, boolean settingValue) {
//Serial.println(" controlDevices.");
  //Set the workstation status to the new setting.
  //Doing this now means if we are turning the relays off
  //  we ignore this workstation to see if any should be on.
  settingValue = !workstationsStatus[workstationGroup];
  workstationsStatus[workstationGroup] = settingValue;
//  workstationsStatus[workstationGroup] = !workstationsStatus[workstationGroup];
//Serial.println("Workstation Status ");
//Serial.print("Workstation: ");
//Serial.println(workstationGroup);
//Serial.print("Passed Setting: ");
//Serial.println(settingValue);
//Serial.print("Status: ");
//Serial.println(workstationsStatus[workstationGroup]);


  //First check for Workstations All as we handle this different than other devices
  //If the devices are being turned on, just turn on the apopropriate ones
    if ( workstationGroup == WORKSTATION_ALL) {
//      if (settingValue == true){
        // Loop through every workstation and turn it off
        for (int i = 0; i < workstations_arraysize; i ++){
          if (workstationsStatus[i]) {
            controlDevices(i, false);
          }
        }
        
//      } else {
//        
//      }
    } else if (settingValue == true){
    //Turn on the devices
//Serial.println("Turn it on");
    for (byte s = 0; s < (devicesControlled_arraysize-1); s ++){
      controlTheDevice(workstationGroup, s, settingValue);
    }
  } else {
    //Turn off the devices.
    //  Make sure we do not turn off any which are also in another workstation
    //    which is currently on.
    //  Check the workstations and get a list of the relays which should remain on
    //    These are devices which are controlled by another workstation.
    
    //Set an array to let us know which devices stay on.
//Serial.println("Turn it off");
    bool relaysStayOn[relaysControlled_arraysize + 1];
//    bool mqttStayOn[externalDevicesControlled_arraysize + 1];
    for (byte r = 0; r < relaysControlled_arraysize + 1; r ++) {
      relaysStayOn[r] = false;
    }
    //Run through all the workstations which are On and set the appropriate devicesStayOn to true
    //  for any which should stay on.
    for (byte ws = 0; ws < (workstations_arraysize-1); ws ++){
      if (workstationsStatus[ws] == true) {
        for (byte wr = 0; wr < (workstations_arraysize-1); wr ++){
            relaysStayOn[relaysControlled[ws][wr]] = true;
        }
      }
    }

    //control the appropriate devices.  devicesStayOn will be False if they should be turned on.
    //Look at each device in the workgroup being turned off.
    //  If the matching devicesStayOn value is False, then turn it off
    for (byte d = 0; d < relaysControlled_arraysize - 1; d ++){
        //Control a device
        if (relaysStayOn[relaysControlled[workstationGroup][d]]){
          //If it stays on, go to the next one
//Serial.println("Leave Me On: ");
//Serial.println(relaysControlled[workstationGroup][d]);          
          continue;
        }        
        controlTheDevice(workstationGroup, d, settingValue);
    }
  }
  //Set the appropriate button LEDs
  controlButtonLEDs(workstationGroup);
}

void controlTheDevice(unsigned int workstationGroup, unsigned int workstationItem, boolean settingValue){

    if(relaysControlled[workstationGroup-1][workstationItem] > 99) {
      // This is controlled by MQTT
      publishToMQTT(relaysControlled[workstationGroup-1][workstationItem], settingValue, false);
    } else if (relaysControlled[workstationGroup-1][workstationItem] > 0) {
      // This is controlled via a relay
//      display.setTextColor(YELLOW);
//      display.print("Cntrl Relay:");
//      display.println(relaysControlled[workstationGroup-1][workstationItem]);
//Serial.print("Relay: ");
//Serial.print(relaysControlled[workstationGroup-1][workstationItem] - 1);
//Serial.print("  Set To: ");
//Serial.println(settingValue);
      mcpRelays.digitalWrite(relaysControlled[workstationGroup-1][workstationItem] - 1, settingValue);
    }
}

void publishToMQTT (unsigned int mqttPayloadItem, boolean settingValue, boolean isInternal) {

//  mqttPayloadItem = mqttPayloadItem - 100;

  if (isInternal == false) {
    strcpy(topicBuffer, mqttPayloadRoot);
    mqttPayloadItem = mqttPayloadItem - 100;
    strcat(topicBuffer, mqttExternalPayloads[mqttPayloadItem]);
  } else {
    strcpy(topicBuffer, mqttPayloadRoot);
    strcat(topicBuffer, mqttInternalPayloads);
    strcat(topicBuffer, mqttPayloadDevices[mqttPayloadItem]);
  }

  if (settingValue == HIGH) {
    client.publish(topicBuffer, myPayloadOn);
  } else {
    client.publish(topicBuffer, myPayloadOff);
  }
  
//  display.setTextColor(GREEN);
//  display.print(topicBuffer);
//  delay(2000);
  
}

void reconnect() {
 // Loop until we're reconnected
 while (!client.connected()) {
// Serial.print("Attempting MQTT connection...");
//  display.fillScreen(BLACK);
//  display.setTextColor(RED);
//  display.setCursor(0,5);
//  display.print("Lost MQTT Connection");
//  display.print("Reconnecting...");


// Attempt to connect  client.connect(statusTopic, mqttUser, mqttPassword )
 if (client.connect(statusTopic)) {
    Serial.println("connected");
//    display.fillScreen(BLACK);
//    display.setTextColor(RED);
//    display.setCursor(0,5);
//    display.print("Reestablished MQTT Connection");
    // set callback and subscribe to topics
    client.setCallback(MQTTcallback);
    client.subscribe(mqttSubscription);
    delay(5000);
//    display.fillScreen(BLACK);
 } else {
//    display.fillScreen(BLACK);
//    display.setTextColor(RED);
//    display.setCursor(0,5);
//    display.print("MQTT Connection failed, rc=");
//    display.println();
//    display.println("rc=");
//    display.print(client.state());
//    display.println("Trying again in 5 seconds");
  
//    Serial.print("failed, rc=");
//    Serial.print(client.state());
//    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }
 }
}



void controlButtonLEDs(unsigned int workstationGroup) {
//Serial.print("Controlling Button LEDs");
//Serial.print(" workstationGroup = ");
//Serial.println(workstationGroup);

boolean settingValue = workstationsStatus[workstationGroup];

  for (int b = 0; b < buttonPanels; b ++) {
    //Get each Panel's Workstation Button definition
    for (int ws = 0; ws < buttonPanelMaxButtons; ws ++){
      //Get each Workstation for that panel
      if ( buttonPanelsSinglePressWorkstations[b][ws] == workstationGroup ) {
        //If there is a button which controls this workstation, set the button to the appropriate value
        canMsg.can_id  = unitID;
        canMsg.can_dlc = 4;               //CAN data length
        canMsg.data[0] = b;               //Device ID message is intended for 0xFF = Any
        canMsg.data[1] = 2;               //Message Content 1=button press 2=LED Control 3=?
        canMsg.data[2] = ws;              //If LED control: LED number
        canMsg.data[3] = settingValue;    //If LED control:  0=Off 1=On
//  canMsg.data[4] = 0x01;
//  canMsg.data[5] = 0x00;
//  canMsg.data[6] = 0x00;
//  canMsg.data[7] = 0xA0;
        mcp2515.sendMessage(&canMsg);     //Sends the CAN message
  delay(200); //Small delay
    
//  Serial.print("CAN to send: "); // print ID
//    Serial.print(canMsg.can_id, HEX); // print ID
//    Serial.print(" "); 
//    Serial.print(canMsg.can_dlc, HEX); // print DLC
//    Serial.print(" ");
//    
//    for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
//      Serial.print(canMsg.data[i],HEX);
//      Serial.print(" ");
//    }        
//Serial.println();

      }
    }
  }
}


void singlePressReceived (int myUnit, int myButton){
  controlDevices(buttonPanelsSinglePressWorkstations[myUnit][myButton], workstationsStatus[myUnit]);
}
void doublePressReceived (int myUnit, int myButton){
  controlDevices(buttonPanelsDoublePressWorkstations[myUnit][myButton], workstationsStatus[myUnit]);
}
void longPressReceived (int myUnit, int myButton){
  controlDevices(buttonPanelsLongPressWorkstations[myUnit][myButton], workstationsStatus[myUnit]);
}

void syncButtonLEDs(){
  // A device requested the status of the associated workstations
  int requestingPanel = canMsg.can_id;
  canMsg.can_id  = unitID;
  canMsg.can_dlc = 4;               //CAN data length
  canMsg.data[0] = requestingPanel;   //Device ID message is intended for 0xFF = Any
  canMsg.data[1] = 2;               //Message Content 1=button press 2=LED Control 3=?

//Serial.println("Syncing LEDs");

  //Loop through each button for this button panel
  for (int b = 0; b < buttonPanelMaxButtons; b ++) {
    int myStatus = 0x00;
    if (workstationsStatus[buttonPanelsSinglePressWorkstations[requestingPanel][b]] == true){
      myStatus = 0x01;
    }
    canMsg.data[2] = b;              //LED number = Button number
//    canMsg.data[3] = myStatus;    //If LED control:  0=Off 1=On
    canMsg.data[3] = workstationsStatus[buttonPanelsSinglePressWorkstations[requestingPanel][b]];
    mcp2515.sendMessage(&canMsg);     //Sends the CAN message
    delay(200);
  }
}

void canReceived(){
  // canMsg.can_id is the unit this message is coming from.
  //    For the Button Panels, this will be the same as the index used
  //      in the 3 buttonPanels____PressWorkstations arrays
  // check canMsg.data[1] to determine what the message relates to
  //    1=button press 2=LED Control 3=?
  // If it relates to a button press, use canMsg.data[2] to determine
  //    which button was pressed
  // the type of is defined in canMsg.data[3]
  //    1=single 2=double 3=long

  //Work on button press messages
  if (canMsg.data[1] == 1) {
    // A button was pressed

    switch (canMsg.data[3]) {
      case 1:
//      Serial.println("single");
//      Serial.print("canMsg.data[2] aka Button # = ");
//      Serial.println(canMsg.data[2]);
        singlePressReceived(canMsg.can_id, canMsg.data[2]);
        break;
      case 2:
//      Serial.println("double");
        doublePressReceived(canMsg.can_id, canMsg.data[2]);
        break;
      case 3:
//      Serial.println("long");
        longPressReceived(canMsg.can_id, canMsg.data[2]);
        break;
    }
  } else if (canMsg.data[1] == 3) {
    // A device requested the LED status be sent to sync the buttons
    syncButtonLEDs();
  }
}

void checkCanMessage(){
    
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    display.clearDisplay();
    display.setTextSize(2);      // 2x text
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
//    display.cp437(true);         // Use full 256 char 'Code Page 437' font
    display.println(F("CAN Rcv'd"));
    display.setTextSize(1);             // Normal 1:1 pixel scale
//    display.setCursor(10, 0);
    display.print("  "); 
    display.print(canMsg.can_id); // print ID
    display.print(" "); 
    display.print(canMsg.can_dlc); // print DLC
    display.print(" ");
    
    for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
      display.print(canMsg.data[i]);
      display.print(" ");
    }
    display.display();
    delay(1500);
    display.clearDisplay();
    display.display();
        
//    Serial.print("Message Received: ");
//    Serial.print(canMsg.can_id, HEX); // print ID
//    Serial.print(" "); 
//    Serial.print(canMsg.can_dlc, HEX); // print DLC
//    Serial.print(" ");
//    
//    for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
//      Serial.print(canMsg.data[i]);
//      Serial.print(" ");
//    }
//    Serial.println();  
//    int canUnit = canMsg.data[0];
//    int canUnit = 0xFF;

    if (canMsg.data[0] == unitID || canMsg.data[0] == 0xFF) {
//    if (canUnit == 0xFF) {
      //This is meant for us or anyone, so act on it
      //This unit will currently act on all messages meant for anyone as they are just button presses
      canReceived();
//      controlButtonLEDs(8);
    }

    Serial.println();      
  }
}


void loop(){
  if (!client.connected()) {
    reconnect();
 }

   client.loop();

  checkCanMessage();

  // set millisTick at the top of each loop if and only if millis() has changed
  unsigned long deltaMillis = 0; // clear last result
  unsigned long thisMillis = millis();
  // do this just once to prevent getting different answers from multiple calls to   millis()
  if (thisMillis != lastMillis) {
  // we have ticked over
  // calculate how many millis have gone past
  deltaMillis = thisMillis-lastMillis; // note this works even if millis() has rolled over back to 0
  lastMillis = thisMillis;
}

  // handle repeating timer
  // repeat this code for each timer you need to handle
  timer_1 += deltaMillis;
  if (TIMER_INTERVAL_1 <= timer_1) {
    // reset timer since this is a repeating timer
    timer_1 = 0; // note this prevents the delay accumulating if we miss a mS or two
    // if we want exactly 1000 delay to next time even if this one was late then just use timeOut = 1000;

  }

////bme.readTemperature() - reads the temperature in celsius
////bme.readAltitude(1013.25) - calculates altitude in meter based on the sea level pressure defined
////bme.readPressure() - reads pressure in hPA (hectoPascal = millibar)
//    display.clearDisplay();
//    display.setTextSize(1);      // 2x text
//    display.setTextColor(SSD1306_WHITE); // Draw white text
//    display.setCursor(0, 0);     // Start at top-left corner
//    
//    display.print(F("Temperature = "));
//    display.print(bme.readTemperature());
//    display.println(" *C");
//    display.print(F("Pressure = "));
//    display.print(bme.readPressure() / 100.0F);
//    display.println(" hPa");
//    display.print(F("Approx altitude = "));
//    display.print(bme.readAltitude(1013.25)); /* Adjusted to local forecast! */
//    display.println(" m");
//    display.println();
//    display.print("Humidity = ");
//    display.print(bme.readHumidity());
//    display.println("%");
//    display.println();
//    display.display();


       
  delay(200); //Small delay

}
