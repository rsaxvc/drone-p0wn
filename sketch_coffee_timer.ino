/*INCLUDES*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <time.h>

/*COMPILE-TIME CHECKS*/
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/*WiFi NETWORK CONFIGURATION*/
static const struct
/*These are APs with hardcoded credentials. AP names must be unique*/
  {
  const char * access_point;
  const char * preshared_key;
  } wifi_credential_list[] = {
    {"b.wifi.rsaxvc.net","HaHaNotPuttingMyPasswordOnGithub"}
  };
static const size_t wifi_credential_list_len = sizeof( wifi_credential_list ) / sizeof( wifi_credential_list[0] );
static const char * const wifi_skip_list[]=
/*These never work, so don't bother with them*/
  {
   "xfinitywifi"
  };
static const size_t wifi_skip_list_len = sizeof(wifi_skip_list)/sizeof(wifi_skip_list[0]);

/*GLOBAL CONSTANTS*/
static const unsigned long chicago_offset = 6 * 60 * 60;
static const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
static const char* const ntpServerName = "time.nist.gov";
static const unsigned int localPort = 2390;      // local port to listen for UDP packets

/*GLOBAL BUFFERS*/
static byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
static Adafruit_SSD1306 display;

void setup() {
  Serial.begin(74880); //This is because the ESP8266 bootloader runs at this baud, and because I'm lazy
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // generate high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C
  display.clearDisplay();
  display.display();
  
  while (!Serial);             // Leonardo: wait for serial monitor
  Serial.println("Setup done");
}

// send an NTP request to the time server at the given address
static void sendNTPpacket(WiFiUDP & udp, IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

static void println_tm(unsigned long epoch)
{
  Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
  Serial.print(':');
  if ( ((epoch % 3600) / 60) < 10 ) 
    {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
    }
  Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
  Serial.print(':');
  if ( (epoch % 60) < 10 )
    {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
    }
  Serial.println(epoch % 60); // print the second
}

// recv an NTP request from the time server
static unsigned long recvNTPpacket(WiFiUDP & udp )
{
  int packet_size = udp.parsePacket();
  if( packet_size )
    {
    Serial.print("packet received, length=");
    Serial.println(packet_size);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // now convert NTP time into everyday time:
    return secsSince1900;
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
    return epoch;
    }
  return 0;
}

static unsigned long get_ntp_time() {
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i)
  {
    int j;
    static const char * const no_preshared_key_found = "";
    const char * preshared_key = no_preshared_key_found;

    /*Check if AP in Whitelist*/
    for( j = 0; j < wifi_credential_list_len; ++j )
      {
      if( WiFi.SSID(i) == wifi_credential_list[j].access_point )
        {
        Serial.print("Retrieving Creds for ");
        Serial.println(WiFi.SSID(i));
        preshared_key = wifi_credential_list[j].preshared_key;
        break;
        }
      }

    /*Check if AP in blacklist*/
    for( j = 0; j < wifi_skip_list_len; ++j )
      {
      if( WiFi.SSID(i) == wifi_skip_list[j] )break;
      }
    if( j != wifi_skip_list_len )
      {
      Serial.print("Skipping ");
      Serial.println(WiFi.SSID(i));
      continue;
      }

    /*At this point, we only want open APs*/
    if( WiFi.encryptionType(i) != ENC_TYPE_NONE && preshared_key == no_preshared_key_found)
    {
      Serial.print("Skipping ");
      Serial.println(WiFi.SSID(i));
      continue;
    }

    // We start by connecting to a WiFi network
    Serial.print("Connecting to ");
    Serial.print(WiFi.SSID(i));
    WiFi.begin(WiFi.SSID(i).c_str(), preshared_key );

    Serial.print("..");
    wl_status_t stat = WiFi.status();
    while ( true )
      {
      delay(100);
      stat = WiFi.status();
      if( stat == WL_CONNECTED )
        {
        Serial.print("Connected(");
        Serial.print(WiFi.localIP());
        Serial.println(")!");
        break;
        }
      if( stat == WL_CONNECT_FAILED || stat == WL_CONNECT_FAILED || stat == WL_NO_SSID_AVAIL )
        {
        break;
        }
      }
    if( stat != WL_CONNECTED )
      {
      Serial.println("Failed :(");
      WiFi.disconnect();
      continue;
      }

    //get a random server from the pool
    IPAddress timeServerIP;
    if( 1 != WiFi.hostByName(ntpServerName, timeServerIP) ) {
      Serial.println("Failed to resolve ");
      Serial.print(ntpServerName);
      Serial.print(" into ");
      Serial.print(timeServerIP);
      Serial.println(". Aborting");
      continue;
    }
    Serial.print("Resolved ");
    Serial.print(ntpServerName);
    Serial.print(" into ");
    Serial.print(timeServerIP);
    Serial.println(".");

    // A UDP instance to let us send and receive packets over UDP
    WiFiUDP udp;
    udp.begin(localPort);
    Serial.println(udp.localPort());

    for( i = 0; i < 5; ++i )
    {
      sendNTPpacket(udp, timeServerIP); // send an NTP packet to a time server
      for( unsigned j = 0; j < 4000; ++j )
        {
        delay(1);    // wait to see if a reply is available
        unsigned long epoch = recvNTPpacket(udp);
        if( epoch ) 
          {
            Serial.print("epoch:");
            Serial.println(epoch);
            WiFi.disconnect();
            return epoch;
          }
        }
    }
    WiFi.disconnect();
  }
return 0;
}

static void padform( unsigned input )
{
if( input < 10 )
  display.print("0");
display.print(input);
}

/*
 * The epoch year. Currently 1900.
 */
static const int EPOCH_YEAR = 1900;

/*
 * The year leap seconds began.
 */
static const int LEAP_SECOND_YEAR = 1972;

/*
 * The amount of seconds we were already behind by by 1972.
 * This is added to all times in year >= 1972 before the addition
 * of leap seconds.
 */
static const int LEAP_SECOND_CATCHUP_VALUE = 10;
      
/*
 * Time/date constants that will probably never change.
 */
static const int DAYS_IN_YEAR = 365;
static const int HOURS_IN_DAY = 24;
static const int SECONDS_IN_MINUTE = 60;
static const int MINUTES_IN_HOUR = 60;

// February is 28 here, but we will account for leap years further down.
static const int numDaysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void dateFromNumberOfSeconds(
    uint32_t secs, uint32_t *year, uint32_t *month, uint32_t *day,
    uint32_t *hour, uint32_t *minute, uint32_t *second)
{
    bool isLeapYear = false;
    for (uint32_t currentYear = EPOCH_YEAR; ; currentYear++)
    {
        int multipleOfFour = (currentYear % 4) == 0;
        int multipleOfOneHundred = (currentYear % 100) == 0;
        int multipleOfFourHundred = (currentYear % 400) == 0;        
        isLeapYear = false;
        
        // Formula: years divisble by 4 are leap years, EXCEPT if it's
        // divisible by 100 and not by 400.
        if (multipleOfFour && !(multipleOfOneHundred && !multipleOfFourHundred))
        {
            isLeapYear = true;
        }
        
        uint32_t secsInYear = (uint32_t)SECONDS_IN_MINUTE * (uint32_t)MINUTES_IN_HOUR * (uint32_t)HOURS_IN_DAY * (uint32_t)(isLeapYear ? (DAYS_IN_YEAR + 1) : DAYS_IN_YEAR);
        *year = currentYear;
        if (secs < secsInYear)
        {
            break;
        }
        
        secs -= secsInYear;
    }
    
    for (uint32_t currentMonth = 0; ; currentMonth++)
    {
        uint32_t secsInMonth = (uint32_t)SECONDS_IN_MINUTE * (uint32_t)MINUTES_IN_HOUR * (uint32_t)HOURS_IN_DAY * (uint32_t)numDaysInMonths[currentMonth];
        
        if (currentMonth == 1 && isLeapYear)
        {
            secsInMonth += (uint32_t)SECONDS_IN_MINUTE * (uint32_t)MINUTES_IN_HOUR * (uint32_t)HOURS_IN_DAY;
        }
        
        *month = currentMonth + 1;
        if (secs < secsInMonth)
        {
            break;
        }
        
        secs -= secsInMonth;
    }
    
    for (uint32_t currentDay = 0; ; currentDay++)
    {
        uint32_t secsInDay = (uint32_t)SECONDS_IN_MINUTE * (uint32_t)MINUTES_IN_HOUR * (uint32_t)HOURS_IN_DAY;
        *day = currentDay + 1;
        if (secs < secsInDay)
        {
            break;
        }
        secs -= secsInDay;
    }
    
    for (uint32_t currentHour = 0; ; currentHour++)
    {
        uint32_t secsInHour = (uint32_t)SECONDS_IN_MINUTE * (uint32_t)MINUTES_IN_HOUR;
        *hour = currentHour;
        if (secs < secsInHour)
        {
            break;
        }
        secs -= secsInHour;
    }
    
    for (uint32_t currentMinute = 0; ; currentMinute++)
    {
        uint32_t secsInMinute = (uint32_t)SECONDS_IN_MINUTE;
        *minute = currentMinute;
        if (secs < secsInMinute)
        {
            break;
        }
        secs -= secsInMinute;
    }
    
    *second = secs;
}

bool isLeapYear(uint32_t year)
{
    int multipleOfFour = (year % 4) == 0;
    int multipleOfOneHundred = (year % 100) == 0;
    int multipleOfFourHundred = (year % 400) == 0;
        
    // Formula: years divisble by 4 are leap years, EXCEPT if it's
    // divisible by 100 and not by 400.
    return (multipleOfFour && !(multipleOfOneHundred && !multipleOfFourHundred));
}

uint32_t numberOfSecondsSince1900Epoch(
    uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second)
{
    uint32_t returnValue = 0;
    
    // Hours, minutes and regular seconds are trivial to add. 
    returnValue = 
        second + 
        (minute * SECONDS_IN_MINUTE) + 
        (hour * SECONDS_IN_MINUTE * MINUTES_IN_HOUR);
    
    // Leap second handling. For each year between 1972 and the provided year,
    // add 1 second for each 1 bit in the leapSeconds array.
    /*if (year >= LEAP_SECOND_YEAR)
    {
        returnValue += LEAP_SECOND_CATCHUP_VALUE;
        
        for (uint32_t currentYear = LEAP_SECOND_YEAR; currentYear < year; currentYear++)
        {
            returnValue += numberOfLeapSecondsInYear(currentYear, false);
        }
    
        // For the current year, only add the June leap second if the current month is 
        // >= July.
        if (month >= 7)
        {
            returnValue += numberOfLeapSecondsInYear(year, true);
        }
    }*/
    
    // Days, months and years are as well, with several caveats: 
    //   a) We need to account for leap years.
    //   b) We need to account for different sized months.
    uint32_t numDays = 0;
    for (uint32_t currentYear = EPOCH_YEAR; currentYear < year; currentYear++)
    {
        if (isLeapYear(currentYear))
        {
            numDays++;
        }
    }
    numDays += DAYS_IN_YEAR * (year - EPOCH_YEAR);
    for (uint32_t currentMonth = 0; currentMonth < month - 1; currentMonth++)
    {
        numDays += numDaysInMonths[currentMonth];
    }
    numDays += day - 1;
    if (isLeapYear(year) && month > 2)
    {
        numDays++;
    }
    returnValue += numDays * SECONDS_IN_MINUTE * MINUTES_IN_HOUR * HOURS_IN_DAY;
    
    // Return final result.
    return returnValue;
}

uint32_t numberOfLeapSecondsInYear(uint32_t year, bool skipDecember)
{
    // Leap second bit vector. Every group of two bits is the 6/30 leap second
    // and 12/31 leap second, respectively, beginning from 1972.
    // NOTE: update this whenever IERS announces a new leap second. No, it's 
    //       not optimal.
    static const uint32_t leapSecondAdds[] = {
        0xD5552A21, // 1972-1988
        0x14A92400, // 1989-2005
        0x04082000, // 2006-2022
        0x00000000  // 2023-2039
    };
    static const uint32_t leapSecondDeletes[] = {
        0x00000000, // 1972-1988
        0x00000000, // 1989-2005
        0x00000000, // 2006-2022
        0x00000000  // 2023-2039
    };
    
    static const int numBitsPerEntry = (sizeof(uint32_t) * 8);
    uint32_t yearDiff = year - LEAP_SECOND_YEAR;
    uint32_t leapSecondAddEntry = leapSecondAdds[(2*yearDiff) / numBitsPerEntry];
    uint32_t leapSecondDeleteEntry = leapSecondDeletes[(2*yearDiff) / numBitsPerEntry];
    uint32_t leapSecondMaskJune = 1 << (numBitsPerEntry - (2*yearDiff) - 1);
    uint32_t leapSecondMaskDecember = skipDecember ? 0 : (1 << (numBitsPerEntry - (2*yearDiff) - 2));
    
    return
        ((leapSecondAddEntry & leapSecondMaskJune) ? 1 : 0) +
        ((leapSecondAddEntry & leapSecondMaskDecember) ? 1 : 0) -
        ((leapSecondDeleteEntry & leapSecondMaskJune) ? 1 : 0) -
        ((leapSecondDeleteEntry & leapSecondMaskDecember) ? 1 : 0);
}

void loop()
{
unsigned long ntp_time = 0;
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(WHITE);
display.setCursor(0,0);
display.println("*********************");
display.println("********Coffee!******");
display.println("*********************");
display.println("");
Serial.println("Fresh Coffee!");

while(!ntp_time) ntp_time = get_ntp_time();

display.setTextSize(2);
unsigned long local_time = ntp_time - chicago_offset;
uint32_t year, month, day, hour, minute, second;
dateFromNumberOfSeconds
  (
  local_time,
  &year,
  &month,
  &day,
  &hour,
  &minute,
  &second
  );

padform(year);  display.print("/");
padform(day);   display.print("/");
padform(month); display.println("");

padform(hour);  display.print(":");
padform(minute);display.print(":");
padform(second);display.println("");

ESP.deepSleep(60000000, WAKE_RF_DEFAULT); // Sleep for 60 seconds
}


