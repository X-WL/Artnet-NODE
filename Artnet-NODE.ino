/*
  Name:		ARTNET_NODE.ino
  Created:	3/18/2018 4:34:59 PM
  Author:	xwolk
*/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <FS.h> //FileSystem
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "artnet_node.h"
#include "common.h"          // definitions from libartnet
#include "packets.h"
#include "FastLED.h"
#include "web_server.h"
#include <Preferences.h>


struct NetworkConfig
{
  uint8_t Mac[6];
  uint8_t IP_Local[4];           // the IP address of node
  uint8_t IP_Broadcast[4];           // broadcast IP address
  uint8_t IP_Gateway[4];           // gateway IP address (use ip address of controller)
  uint8_t SubnetMask[4];           // network mask (art-net use 'A' network type)
  uint8_t IP_dns[4];           // TODO DNS
};

struct ARTNODE_Settings
{
  uint8_t Brightness = 255;
  uint8_t Target_FPS = 60;
  bool Auto_Reboot = false;
  bool Use_OSC = false;
  uint8_t OSC_Port = 222;
  const char* ssid = "";
  const char* password = "";
  NetworkConfig NetConf;
};

WiFiUDP Udp;
Preferences preferences;
AsyncWebServer server(80);

#pragma region Settings

const char* ssid = "X-WL Wireless";
const char* password = "wolk244521240792";

uint8_t factory_mac[6]; // the mac address of node
uint8_t factory_localIp[4];           // the IP address of node
uint8_t factory_broadcastIp[4];           // broadcast IP address
uint8_t factory_gateway[4];           // gateway IP address (use ip address of controller)
uint8_t factory_subnetMask[4];           // network mask (art-net use 'A' network type)

uint8_t factory_dns[4];           // TODO DNS

uint8_t factory_swin[4] = { 0,   1,   2,   3};
uint8_t factory_swout[4] = { 0,   1,   2,   3 };

ARTNODE_Settings tempPageSettings;

artnet_node_t             ArtNode;
artnet_reply_t            ArtPollReply;
//artnet_ipprog_reply_t   ArtIpprogReply; //not implemented yet
artnet_packet_type_t      packet_type;

const int MAX_BUFFER_UDP = 1650;	// For Arduino MEGA

uint8_t packetBuffer[MAX_BUFFER_UDP];             //buffer to store incoming data (volatile?)
byte Remote_IP[4] = { 2, 255, 255, 255 };
uint16_t tRemotePort = 6454;

#define DATA_PIN1    23
#define DATA_PIN2    21
#define DATA_PIN3    19
#define DATA_PIN4    18

#define DATA_PIN5    5
#define DATA_PIN6    4
#define DATA_PIN7    2
#define DATA_PIN8    15

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

#define NUM_STRIPS 8
#define NUM_LEDS_PER_STRIP 170

#define NUM_LEDS NUM_LEDS_PER_STRIP * NUM_STRIPS
CRGB leds[NUM_LEDS];

// -- The core to run FastLED.show()
#define FASTLED_SHOW_CORE 0

// -- NVS SAVE 
bool ARTNODE_MODE = true;
// -- 
uint8_t ARTNODE_BRIGHT = 100; //%
uint8_t ARTNODE_TARGET_FPS = 60;
uint16_t ARTNODE_IO[8] = {0,1,2,3,4,5,6,7};
uint8_t ARTNODE_OSC_PORT = 222;

bool ARTNODE_AUTO_REBOOT = false;
bool ARTNODE_USE_OSC = false;

uint16_t ARTNODE_STD_SCENE = 1;
uint8_t ARTNODE_STD_PAGE = 0;
uint16_t ARTNODE_STD_SPEED = 60;
uint16_t ARTNODE_STD_LAST_SPEED = 60;
uint8_t ARTNODE_STD_DIM = 100; //%

// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

int RefreshTime = 1000 / ARTNODE_TARGET_FPS;
float SpeedModif = 1;
bool is_Need_STD_Render = false;

float STD_PARAMS[4] = {0,0,0,0};

uint32_t lastReply = 0;
uint32_t lastUpdate = 0;
uint32_t lastShowFPS = 0;
uint16_t STAT_IO_COUNT[8] = {0,0,0,0,0,0,0,0};
uint16_t STAT_FPS = 0;
#pragma endregion

void setup() {
  Serial.begin(115200);
  preferences.begin("settings",false);
  ReadNVS();
  fill_art_node(&ArtNode);
  ArtNode.numbports = 8;
  while (ConnectWiFi() == false) {
  }
  Serial.println("Start NODE");
  FastLED.addLeds<LED_TYPE, DATA_PIN1, COLOR_ORDER>(leds, 0, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN2, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN3, COLOR_ORDER>(leds, 2 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN4, COLOR_ORDER>(leds, 3 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);

  FastLED.addLeds<LED_TYPE, DATA_PIN5, COLOR_ORDER>(leds, 4 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN6, COLOR_ORDER>(leds, 5 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN7, COLOR_ORDER>(leds, 6 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN8, COLOR_ORDER>(leds, 7 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);

  int core = xPortGetCoreID();
  Serial.print("Main code running on core ");
  Serial.println(core);

  // -- Create the FastLED show task
  xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);

  fill_art_poll_reply(&ArtPollReply, &ArtNode);
  //fill_art_ipprog_reply  (&ArtIpprogReply, &ArtNode);

  send_reply(BROADCAST, (uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
  /*------WEB SERVER-----*/
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    String value;
    if (request->hasParam("dim")) {
      value = request->getParam("dim")->value();
      ARTNODE_STD_DIM = value.toInt();
      Serial.print("DIM: ");
      Serial.println(value);
      CorrectBri();
    } 
    else if (request->hasParam("mode")) {
      value = request->getParam("mode")->value();
      if (value.toInt() == 1){       
        ARTNODE_MODE = true;
        CorrectBri();
      }
      else {        
        ARTNODE_MODE = false;
        CorrectBri();
      }
      Serial.print("MODE: ");
      Serial.println(value);
    }
    else if (request->hasParam("scn")) {
      value = request->getParam("scn")->value();
      ARTNODE_STD_SCENE = value.toInt();
      Serial.print("SCENE: ");
      Serial.println(ARTNODE_STD_SCENE);
    }
    else if (request->hasParam("page")) {
      value = request->getParam("page")->value();
      if (value == "+") {
        ARTNODE_STD_PAGE++;
      }
      else if (value == "0") {
        ARTNODE_STD_PAGE = 0; 
      }
      else if (ARTNODE_STD_PAGE > 0) {
        ARTNODE_STD_PAGE--;  
      }
      Serial.print("PAGE: ");
      Serial.println(ARTNODE_STD_PAGE + 1);
    }
    else if (request->hasParam("speed")){
      value = request->getParam("speed")->value();
      if (value == "*") {
        ARTNODE_STD_SPEED*=2;
      }
      else if (value == "+") {
        ARTNODE_STD_SPEED++;
      }
      else if (value == "0") {
        ARTNODE_STD_SPEED=60;
      }
      else if (value == "-") {
        if (ARTNODE_STD_SPEED > 1){
          ARTNODE_STD_SPEED--;
        }
      }
      else if (value == "/") {
        if (ARTNODE_STD_SPEED > 1) {
          ARTNODE_STD_SPEED=ceil(ARTNODE_STD_SPEED/2);
        }
      }
      Serial.print("SPEED: ");
      Serial.println(ARTNODE_STD_SPEED);
      RecalcSpeed();
    }
    IndexGen(request);
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest * request) {
    String value;
    
    if (request->hasParam("restart")) {
      value = request->getParam("restart")->value();
      SettingsGen(request);
      if (value == "1"){
        delay(2000);
        ESP.restart();
      }
    }
    if (request->hasParam("apply")) {
      value = request->getParam("apply")->value();
      if (value == "1"){
        ARTNODE_BRIGHT = tempPageSettings.Brightness;
        ARTNODE_TARGET_FPS = tempPageSettings.Target_FPS;
        ARTNODE_AUTO_REBOOT = tempPageSettings.Auto_Reboot;
        ARTNODE_USE_OSC = tempPageSettings.Use_OSC;
        ARTNODE_OSC_PORT = tempPageSettings.OSC_Port;
        /*
        if (ssid !=  tempPageSettings.ssid) {
            ssid = tempPageSe;
            is_wifiChange = true;
            Serial.println("ssid change");
          }
        */
        ssid = tempPageSettings.ssid;
        password = tempPageSettings.password;
        
        //factory_mac = tempPageSettings.NetConf.Mac;
        for(int i = 0; i < 4; i++)
        {
          factory_localIp[i] = tempPageSettings.NetConf.IP_Local[i];
          factory_broadcastIp[i] = tempPageSettings.NetConf.IP_Broadcast[i];
          factory_gateway[i] = tempPageSettings.NetConf.IP_Gateway[i];
          factory_subnetMask[i] = tempPageSettings.NetConf.SubnetMask[i];
          factory_dns[i] = tempPageSettings.NetConf.IP_dns[i];
        }

        CorrectBri();
        RefreshTime = 1000 / ARTNODE_TARGET_FPS;
      }
      SettingsGen(request);
      bool isWriteNVS = WriteNVS();
    }

    if (request->hasParam("c")) {
      value = request->getParam("c")->value();
      if (value == "1"){
        ApplyRestartGen(request);
      }
    }  

    if (request->hasParam("c")) {
      value = request->getParam("c")->value();
      if (value == "3"){ 
        Serial.println("c-3");      
        if (request->hasParam("i_bri")) {
          value = request->getParam("i_bri")->value();
          tempPageSettings.Brightness = value.toInt();
        }
        if (request->hasParam("i_fps")) {
          value = request->getParam("i_fps")->value();
          tempPageSettings.Target_FPS = value.toInt();
          
        }  
        if (request->hasParam("c_ar")) {
          value = request->getParam("c_ar")->value();
          if (value == "yes"){       
            tempPageSettings.Auto_Reboot = true;
          }
        }
        if (request->hasParam("c_osc")) {
          value = request->getParam("c_osc")->value();
          if (value == "yes"){       
            tempPageSettings.Use_OSC = true;
          }
        }
        if (request->hasParam("i_osc")) {
          value = request->getParam("i_osc")->value();
          tempPageSettings.OSC_Port = value.toInt();
          ///OSC RECONF
        }
        if (request->hasParam("ssid")) {
          value = request->getParam("ssid")->value();
          unsigned char* buf = new unsigned char[100];
          value.getBytes(buf, 100, 0);
          tempPageSettings.ssid = (const char*)buf;
        }
        if (request->hasParam("pass")) {
          value = request->getParam("pass")->value();
          unsigned char* buf = new unsigned char[100];
          value.getBytes(buf, 100, 0);
          tempPageSettings.password = (const char*)buf;
        }
        //LOCAL IP
        if (request->hasParam("ip_l1")) {
          value = request->getParam("ip_l1")->value();
          tempPageSettings.NetConf.IP_Local[0] = value.toInt();
        } 
        if (request->hasParam("ip_l2")) {
          value = request->getParam("ip_l2")->value();
          tempPageSettings.NetConf.IP_Local[1] = value.toInt();
        }
        if (request->hasParam("ip_l3")) {
          value = request->getParam("ip_l3")->value();
          tempPageSettings.NetConf.IP_Local[2] = value.toInt();
        }
        if (request->hasParam("ip_l4")) {
          value = request->getParam("ip_l4")->value();
          tempPageSettings.NetConf.IP_Local[3] = value.toInt();
        }
        //SUBNET MASK
        if (request->hasParam("ip_s1")) {
          value = request->getParam("ip_s1")->value();
          tempPageSettings.NetConf.SubnetMask[0] = value.toInt();
        } 
        if (request->hasParam("ip_s2")) {
          value = request->getParam("ip_s2")->value();
          tempPageSettings.NetConf.SubnetMask[1] = value.toInt();
        }
        if (request->hasParam("ip_s3")) {
          value = request->getParam("ip_s3")->value();
          tempPageSettings.NetConf.SubnetMask[2] = value.toInt();
        }
        if (request->hasParam("ip_s4")) {
          value = request->getParam("ip_s4")->value();
          tempPageSettings.NetConf.SubnetMask[3] = value.toInt();
        }
        //GATEWAY
        if (request->hasParam("ip_g1")) {
          value = request->getParam("ip_g1")->value();
          tempPageSettings.NetConf.IP_Gateway[0] = value.toInt();
        } 
        if (request->hasParam("ip_g2")) {
          value = request->getParam("ip_g2")->value();
          tempPageSettings.NetConf.IP_Gateway[1] = value.toInt();
        }
        if (request->hasParam("ip_g3")) {
          value = request->getParam("ip_g3")->value();
          tempPageSettings.NetConf.IP_Gateway[2] = value.toInt();
        }
        if (request->hasParam("ip_g4")) {
          value = request->getParam("ip_g4")->value();
          tempPageSettings.NetConf.IP_Gateway[3] = value.toInt();
        }
        //BROADCAST
        if (request->hasParam("ip_b1")) {
          value = request->getParam("ip_b1")->value();
          tempPageSettings.NetConf.IP_Broadcast[0] = value.toInt();
        } 
        if (request->hasParam("ip_b2")) {
          value = request->getParam("ip_b2")->value();
          tempPageSettings.NetConf.IP_Broadcast[1] = value.toInt();
        }
        if (request->hasParam("ip_b3")) {
          value = request->getParam("ip_b3")->value();
          tempPageSettings.NetConf.IP_Broadcast[2] = value.toInt();
        }
        if (request->hasParam("ip_b4")) {
          value = request->getParam("ip_b4")->value();
          tempPageSettings.NetConf.IP_Broadcast[3] = value.toInt();
        }
        ApplySaveGen(request);
      }
    } 
    else {
      SettingsGen(request);
    } 
  });

  //////////////////////////////////
  // Send a GET request to <IP>/get?message=<message>
  CorrectBri();
  Serial.println("Start server!");
  server.begin();
  Serial.println("Start working!");
}

void IndexGen(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server", "ESP Artnet NODE");
  response->printf(HEADER_BEGIN);
  response->printf(STYLE);
  response->printf(STYLEID);
  response->printf(STYLECLASS);
  response->printf(HEADER_END);
  response->printf("<div class=\"wrapper\">");
  response->printf("<div class=\"header\">");
  response->printf("<h1 id=\"tcntr\">ARTNODE 8U</h1>");
  response->print("<h2 id=\"tcntr\" style=\"color: #EEEEEE;\">Hello, ");
  response->print(request->client()->remoteIP());
  response->printf("</h2><hr></div>");
  /////////////////////////////////////
  response->printf("<div class=\"content\">");
  response->printf("<form>");
  if (ARTNODE_MODE) {
    response->printf("<button id=\"btn_on\"  type=\"submit\" name=\"mode\" value=\"1\">Artnet</button>");
    response->printf("<button id=\"btn_off\"  type=\"submit\" name=\"mode\" value=\"0\">Standalone</button>");
  }
  else {
    response->printf("<button id=\"btn_off\"  type=\"submit\" name=\"mode\" value=\"1\">Artnet</button>");
    response->printf("<button id=\"btn_on\"  type=\"submit\" name=\"mode\" value=\"0\">Standalone</button>");
  }
  /*********************Layer 1***********************/
  if (ARTNODE_STD_DIM == 100) {
    response->printf("<button id=\"btn_sys_hi\"  type=\"submit\" name=\"dim\" value=\"100\">100%</button>");
  }
  else {
    response->printf("<button id=\"btn_sys\"  type=\"submit\" name=\"dim\" value=\"100\">100%</button>");
  }
  for (int i = 25*ARTNODE_STD_PAGE+1; i < 25*ARTNODE_STD_PAGE+6; i++) {
    if (ARTNODE_STD_SCENE == i) {
      response->printf("<button id=\"btn_nav_hi\" type=\"submit\" name=\"scn\" value=\"");
    }
    else {
      response->printf("<button id=\"btn_nav\" type=\"submit\" name=\"scn\" value=\"");
    }
    response->printf("%d",i);
    response->printf("\">");
    response->printf("%d",i);
    response->printf("</button>");
  }
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"page\" value=\"+\">Page +</button>");
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"speed\" value=\"*\">Speed x2</button>");
  /*********************Layer 2***********************/
  if (ARTNODE_STD_DIM == 75) {
    response->printf("<button id=\"btn_sys_hi\"  type=\"submit\" name=\"dim\" value=\"75\">75%</button>");
  }
  else {
    response->printf("<button id=\"btn_sys\"  type=\"submit\" name=\"dim\" value=\"75\">75%</button>");
  }
  for (int i = 25*ARTNODE_STD_PAGE+6; i < 25*ARTNODE_STD_PAGE+11; i++) {
    if (ARTNODE_STD_SCENE == i) {
      response->printf("<button id=\"btn_nav_hi\" type=\"submit\" name=\"scn\" value=\"");
    }
    else {
      response->printf("<button id=\"btn_nav\" type=\"submit\" name=\"scn\" value=\"");
    }
    response->printf("%d",i);
    response->printf("\">");
    response->printf("%d",i);
    response->printf("</button>");
  }
  response->printf("<button id=\"btn_txt\" type=\"submit\" name=\"int\" value=\"0\">Page: ");
  response->printf("%d",ARTNODE_STD_PAGE+1);
  response->printf("</button>");
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"speed\" value=\"+\">Speed +</button>");
  /*********************Layer 3***********************/
  if (ARTNODE_STD_DIM == 50) {
    response->printf("<button id=\"btn_sys_hi\"  type=\"submit\" name=\"dim\" value=\"50\">50%</button>");
  }
  else {
    response->printf("<button id=\"btn_sys\"  type=\"submit\" name=\"dim\" value=\"50\">50%</button>");
  }
  for (int i = 25*ARTNODE_STD_PAGE+11; i < 25*ARTNODE_STD_PAGE+16; i++) {
    if (ARTNODE_STD_SCENE == i) {
      response->printf("<button id=\"btn_nav_hi\" type=\"submit\" name=\"scn\" value=\"");
    }
    else {
      response->printf("<button id=\"btn_nav\" type=\"submit\" name=\"scn\" value=\"");
    }
    response->printf("%d",i);
    response->printf("\">");
    response->printf("%d",i);
    response->printf("</button>");
  }
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"page\" value=\"0\">Reset Page</button>");
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"speed\" value=\"0\">Speed INIT</button>");
  /*********************Layer 4***********************/
  if (ARTNODE_STD_DIM == 25) {
    response->printf("<button id=\"btn_sys_hi\"  type=\"submit\" name=\"dim\" value=\"25\">25%</button>");
  }
  else {
    response->printf("<button id=\"btn_sys\"  type=\"submit\" name=\"dim\" value=\"25\">25%</button>");
  }
  for (int i = 25*ARTNODE_STD_PAGE+16; i < 25*ARTNODE_STD_PAGE+21; i++) {
    if (ARTNODE_STD_SCENE == i) {
      response->printf("<button id=\"btn_nav_hi\" type=\"submit\" name=\"scn\" value=\"");
    }
    else {
      response->printf("<button id=\"btn_nav\" type=\"submit\" name=\"scn\" value=\"");
    }
    response->printf("%d",i);
    response->printf("\">");
    response->printf("%d",i);
    response->printf("</button>");
  }
  response->printf("<button id=\"btn_txt\" type=\"submit\" name=\"int\" value=\"0\">Speed: ");
  response->printf("%d",ARTNODE_STD_SPEED);
  response->printf(" BPM</button>");
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"speed\" value=\"-\">Speed -</button>");
  /*********************Layer 5***********************/
  if (ARTNODE_STD_DIM == 0) {
    response->printf("<button id=\"btn_sys_hi\"  type=\"submit\" name=\"dim\" value=\"0\">0%</button>");
  }
  else {
    response->printf("<button id=\"btn_sys\"  type=\"submit\" name=\"dim\" value=\"0\">0%</button>");
  }
  for (int i = 25*ARTNODE_STD_PAGE+21; i < 25*ARTNODE_STD_PAGE+26; i++) {
    if (ARTNODE_STD_SCENE == i) {
      response->printf("<button id=\"btn_nav_hi\" type=\"submit\" name=\"scn\" value=\"");
    }
    else {
      response->printf("<button id=\"btn_nav\" type=\"submit\" name=\"scn\" value=\"");
    }
    response->printf("%d",i);
    response->printf("\">");
    response->printf("%d",i);
    response->printf("</button>");
  }
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"page\" value=\"-\">Page -</button>");
  response->printf("<button id=\"btn_sys\" type=\"submit\" name=\"speed\" value=\"/\">Speed /2</button>");
  /*********************Layer END***********************/
  /*********************Footer***********************/
  response->printf("</form></div>");
  response->printf("<div class=\"footer\">");
  response->printf("<form action=\"/\">");
  response->printf("<button formaction=\"settings\" id=\"btn_sys\" style=\"width: 25%%\">Settings</button>");
  response->printf("</form>");
  response->printf("<div id=\"tcntr\" style=\"margin: 17px 0\">");
  response->printf("<span id=\"txtw\">Artnet Node 2018<br></span>");
  response->printf("<span id=\"txtw\">Programming by X-WL</span>");
  response->printf("</div></div></div>");
  
  response->print("</body></html>");
  //send the response last
  request->send(response);
}

void SettingsGen(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server", "ESP Artnet NODE");
  response->printf(HEADER_BEGIN);
  response->printf(CSS_SETTINGS);
  response->printf(CSS_SETTINGS_ID);
  response->printf(CSS_SETTINGS_CLASS);
  response->printf(HEADER_END);
  response->printf("<div class=\"wrapper\">");
  response->printf("<div class=\"header\">");
  response->printf("<h1 id=\"tcntr\">ARTNODE 8U</h1>");
  response->print("<h2 id=\"tcntr\" style=\"color: #EEEEEE;\">Hello, ");
  response->print(request->client()->remoteIP());
  response->printf("</h2><hr></div>");
  /////////////////////////////////////
  response->printf("<div class=\"content\">");
  response->printf("<form>");
  response->printf("<button formaction=\"output\" class=\"btn_nav\" style=\"width:100%%\">Output Mapping</button>");
  response->printf("</form>");
  response->printf("<h3 id=\"tcntr\">Settings</h3> ");
  response->printf("<form style=\"text-align:center\">");
  response->printf("<table><tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">Brightness: </span><span id=\"txtw\" style=\"font-size:12px; color:#BBBBBB;\">%% </span></td>");
  response->printf("<td id=\"tdr\"><input name=\"i_bri\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
  response->printf("%d",(ARTNODE_BRIGHT));
  response->printf("\"></td></tr><tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">Target FPS: </span></td>");
  response->printf("<td id=\"tdr\"><input name=\"i_fps\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
  response->printf("%d",(ARTNODE_TARGET_FPS));
  response->printf("\"></td></tr><tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">Auto Reboot: </span></td>");
  response->printf("<td id=\"tdr\"><input name=\"c_ar\" type=\"checkbox\" value=\"yes\"");
  if (ARTNODE_AUTO_REBOOT) {
    response->printf(" checked");
  }
  response->printf("></td></tr></table>");

  response->printf("<h4 id=\"tcntr\">OSC</h4>");
  response->printf("<table><tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">Use OSC: </span></td>");
  response->printf("<td id=\"tdr\"><input name=\"c_osc\" type=\"checkbox\" value=\"yes\"");
  if (ARTNODE_USE_OSC) {
    response->printf(" checked");
  }
  response->printf("></td></tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">OSC Port: </span></td>");
  response->printf("<td id=\"tdr\"><input name=\"i_osc\" maxlength=\"5\" style=\"width: 45px;\" value=\"");
  response->printf("%d",ARTNODE_OSC_PORT);
  response->printf("\"></td></tr></table>");

  response->printf("<h4 id=\"tcntr\">Network</h4>");
  response->printf("<table><tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">SSID: </span></td>");
  response->printf("<td id=\"tdr\"><input name=\"ssid\" style=\"width: 230px;\" value=\"");
  response->printf(ssid);
  response->printf("\"></td></tr><tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">Password: </span></td>");
  response->printf("<td id=\"tdr\"><input name=\"pass\" style=\"width: 230px;\" value=\"");
  response->printf(password);
  response->printf("\"></td></tr><tr></tr><tr>");
  response->printf("<td id=\"tdl\"><span id=\"txtw\">MAC Address: </span></td>");
  response->printf("<td id=\"tdr\"><span id=\"txtw\" style=\"color: #BBBBBB\">");
  for(int i = 0; i < 5; i++)
  {
    response->printf("%d",factory_mac[i]);
    response->printf(":");
  }
  response->printf("%d",factory_mac[6]);
  response->printf("</span></td></tr><tr>");

  response->printf("<td id=\"tdl\"><span id=\"txtw\">IP Address: </span></td>");
  response->printf("<td id=\"tdr\">");
  for(int i = 0; i < 3; i++)
  {
    response->printf("<input name=\"ip_l");
    response->printf("%d",i + 1);
    response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
    response->printf("%d",factory_localIp[i]);
    response->printf("\"><span id=\"txtdot\"> .</span>");
  }
  response->printf("<input name=\"ip_l");
  response->printf("%d",4);
  response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
  response->printf("%d",factory_localIp[3]);
  response->printf("\"</tr><tr>");
  
  response->printf("<td id=\"tdl\"><span id=\"txtw\">Subnet Mask: </span></td>");
  response->printf("<td id=\"tdr\">");
  for(int i = 0; i < 3; i++)
  {
    response->printf("<input name=\"ip_s");
    response->printf("%d",i + 1);
    response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
    response->printf("%d",factory_subnetMask[i]);
    response->printf("\"><span id=\"txtdot\"> .</span>");
  }
  response->printf("<input name=\"ip_s");
  response->printf("%d",4);
  response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
  response->printf("%d",factory_subnetMask[3]);
  response->printf("\"</td></tr><tr>");

  response->printf("<td id=\"tdl\"><span id=\"txtw\">Gateway: </span></td>");
  response->printf("<td id=\"tdr\">");
  for(int i = 0; i < 3; i++)
  {
    response->printf("<input name=\"ip_g");
    response->printf("%d",i + 1);
    response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
    response->printf("%d",factory_gateway[i]);
    response->printf("\"><span id=\"txtdot\"> .</span>");
  }
  response->printf("<input name=\"ip_g");
  response->printf("%d",4);
  response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
  response->printf("%d",factory_gateway[3]);
  response->printf("\"</td></tr><tr>");

  response->printf("<td id=\"tdl\"><span id=\"txtw\">Broadcast IP: </span></td>");
  response->printf("<td id=\"tdr\">");
  for(int i = 0; i < 3; i++)
  {
    response->printf("<input name=\"ip_b");
    response->printf("%d",i + 1);
    response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
    response->printf("%d",factory_broadcastIp[i]);
    response->printf("\"><span id=\"txtdot\"> .</span>");
  }
  response->printf("<input name=\"ip_b");
  response->printf("%d",4);
  response->printf("\" maxlength=\"3\" style=\"width: 45px;\" value=\"");
  response->printf("%d",factory_broadcastIp[3]);
  response->printf("\"</td></tr><tr></tr></table>");

  response->printf("<button class=\"btn_nav\" style=\"width:33%%\" type=\"submit\" name=\"c\" value=\"1\">Reboot Device</button>");
  response->printf("<button class=\"btn_nav\" style=\"width:34%%\" type=\"submit\" name=\"c\" value=\"2\">Reset to Default</button>");
  response->printf("<button class=\"btn_nav\" style=\"width:33%%\" type=\"submit\" name=\"c\" value=\"3\">Save & Apply</button>");
  /*********************Footer***********************/
  response->printf("</form></div>");
  response->printf("<div class=\"footer\">");
  response->printf("<form>");
  response->printf("<button formaction=\"/\" class=\"btn_sys\" style=\"width: 25%%\">Main</button>");
  response->printf("</form>");
  response->printf("<div id=\"tcntr\" style=\"margin: 17px 0\">");
  response->printf("<span id=\"txtw\">Artnet Node 2018<br></span>");
  response->printf("<span id=\"txtw\">Programming by X-WL</span>");
  response->printf("</div></div></div>");
  
  response->print("</body></html>");
  //send the response last
  request->send(response);
}

void ApplySaveGen(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server", "ESP Artnet NODE");
  response->printf(HEADER_BEGIN);
  response->printf(CSS_APPLY);
  response->printf(CSS_APPLY_ID);
  response->printf(CSS_APPLY_CLASS);
  response->printf(HEADER_END);
  response->printf("<div class=\"wrapper\">");
  response->printf("<div class=\"header\">");
  response->printf("<h1 id=\"tcntr\">ARTNODE 8U</h1>");
  response->print("<h2 id=\"tcntr\" style=\"color: #EEEEEE;\">Hello, ");
  response->print(request->client()->remoteIP());
  response->printf("</h2><hr></div>");
  /////////////////////////////////////
  response->printf("<div class=\"content\">");
  response->printf("<br><h2 id=\"tcntr\">Save new settings?</h2><br>");
  response->printf("<br><h3 id=\"tcntr\" style=\"color: #DDDDDD;\">*Need to restart to apply changes!*</h3><br>");
  response->printf("<form action=\"/settings\">");
  response->printf("<button class=\"btn_nav\" style=\"width:50%%\" type=\"submit\">Back</button>");
  response->printf("<button class=\"btn_nav\" style=\"width:50%%\" type=\"submit\" name=\"apply\" value=\"1\">Save</button>");
  response->printf("</form></div>");
  /*********************Footer***********************/
  response->printf("<div class=\"footer\">");
  response->printf("<form>");
  response->printf("<button formaction=\"/\" class=\"btn_sys\" style=\"width: 25%%\">Main</button>");
  response->printf("</form>");
  response->printf("<div id=\"tcntr\" style=\"margin: 17px 0\">");
  response->printf("<span id=\"txtw\">Artnet Node 2018<br></span>");
  response->printf("<span id=\"txtw\">Programming by X-WL</span>");
  response->printf("</div></div></div>");
  
  response->print("</body></html>");
  //send the response last
  request->send(response);
}

void ApplyRestartGen(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server", "ESP Artnet NODE");
  response->printf(HEADER_BEGIN);
  response->printf(CSS_APPLY);
  response->printf(CSS_APPLY_ID);
  response->printf(CSS_APPLY_CLASS);
  response->printf(HEADER_END);
  response->printf("<div class=\"wrapper\">");
  response->printf("<div class=\"header\">");
  response->printf("<h1 id=\"tcntr\">ARTNODE 8U</h1>");
  response->print("<h2 id=\"tcntr\" style=\"color: #EEEEEE;\">Hello, ");
  response->print(request->client()->remoteIP());
  response->printf("</h2><hr></div>");
  /////////////////////////////////////
  response->printf("<div class=\"content\">");
  response->printf("<br><h2 id=\"tcntr\">Restart your device?</h2><br>");
  response->printf("<br><h3 id=\"tcntr\" style=\"color: #DDDDDD;\">*Please wait 10 seconds...*</h3><br>");
  response->printf("<form action=\"/settings\">");
  response->printf("<button class=\"btn_nav\" style=\"width:50%%\" type=\"submit\">Back</button>");
  response->printf("<button class=\"btn_nav\" style=\"width:50%%\" type=\"submit\" name=\"restart\" value=\"1\">Restart</button>");
  response->printf("</form></div>");
  /*********************Footer***********************/
  response->printf("<div class=\"footer\">");
  response->printf("<form>");
  response->printf("<button formaction=\"/\" class=\"btn_sys\" style=\"width: 25%%\">Main</button>");
  response->printf("</form>");
  response->printf("<div id=\"tcntr\" style=\"margin: 17px 0\">");
  response->printf("<span id=\"txtw\">Artnet Node 2018<br></span>");
  response->printf("<span id=\"txtw\">Programming by X-WL</span>");
  response->printf("</div></div></div>");
  
  response->print("</body></html>");
  //send the response last
  request->send(response);
}

void RecalcSpeed(){
    SpeedModif = (float)ARTNODE_STD_SPEED/(float)ARTNODE_TARGET_FPS;  
    Serial.println("SpeedModif = ");
    Serial.println(SpeedModif);
}

void loop() {
  if (ARTNODE_MODE == false) {
    if (is_Need_STD_Render) {
      
      is_Need_STD_Render = false;
    }
    
  } else {
    if (millis() - lastReply > 3000){
      send_reply(BROADCAST, (uint8_t *)&ArtPollReply, sizeof(ArtPollReply));  //Need FIX Art_Poll, Art_Replay
      lastReply = millis();													// Remote IP/Port
    }
    int l = Udp.parsePacket();
    if (l > 0) {
      handle_packet();
    }      
  }
  if (millis() - lastUpdate > RefreshTime)
  {
    lastUpdate = millis();
    FastLED.show();
    is_Need_STD_Render= true;
    STAT_FPS++;
  }
  ShowFPS();
}

void ShowFPS() {
  if (millis() - lastShowFPS > 1000)
  {
    lastShowFPS = millis();
    Serial.print("FPS-");
    Serial.print(STAT_FPS);
    STAT_FPS = 0;
    for(int i = 0; i < 8; i++)
    {
      Serial.print(" IO");
      Serial.print(i);
      Serial.print("-");
      Serial.print(STAT_IO_COUNT[i]);
      STAT_IO_COUNT[i] = 0;
    }
    Serial.print(" us-");
    Serial.println(lastShowFPS);
  }
}

#pragma region ARTNETMODE

void ParseArtnetDmx(artnet_dmx_t *packet, uint8_t universe) {
  int id = 0;
  for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
  {
    leds[universe * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
    id = id + 3;
  }
}

void handle_packet()
{
  Udp.read((uint8_t *)&packetBuffer, MAX_BUFFER_UDP);
  packet_type = (artnet_packet_type_t)get_packet_type((uint8_t *)&packetBuffer);
  //Serial.println(packet_type);
  if (packet_type == 0)  // bad packet
  {
    return;
  }
  if (packet_type == ARTNET_DMX)
  {
    if (sizeof(packetBuffer) < sizeof(artnet_dmx_t))
      return;
    else
      handle_dmx((artnet_dmx_t *)&packetBuffer);
  }
  //else
  if (packet_type == ARTNET_POLL)
  {
    if (sizeof(packetBuffer) < sizeof(artnet_poll_t))
      return;
    else
      handle_poll((artnet_poll_t *)&packetBuffer);
  } /*
	  else if(packet_type == ARTNET_IPPROG)
	  {
	  if(sizeof(packetBuffer) < sizeof(artnet_ipprog_t))
	  return;
	  else
	  handle_ipprog((artnet_ipprog_t *)&packetBuffer);
	  } */
  else if (packet_type == ARTNET_ADDRESS)
  {
    if (sizeof(packetBuffer) < sizeof(artnet_address_t))
      return;
    else
      handle_address((artnet_address_t *)&packetBuffer);
  }
}

uint16_t get_packet_type(uint8_t *packet) //this get artnet packet type
{
  if (!memcmp(packet, ArtNode.id, 8))
  {
    return bytes_to_short(packet[9], packet[8]);
  }
  return 0;  // bad packet
}

int handle_dmx(artnet_dmx_t *packet){
  //Serial.print("U-");
  //Serial.println(packet->universe);
  
  for(int i = 0; i < 8; i++)
  {
    if (packet->universe==ARTNODE_IO[i]) {
      STAT_IO_COUNT[i]++;
      ParseArtnetDmx(packet, i);
      break;
    }
  }
}

int handle_poll(artnet_poll_t *packet)
{
  if ((packet->ttm & 1) == 1) // controller say: send unicast reply
  {
    send_reply(UNICAST, (uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
  }
  else // controller say: send broadcast reply
  {
    send_reply(BROADCAST, (uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
  }
}

/*
  int handle_ipprog(artnet_ipprog_t *packet)
  {
  send_reply(UNICAST, (uint8_t *)&ArtIpprogReply, sizeof(ArtIpprogReply));//ojo
  }
*/

int handle_address(artnet_address_t *packet) //not implemented yet
{
  send_reply(UNICAST, (uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
}

void send_reply(uint8_t mode_broadcast, uint8_t *packet, uint16_t size)
{
  if (mode_broadcast == 1) // send broadcast packet
  {
    //Udp.beginPacket(ArtNode.broadcastIp, ArtNode.remotePort);
    Udp.beginPacket(ArtNode.broadcastIp, 6454);
    Udp.write(packet, size);
    Udp.endPacket();
  }
  else // send unicast packet to controller
  {
    //Udp.beginPacket(Remote_IP, ArtNode.remotePort);
    Udp.beginPacket(Remote_IP, 6454);
    Udp.write(packet, size);
    Udp.endPacket();
  }
}

void fill_art_node(artnet_node_t *node)
{
  //fill to 0's
  memset(node, 0, sizeof(node));

  //fill data
  memcpy(node->mac, factory_mac, 6);                   // the mac address of node
  memcpy(node->localIp, factory_localIp, 4);           // the IP address of node
  memcpy(node->broadcastIp, factory_broadcastIp, 4);   // broadcast IP address
  memcpy(node->gateway, factory_gateway, 4);           // gateway IP address
  memcpy(node->subnetMask, factory_subnetMask, 4);     // network mask (art-net use 'A' network type)

  sprintf((char *)node->id, "Art-Net\0"); // *** don't change never ***
  sprintf((char *)node->shortname, "LANPIX NODE 8U\0");
  sprintf((char *)node->longname, "LANPIX NODE by X-WL (Art-Net Node)\0");

  //memset(node->porttypes, 0x80, ARTNET_MAX_PORTS);
  //memset(node->goodinput, 0x08, ARTNET_MAX_PORTS);

  memset(node->porttypes, 0x45, 4);
  memset(node->goodinput, 0x80, 4);
  memset (node->goodoutput, 0x00, 4);


  node->subH = 0x00;        // high byte of the Node Subnet Address (This field is currently unused and set to zero. It is
  // provided to allow future expansion.) (art-net III)
  node->sub = 0x00;        // low byte of the Node Subnet Address

  // **************************** art-net address of universes **********************************
  // not implemented yet
  node->swin[0] = 0x00;        // This array defines the 8 bit Universe address of the available input channels.
  node->swin[1] = 0x01;        // values from 0x00 to 0xFF
  node->swin[2] = 0x02;
  node->swin[3] = 0x03;
  //node->swin[0] = 0x08;        // This array defines the 8 bit Universe address of the available input channels.
  //node->swin[1] = 0x09;        // values from 0x00 to 0xFF
  //node->swin[2] = 0x0A;
  //node->swin[3] = 0x0B;

  node->swout[0] = 0x00;        // This array defines the 8 bit Universe address of the available output channels.
  node->swout[1] = 0x01;        // values from 0x00 to 0xFF
  node->swout[2] = 0x02;
  node->swout[3] = 0x03;
  //node->swout[4] = 0x04;

  node->goodoutput[0] = 0x80;
  node->goodoutput[1] = 0x80;
  node->goodoutput[2] = 0x80;
  node->goodoutput[3] = 0x80;

  node->etsaman[0] = 0;        // The ESTA manufacturer code.
  node->etsaman[1] = 0;        // The ESTA manufacturer code.
  node->localPort = 0x1936;   // artnet UDP port is by default 6454 (0x1936)
  node->verH = 0;        // high byte of Node firmware revision number.
  node->ver = 2;        // low byte of Node firmware revision number.
  node->ProVerH = 0;        // high byte of the Art-Net protocol revision number.
  node->ProVer = 14;       // low byte of the Art-Net protocol revision number.
  node->oemH = 0;        // high byte of the oem value.
  node->oem = 0xFF;     // low byte of the oem value. (0x00FF = developer code)
  node->ubea = 0;        // This field contains the firmware version of the User Bios Extension Area (UBEA). 0 if not used
  node->status = 0x08;
  node->swvideo = 0;
  node->swmacro = 0;
  node->swremote = 0;
  node->style = 0;        // StNode style - A DMX to/from Art-Net device
}

void fill_art_poll_reply(artnet_reply_t *poll_reply, artnet_node_t *node)
{
  //fill to 0's
  memset(poll_reply, 0, sizeof(poll_reply));

  //copy data from node
  memcpy(poll_reply->id, node->id, sizeof(poll_reply->id));
  memcpy(poll_reply->ip, node->localIp, sizeof(poll_reply->ip));
  memcpy(poll_reply->mac, node->mac, sizeof(poll_reply->mac));
  memcpy(poll_reply->shortname, node->shortname, sizeof(poll_reply->shortname));
  memcpy(poll_reply->longname, node->longname, sizeof(poll_reply->longname));
  memcpy(poll_reply->nodereport, node->nodereport, sizeof(poll_reply->mac));
  memcpy(poll_reply->porttypes, node->porttypes, sizeof(poll_reply->porttypes));
  memcpy(poll_reply->goodinput, node->goodinput, sizeof(poll_reply->goodinput));
  memcpy(poll_reply->goodoutput, node->goodoutput, sizeof(poll_reply->goodoutput));
  memcpy(poll_reply->swin, node->swin, sizeof(poll_reply->swin));
  memcpy(poll_reply->swout, node->swout, sizeof(poll_reply->swout));
  memcpy(poll_reply->etsaman, node->etsaman, sizeof(poll_reply->etsaman));

  sprintf((char *)poll_reply->nodereport, "%i DMX output universes active.\0", node->numbports);

  poll_reply->opCode = 0x2100;  // ARTNET_REPLY
  poll_reply->port = node->localPort;
  poll_reply->verH = node->verH;
  poll_reply->ver = node->ver;
  poll_reply->subH = node->subH;
  poll_reply->sub = node->sub;
  poll_reply->oemH = node->oemH;
  poll_reply->oem = node->oem;
  poll_reply->status = node->status;
  poll_reply->numbportsH = node->numbportsH;
  poll_reply->numbports = node->numbports;
  poll_reply->swvideo = node->swvideo;
  poll_reply->swmacro = node->swmacro;
  poll_reply->swremote = node->swremote;
  poll_reply->style = node->style;
}
/*
  void fill_art_ipprog_reply(artnet_ipprog_reply_t *ipprog_reply, artnet_node_t *node)
  {
  //fill to 0's
  memset (ipprog_reply, 0, sizeof(ipprog_reply));

  //copy data from node
  memcpy (ipprog_reply->id, node->id, sizeof(ipprog_reply->id));

  ipprog_reply->ProgIpHi  = node->localIp[0];
  ipprog_reply->ProgIp2   = node->localIp[1];
  ipprog_reply->ProgIp1   = node->localIp[2];
  ipprog_reply->ProgIpLo  = node->localIp[3];

  ipprog_reply->ProgSmHi  = node->subnetMask[0];
  ipprog_reply->ProgSm2   = node->subnetMask[1];
  ipprog_reply->ProgSm1   = node->subnetMask[2];
  ipprog_reply->ProgSmLo  = node->subnetMask[3];

  ipprog_reply->OpCode        = 0xF900; //ARTNET_IPREPLY
  ipprog_reply->ProVerH       = node->ProVerH;
  ipprog_reply->ProVer        = node->ProVer;
  ipprog_reply->ProgPortHi    = node->localPort >> 8;
  ipprog_reply->ProgPortLo    =(node->localPort & 0xFF);
  }
*/
#pragma endregion
// ARTNETMODE

boolean ReadNVS(){
  Serial.println("ReadNVS");
  byte temp[6]={0,0,0,0,0,0};
  preferences.getBytes("mac",temp,6);
  factory_mac[0] = temp[0];
  factory_mac[1] = temp[1];
  factory_mac[2] = temp[2];
  factory_mac[3] = temp[3];
  factory_mac[4] = temp[4];
  factory_mac[5] = temp[5];
  
  preferences.getBytes("l_ip",temp,4);
  factory_localIp[0] = temp[0];
  factory_localIp[1] = temp[1];
  factory_localIp[2] = temp[2];
  factory_localIp[3] = temp[3];
  
  preferences.getBytes("b_ip",temp,4);
  factory_broadcastIp[0] = temp[0];
  factory_broadcastIp[1] = temp[1];
  factory_broadcastIp[2] = temp[2];
  factory_broadcastIp[3] = temp[3];

  preferences.getBytes("g_ip",temp,4);
  factory_gateway[0] = temp[0];
  factory_gateway[1] = temp[1];
  factory_gateway[2] = temp[2];
  factory_gateway[3] = temp[3];
  
  preferences.getBytes("s_msk",temp,4);
  factory_subnetMask[0] = temp[0];
  factory_subnetMask[1] = temp[1];
  factory_subnetMask[2] = temp[2];
  factory_subnetMask[3] = temp[3];

  preferences.getBytes("dns",temp,4);
  factory_dns[0] = temp[0];
  factory_dns[1] = temp[1];
  factory_dns[2] = temp[2];
  factory_dns[3] = temp[3];

  String str = preferences.getString("ssid",str);
  unsigned char* buf = new unsigned char[100];
  str.getBytes(buf, 100, 0);
  ssid = (const char*)buf;
  Serial.print("SSID: \t");
  Serial.println(ssid);
  str = preferences.getString("password",str);
  buf = new unsigned char[100];
  str.getBytes(buf, 100, 0);
  password = (const char*)buf;
  Serial.print("Password: \t");
  Serial.println(password);

  preferences.getBytes("params",temp,5);
  ARTNODE_BRIGHT = temp[0];
  ARTNODE_TARGET_FPS = temp[1];
  RefreshTime = 1000 / ARTNODE_TARGET_FPS;
  //Serial.print("Auto Reboot: \t");
  //Serial.println(temp[2]);
  //Serial.print("Use OSC: \t");
  //Serial.println(temp[3]);
  //Serial.print("OSC Port: \t");
  //Serial.println(temp[4]);
}

bool WriteNVS(){
  Serial.println("Write NVS");
  preferences.putBytes("mac",factory_mac,6);
  preferences.putBytes("l_ip",factory_localIp,4);
  preferences.putBytes("b_ip",factory_broadcastIp,4);
  preferences.putBytes("g_ip",factory_gateway,4);
  preferences.putBytes("s_msk",factory_subnetMask,4);
  preferences.putBytes("dns",factory_dns,4);

  preferences.putString("ssid",ssid);
  preferences.putString("password",password);

  //preferences.putBytes("params",params,PARAMS_NUM);
  //preferences.putBytes("io_map",STAT_IO_COUNT,8);
  Serial.println("Write NVS complite!");
  Serial.println("");
}

void CorrectBri(){
  if (ARTNODE_MODE == true){
    //Fix DevideInteger
    //FastLED.setBrightness(ARTNODE_BRIGHT*255/100);
  } else {
    //FastLED.setBrightness(floor(ARTNODE_BRIGHT*255/ARTNODE_STD_DIM));
  } 
}

boolean ConnectWiFi(void){
  boolean state = true;
  int i = 0;
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  //Wait for connection
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20) {
      state = false;
      break;
    }
    i++;
  }
  if (state) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);

    WiFi.config(ArtNode.localIp, ArtNode.gateway, ArtNode.subnetMask, factory_dns);
    Udp.begin(ArtNode.localPort);
    Serial.println("     Node Config");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("SubnetMask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("GatewayIP: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS_IP: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("MAC Adress: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("");
    Serial.println("Connected failed");
  }
  return state;
}

/** show() for ESP32
    Call this function instead of FastLED.show(). It signals core 0 to issue a show,
    then waits for a notification that it is done.
*/
void FastLEDshowESP32(){
  if (userTaskHandle == 0) {
    // -- Store the handle of the current task, so that the show task can
    //    notify it when it's done
    userTaskHandle = xTaskGetCurrentTaskHandle();

    // -- Trigger the show task
    xTaskNotifyGive(FastLEDshowTaskHandle);

    // -- Wait to be notified that it's done
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
    ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    userTaskHandle = 0;
  }
}
/** show Task
    This function runs on core 0 and just waits for requests to call FastLED.show()
*/
void FastLEDshowTask(void *pvParameters){
  // -- Run forever...
  for (;;) {
    // -- Wait for the trigger
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // -- Do the show (synchronously)
    FastLED.show();

    // -- Notify the calling task
    xTaskNotifyGive(userTaskHandle);
    //userTaskHandle = 0;
  }
}
