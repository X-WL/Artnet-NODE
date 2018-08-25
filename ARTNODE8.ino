/*
 Name:		ARTNET_NODE.ino
 Created:	3/18/2018 4:34:59 PM
 Author:	xwolk
*/
#include <WiFi.h>
#include <WiFiUdp.h>
#include "artnet_node.h"
#include "common.h"          // definitions from libartnet
#include "packets.h" 
#include "FastLED.h"

WiFiUDP Udp;

const char* ssid = "LED";
const char* password = "Soundcraft";

#pragma region Settings
uint8_t factory_mac[6] = { 1,   2,   3,   0,   0,  10 }; // the mac address of node
uint8_t factory_localIp[4] = { 2,   0,   0,  30 };           // the IP address of node
uint8_t factory_broadcastIp[4] = { 2, 255, 255, 255 };           // broadcast IP address
uint8_t factory_gateway[4] = { 2,   0,   0,   1 };           // gateway IP address (use ip address of controller)
uint8_t factory_subnetMask[4] = { 255,   0,   0,   0 };           // network mask (art-net use 'A' network type)

uint8_t factory_dns[4] = { 2,   0,   0,   1 };           // TODO DNS

uint8_t factory_swin[4] = { 0,   1,   2,   3};
uint8_t factory_swout[4] = { 0,   1,   2,   3 };

artnet_node_t             ArtNode;
artnet_reply_t            ArtPollReply;
//artnet_ipprog_reply_t   ArtIpprogReply; //not implemented yet
artnet_packet_type_t      packet_type;

const int MAX_BUFFER_UDP = 1650;	// For Arduino MEGA

uint8_t packetBuffer[MAX_BUFFER_UDP];             //buffer to store incoming data (volatile?)
byte Remote_IP[4] = { 2,255,255,255 };
uint16_t tRemotePort = 6454;

#pragma endregion

#define DATA_PIN1    3
#define DATA_PIN2    21
#define DATA_PIN3    19
#define DATA_PIN4    18

#define DATA_PIN5    5
#define DATA_PIN6    17
#define DATA_PIN7    16
#define DATA_PIN8    14

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

#define NUM_STRIPS 9
#define NUM_LEDS_PER_STRIP 170

#define NUM_LEDS NUM_LEDS_PER_STRIP * NUM_STRIPS
CRGB leds[NUM_LEDS];

#define TargetFPS 30

// -- The core to run FastLED.show()
#define FASTLED_SHOW_CORE 0

// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

int RefreshTime = 1000 / TargetFPS;

int OldMilis = 0;
int lastReply = 0;
int lastUpdate = 0;

int u0Count = 0;
int u1Count = 0;
int u2Count = 0;
int u3Count = 0;
int u4Count = 0;
int u5Count = 0;
int u6Count = 0;
int u7Count = 0;
int FPSCount = 0;
int CounterOld = 0;

int lastShow = 0;

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(115200);
  fill_art_node(&ArtNode);
  ArtNode.numbports = 5;
  while(ConnectWiFi() == false){
  }
  Serial.println("Start NODE");
  
  FastLED.addLeds<LED_TYPE,DATA_PIN1,COLOR_ORDER>(leds, 0, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,DATA_PIN2,COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,DATA_PIN3,COLOR_ORDER>(leds, 2 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,DATA_PIN4,COLOR_ORDER>(leds, 3 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
   
  FastLED.addLeds<LED_TYPE,DATA_PIN5,COLOR_ORDER>(leds, 4 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,DATA_PIN6,COLOR_ORDER>(leds, 5 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,DATA_PIN7,COLOR_ORDER>(leds, 6 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,DATA_PIN8,COLOR_ORDER>(leds, 7 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);

  int core = xPortGetCoreID();
    Serial.print("Main code running on core ");
    Serial.println(core);

    // -- Create the FastLED show task
    xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);

	fill_art_poll_reply(&ArtPollReply, &ArtNode);
	//fill_art_ipprog_reply  (&ArtIpprogReply, &ArtNode);
  
	//Ethernet.begin(ArtNode.mac, ArtNode.localIp, factory_dns, ArtNode.gateway, ArtNode.subnetMask);
	
	send_reply(BROADCAST, (uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
	Serial.println("Setup");
  Serial.println(ARNET_HEADER_SIZE);
}

// the loop function runs over and over again until power down or reset
void loop() {
	if (millis() - lastReply>3000)
	{
		send_reply(BROADCAST, (uint8_t *)&ArtPollReply, sizeof(ArtPollReply));  //Need FIX Art_Poll, Art_Replay 
		lastReply = millis();													// Remote IP/Port
	}
 int l = Udp.parsePacket();
 if (l>0){
  handle_packet();
 }
 /*
 if (Udp.parsePacket() > ARNET_HEADER_SIZE) {
    //Serial.print("handle");
    //PackRead();
   handle_packet();
  }
  */
	if (millis()-lastUpdate>RefreshTime)
	{
		//lastShow = millis();
		FastLEDshowESP32();
		//Serial.print("S-");
		//Serial.println(millis()- lastShow);
   FPSCount++;
		lastUpdate = millis();
	}
	ShowFPS();
}

void ShowPacket(int len){
  uint8_t artnetPacket[len];
  Udp.read(artnetPacket, len);
  for (int i=0;i<len;i++){
    Serial.print(artnetPacket[i]);
  }
  Serial.println();
}

void ShowFPS(){
  if (millis()-CounterOld>1000)
  {
    Serial.print("FPS-");
    Serial.print(FPSCount);
    Serial.print(" CU0-");
    Serial.print(u0Count);
    Serial.print(" CU1-");
    Serial.print(u1Count);
    Serial.print(" CU2-");
    Serial.print(u2Count);
    Serial.print(" CU3-");
    Serial.print(u3Count);
    Serial.print(" CU4-");
    Serial.print(u4Count);
    Serial.print(" CU5-");
    Serial.print(u5Count);
    Serial.print(" CU6-");
    Serial.print(u6Count);
    Serial.print(" CU7-");
    Serial.println(u7Count);
    u0Count = 0;
    u1Count = 0;
    u2Count = 0;
    u3Count = 0;
    u4Count = 0;
    u5Count = 0;
    u6Count = 0;
    u7Count = 0;
    FPSCount = 0;
    CounterOld = millis();
  }
}

#pragma region Parse Normal
void NormalParseU0(artnet_dmx_t *packet) {
	int id = 0;
	for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
	{
		leds[i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
		id = id + 3;
	}
}

void NormalParseU1(artnet_dmx_t *packet) {
	int id = 0;
	for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
	{
		leds[NUM_LEDS_PER_STRIP+i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
		id = id + 3;
	}
}

void NormalParseU2(artnet_dmx_t *packet) {
	int id = 0;
	for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
	{
		leds[2 * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
		id = id + 3;
	}
}

void NormalParseU3(artnet_dmx_t *packet) {
	int id = 0;
	for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
	{
		leds[3 * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
		id = id + 3;
	}
}

void NormalParseU4(artnet_dmx_t *packet) {
	int id = 0;
	for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
	{
		leds[4 * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
		id = id + 3;
	}
}

void NormalParseU5(artnet_dmx_t *packet) {
	int id = 0;
	for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
	{
		leds[5 * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
		id = id + 3;
	}
}

void NormalParseU6(artnet_dmx_t *packet) {
  int id = 0;
  for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
  {
    leds[6 * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
    id = id + 3;
  }
}

void NormalParseU7(artnet_dmx_t *packet) {
  int id = 0;
  for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
  {
    //leds[7 * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
    //id = id + 3;
  }
}

void NormalParseU8(artnet_dmx_t *packet) {
  int id = 0;
  for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) //Fix Sizeble
  {
    leds[8 * NUM_LEDS_PER_STRIP + i] = CRGB(packet->data[id], packet->data[id + 1], packet->data[id + 2]);
    id = id + 3;
  }
}

#pragma endregion

#pragma region ARTNETMODE
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

int handle_dmx(artnet_dmx_t *packet)
{
	//Serial.print("U-");
	//Serial.println(packet->universe);
	if (packet->universe == ArtNode.swin[0])
	{
		u0Count++;
		NormalParseU0(packet);
	}
	else if (packet->universe == ArtNode.swin[1])
	{
		u1Count++;
		NormalParseU1(packet);
	}
	else if (packet->universe == ArtNode.swin[2])
	{
		u2Count++;
		NormalParseU2(packet);
	}
	else if (packet->universe == ArtNode.swin[3])
	{
		u3Count++;
		NormalParseU3(packet);
	}
	else if (packet->universe == ArtNode.swin[4])
	{
		u4Count++;
		NormalParseU4(packet);
	}
	else if (packet->universe == 5)
	{
		u5Count++;
		NormalParseU5(packet);
	}
  else if (packet->universe == 6)
  {
    u6Count++;
    NormalParseU6(packet);
  }
  else if (packet->universe == 7)
  {
    u7Count++;
    NormalParseU7(packet);
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
	sprintf((char *)node->shortname, "LANPIX NODE 5U\0");
	sprintf((char *)node->longname, "LANPIX NODE by X-WL (Art-Net Node)\0");

	//memset(node->porttypes, 0x80, ARTNET_MAX_PORTS);
	//memset(node->goodinput, 0x08, ARTNET_MAX_PORTS);

	memset(node->porttypes, 0x45, 4);
	//memset(node->goodinput, 0x08, 4);
	//memset (node->goodoutput, 0x00, 4);
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
	node->swin[4] = 0x04;
	//node->swin[5] = 0x05;

	node->swout[0] = 0x00;        // This array defines the 8 bit Universe address of the available output channels.
	node->swout[1] = 0x01;        // values from 0x00 to 0xFF
	node->swout[2] = 0x02;
	node->swout[3] = 0x03;
	node->swout[4] = 0x04;
	//node->swout[5] = 0x05;

/*	

#if defined(USE_UNIVERSE_0)
	node->goodoutput[0] = 0x80;
#endif

#if defined(USE_UNIVERSE_1)
	node->goodoutput[1] = 0x80;
#endif

#if defined(USE_UNIVERSE_2)
	node->goodoutput[2] = 0x80;
#endif

#if defined(USE_UNIVERSE_3)
	node->goodoutput[3] = 0x80;
#endif

#if defined(USE_UNIVERSE_4)
	node->goodoutput[4] = 0x80;
#endif

#if defined(USE_UNIVERSE_5)
	//node->goodoutput[5] = 0x80;
#endif
*/

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

boolean ConnectWiFi(void)
{
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
    if (i>20){
      state = false;
      break;
    }
    i++;
  }
  if (state) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    
    WiFi.config(ArtNode.localIp,ArtNode.gateway,ArtNode.subnetMask,factory_dns);
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
 *  Call this function instead of FastLED.show(). It signals core 0 to issue a show, 
 *  then waits for a notification that it is done.
 */
void FastLEDshowESP32()
{
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
 *  This function runs on core 0 and just waits for requests to call FastLED.show()
 */
void FastLEDshowTask(void *pvParameters)
{
    // -- Run forever...
    for(;;) {
        // -- Wait for the trigger
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // -- Do the show (synchronously)
        FastLED.show();

        // -- Notify the calling task
        xTaskNotifyGive(userTaskHandle);
    }
}
