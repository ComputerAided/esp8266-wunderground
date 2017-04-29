#include <Wire.h>

#define SSID        "xxxx"                                        // Wifi SSID
#define PASS        "xxxxxxxx"                                    // WiFi Password
#define DEST_HOST   "api.wunderground"                            // API site host
#define DEST_IP     "192.254.235.21"                              // API site IP address
#define TIMEOUT     5000                                          // mS
#define CONTINUE    false                                         // Define for readability
#define HALT        true                                          // Define for readibility
 
boolean reading;                                                  // Bool for some indication of something
String responseString = "";                                       // Response string from server for .JSON page
int APItemp;                                                      // Integer temperature from .JSON page
boolean tempFound = false;                                        // Indicator for temperature found
boolean weatherFound = false;                                     // Indicator for weather found
char weather[30];                                                 // Char array for weatehr from .JSON page 
byte TWItemperature[4];                                           // Byte array for BCD temperature values
byte TWIcomb[3];                                                  // Byte array for I2C sending

/*******************************************************************************************************************************
* void checkData()
********************
* Purpose: Upates the response values from a string containing the API response .JSON file
* Entry: int APItemp and char weather contain outdated values
* Exit: int APItemp - updated
*       char[] weather - updated
*******************************************************************************************************************************/
void checkData()                                                
{
  int i;                                                            // Variable for string length
  if(responseString.startsWith(F("temp_f")) & !tempFound)           // If this is the temp_f line and we havent found temperature
  {
    char temp2[3];                                                  // Allocate temporary temperature variable
    tempFound = true;                                               // We have found temperature, so update variable
    for(i=7; i < responseString.length()-1; ++i )                   // For the length of the response string
    {
      if(responseString[i] == '.')                                  // If we found a '.' break the loop
        break;
      temp2[i-7] = responseString[i];                               // Otherwise add the character to temperature
    }
    APItemp = atoi(temp2);                                          // Convert char to int and store in global
  } 
  else if(responseString.startsWith(F("weather")) & !weatherFound)  // If this is the weather line and we haven't found weather
  {
    memset(weather, 0, sizeof(weather));
    weatherFound = true;                                            // We have found weather, so update variable
    for(i = 8; i<responseString.length()-1; ++i )                   // For the length of the response string
      weather[i-8] = responseString[i];                             // Append the character on the response string
  }
  responseString = "";                                              // Clear the response string
}

/*******************************************************************************************************************************
* void errorHalt()
********************
* Purpose: Halts the ESP8266 if we have an error
* Entry: An error was found
* Exit: Software has halted
*
* ToDo: Toggle the ESP8266 reset pin and restart software
*******************************************************************************************************************************/
void errorHalt()
{
  while(true){};
}

/*******************************************************************************************************************************
* boolean echoFind(String keyword)
********************
* Purpose: Finds a specific keyword response from the ESP8266
* Entry: String keyword - The response we are looking for
* Exit: Returns true if the keyword is found.
        Returns false if timed out
*******************************************************************************************************************************/
boolean echoFind(String keyword)
{
  byte current_char   = 0;
  byte keyword_length = keyword.length();
  
  long deadline = millis() + TIMEOUT;              // Calculate timeout deadline
  while(millis() < deadline)                       // Try until deadline
  {
    if (Serial.available())                        // If characters are available
    {
      char ch = Serial.read();
      if (ch == keyword[current_char])
      {
        if (++current_char == keyword_length)
          return true;
      }
    }
  }
  return false;                                    // Timed out
}

/*******************************************************************************************************************************
* void echoFlush()
********************
* Purpose: Reads and releases all characters from the ESP8266
* Entry: N/A
* Exit: All waiting characters from ESP8266 are flushed
*******************************************************************************************************************************/
void echoFlush()
  {while(Serial.available()) Serial.read();}

/*******************************************************************************************************************************
* void echoSkip()
********************
* Purpose: Look for 3 newlines
* Entry: N/A
* Exit: N/A
*******************************************************************************************************************************/
void echoSkip()
{
  echoFind("\n");        // Search for nl at end of command echo
  echoFind("\n");        // Search for 2nd nl at end of response.
  echoFind("\n");        // Search for 3rd nl at end of blank line.
}

/*******************************************************************************************************************************
* boolean echoCommand(String cmd, String ack, boolean halt_on_fail)
********************
* Purpose: Sends a command to the ESP8266 and wait for acknowledgement
* Entry: String cmd - Command send to the ESP8266
*        String ack - Expected ack string
*        boolean halt_on_fail - Configures if we should halt on a failure
* Exit: ESP8266 runs the command.
*       Ack is captured
*******************************************************************************************************************************/
boolean echoCommand(String cmd, String ack, boolean halt_on_fail)
{
  Serial.println(cmd);
  if (ack == "")                 // If no ack response specified, skip all available module output.
    echoSkip();                  
  else                           // Otherwise wait for ack.
    if (!echoFind(ack))          // timed out waiting for ack string 
      if (halt_on_fail)          // If halt on failure
        errorHalt();             // Critical failure halt.
      else
        return false;            // Let the caller handle it.
  return true;                   // ack blank or ack found
}

/*******************************************************************************************************************************
* boolean connectWiFi()
********************
* Purpose: Connects to the WiFi
* Entry: SSID defines the WiFi SSID
*        password defines the WiFi Password
* Exit: Return TRUE - ESP8266 is connected to the defined WiFi
*       Return FALSE - ESP8266 failed to connect
*******************************************************************************************************************************/
boolean connectWiFi()
{
  String cmd = "AT+CWJAP=\""; cmd += SSID; cmd += "\",\""; cmd += PASS; cmd += "\"";  // Connection string to connect to wifi
  if (echoCommand(cmd, "OK", CONTINUE))                                               // Join Access Point
    return true;
  else
    return false;
}

/*******************************************************************************************************************************
* void setBytes()
********************
* Purpose: Sets the bytes for the I2C request
* Entry: char[] weather - current weather from JSON
*        int APItemp - current temperature rom JSON
* Exit: TWIcomb[0] - encoded weather
*       TWIcomb[1] - encoded [tens][ones]
*       TWIcomb[2] - encoded [sign][hundreds]
*******************************************************************************************************************************/
void setBytes()
{
  String temp = weather;                    // Temp holder for weather value
  TWItemperature[0] = APItemp % 10;         // Assign ones
  TWItemperature[1] = (APItemp/10) % 10;    // Assign tens
  TWItemperature[2] = (APItemp/100) % 10;   // Assign hundreds
  if(APItemp > 0)                           // If the temperature is greater than 0
    TWItemperature[3] = B1111;              // Positive
  else                                      // If the temperature is less than 0
    TWItemperature[3] = B1110;              // Negative
    
  if(temp == "Overcast") {TWIcomb[0] = B1;}
  else if(temp.startsWith("Thunderstorm")) {TWIcomb[0] = B10;}
  else if(temp.endsWith("Fog")) {TWIcomb[0] = B11;}
  else if(weather == "Partly Cloudy") {TWIcomb[0] = B100;}
  else if(temp.endsWith("Snow") | temp.startsWith("Snow")){TWIcomb[0] = B101;}
  else if(temp == "Clear") {TWIcomb[0] = B110;}
  else if(temp.endsWith("Hail") | temp.startsWith("Hail")) {TWIcomb[0] = B111;}
  else if(temp.endsWith("Cloudy") | temp.startsWith("Cloudy")) {TWIcomb[0] = B1000;}
  else if(temp.endsWith("Smoke") | temp.startsWith("Smoke")) {TWIcomb[0] = B1001;}
  else if(temp.startsWith("Rain") | temp.endsWith("Rain")) {TWIcomb[0] = B1010;}
  else {TWIcomb[0] = B0;}
  
  TWIcomb[1] = (TWItemperature[1] << 4) | TWItemperature[0];    // TWIcomb[1] = [tens][ones]
  TWIcomb[2] = (TWItemperature[3] << 4) | TWItemperature[2];    // TWIcomb[2] = [sign][hundreds]
}

/*******************************************************************************************************************************
* void setup() - Required Arduino bootloader function
********************
* Purpose: sets up the MCU
* Entry: N/A
* Exit: System is ready for the ESP8266 to fetch webpages
*******************************************************************************************************************************/
void setup()  
{
  responseString.reserve(100);             // Reserve space for the .JSON response
  reading = false;                         // Initialize reading
  
  Wire.begin(2);                           // Set I2C slave address to 0x02
  Wire.onRequest(requestEvent);            // Set ISR for I2C request

  Serial.begin(115200);                    // Communication with ESP8266 via 5V/3.3V level shifter
  Serial.setTimeout(TIMEOUT);              // Set the timeout value for the ESP8266
  
  delay(2000);                             // Delay to complete setup

  echoCommand("AT+RST", "ready", HALT);    // Reset & test if the module is ready  
  
  echoCommand("AT+CWMODE=1", "", HALT);    // Station mode
  echoCommand("AT+CIPMUX=1", "", HALT);    // Allow multiple connections (we'll only use the first).

  boolean connection_established = false;  // Initialize connection state
  for(int i=0;i<5;i++)                     // For 5 attempts
  {
    if(connectWiFi())                      // If connection attempt is successful
    {
      connection_established = true;       // Set connection state
      break;                               // Break connection loop
    }
  }
  if (!connection_established)             // If connection attempts failed
    errorHalt();                           // Indicate there was an error
  
  delay(5000);                             // Delay to complete setup
  
  String cmd = "AT+CIPSTART=0,\"TCP\",\""; cmd += "23.203.214.89"; cmd += "\",80"; // Establish TCP connection
  if (!echoCommand(cmd, "OK", CONTINUE)) return;
  delay(2000);                             // Delay to complete setup
  if (!echoCommand("AT+CIPSTATUS", "OK", CONTINUE)) return; // Get connection status 
}

/*******************************************************************************************************************************
* void loop() - Required Arduino bootloader function
********************
* Purpose: Main code. Executes forever
* Entry: setup() completed
* Exit: N/A
*******************************************************************************************************************************/
void loop() 
{
  String cmd;                            // Command string to the ESP8266
  boolean startCapture = false;          // Boolean for indicating JSON capture
  String response = "";                  // Initialize the response string
  tempFound = false;                     // Indicator for temperature found
  weatherFound = false;                  // Indicator for weather found

  // Build HTTP request.
  cmd = "GET ";
  cmd += "http://api.wunderground.com/api/<your_key_here>/conditions/q/autoip.json";
  cmd += " HTTP/1.1\r\n\r\n";
  
  // Ready the module to receive raw data
  if (!echoCommand("AT+CIPSEND=0,"+String(cmd.length()), ">", CONTINUE))
  {
    echoCommand("AT+CIPCLOSE", "", CONTINUE);
    return;
  }
  
  
  echoCommand(cmd, "OK", CONTINUE);  // Send the raw HTTP request
  while(!(tempFound & weatherFound)) // Loop forever echoing data received from destination server.
  {
    if(Serial.available())
    {
      char inChar = Serial.read();
      if(inChar == '"')
        startCapture = true;
      else if(startCapture)
      {
        responseString += inChar;
        if(inChar == ',')
        {
          startCapture = false;
          checkData();
        }
      }
    }
  }
 
  while(Serial.available())      // Flush remaining characters
  {
    Serial.read();
    delay(1);
  }
  setBytes();                                      // Update TWIcomb
  delay(180000);                                   // 3 minute delay                             
}

void requestEvent()
{
  Wire.write((uint8_t *)TWIcomb, sizeof(TWIcomb));  // Will send 3 bytes
}
