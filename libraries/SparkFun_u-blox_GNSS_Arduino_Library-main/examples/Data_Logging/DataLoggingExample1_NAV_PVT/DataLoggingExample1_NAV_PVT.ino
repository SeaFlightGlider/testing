/*
  Configuring the GNSS to automatically send NAV PVT reports over I2C and log them to file on SD card
  By: Paul Clark
  SparkFun Electronics
  Date: October 18th, 2021
  License: MIT. See license file for more information but you can
  basically do whatever you want with this code.

  This example shows how to configure the u-blox GNSS to send NAV PVT reports automatically
  and log the data to SD card in UBX format.

  This code is intended to be run on the MicroMod Data Logging Carrier Board using the Artemis Processor
  but can be adapted by changing the chip select pin and SPI definitions:
  https://www.sparkfun.com/products/16829
  https://www.sparkfun.com/products/16401

  Hardware Connections:
  Please see: https://learn.sparkfun.com/tutorials/micromod-data-logging-carrier-board-hookup-guide
  Insert the Artemis Processor into the MicroMod Data Logging Carrier Board and secure with the screw.
  Connect your GNSS breakout to the Carrier Board using a Qwiic cable.
  Connect an antenna to your GNSS board if required.
  Insert a formatted micro-SD card into the socket on the Carrier Board.
  Connect the Carrier Board to your computer using a USB-C cable.
  Ensure you have the SparkFun Apollo3 boards installed: http://boardsmanager/All#SparkFun_Apollo3
  This code has been tested using version 2.1.0 of the Apollo3 boards on Arduino IDE 1.8.13.
   - Version 2.1.1 of Apollo3 contains a feature which makes I2C communication with u-blox modules problematic
   - We recommend using v2.1.0 of Apollo3 until v2.2.0 is released
  Select "Artemis MicroMod Processor" as the board type.
  Press upload to upload the code onto the Artemis.
  Open the Serial Monitor at 115200 baud to see the output.

  To minimise I2C bus errors, it is a good idea to open the I2C pull-up split pad links on
  both the MicroMod Data Logging Carrier Board and the u-blox module breakout.

  Data is logged in u-blox UBX format. Please see the u-blox protocol specification for more details.
  You can replay and analyze the data using u-center:
  https://www.u-blox.com/en/product/u-center

  Feel like supporting open source hardware?
  Buy a board from SparkFun!
  ZED-F9P RTK2: https://www.sparkfun.com/products/15136
  NEO-M8P RTK: https://www.sparkfun.com/products/15005
  SAM-M8Q: https://www.sparkfun.com/products/15106

*/

#include <SPI.h>
#include <SD.h>
//#include <Wire.h>
#include <i2c_t3.h>                         //library: https://github.com/nox771/i2c_t3
 //Needed for I2C to GNSS

#include <SparkFun_u-blox_GNSS_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGNSS;

File myFile; //File that all GNSS data is written to

//Define the microSD (SPI) Chip Select pin. Adjust for your processor if necessary.
#if defined(ARDUINO_ARCH_APOLLO3) // Check for SparkFun Apollo3 (Artemis) v1 or v2
  #if defined(ARDUINO_APOLLO3_SFE_ARTEMIS_MM_PB)      // Check for the Artemis MicroMod Processor Board on Apollo3 v2
    #define sdChipSelect SPI_CS   // SPI (microSD) Chip Select for the Artemis MicroMod Processor Board on Apollo3 v2
  #elif defined(ARDUINO_AM_AP3_SFE_ARTEMIS_MICROMOD)  // Check for the Artemis MicroMod Processor Board on Apollo3 v1
    #define sdChipSelect CS       // SPI (microSD) Chip Select for the Artemis MicroMod Processor Board on Apollo3 v1
  #else
    #define sdChipSelect CS       // Catch-all for the other Artemis Boards - change this if required to match your hardware
  #endif
#else

  #define sdChipSelect CS         // Catch-all for all non-Artemis boards - change this if required to match your hardware
  
#endif

#define packetLength 100 // NAV PVT is 92 + 8 bytes in length (including the sync chars, class, id, length and checksum bytes)

// Callback: printPVTdata will be called when new NAV PVT data arrives
// See u-blox_structs.h for the full definition of UBX_NAV_PVT_data_t
//         _____  You can use any name you like for the callback. Use the same name when you call setAutoPVTcallback
//        /                  _____  This _must_ be UBX_NAV_PVT_data_t
//        |                 /               _____ You can use any name you like for the struct
//        |                 |              /
//        |                 |              |
void printPVTdata(UBX_NAV_PVT_data_t ubxDataStruct)
{
    Serial.println();

    Serial.print(F("Time: ")); // Print the time
    uint8_t hms = ubxDataStruct.hour; // Print the hours
    if (hms < 10) Serial.print(F("0")); // Print a leading zero if required
    Serial.print(hms);
    Serial.print(F(":"));
    hms = ubxDataStruct.min; // Print the minutes
    if (hms < 10) Serial.print(F("0")); // Print a leading zero if required
    Serial.print(hms);
    Serial.print(F(":"));
    hms = ubxDataStruct.sec; // Print the seconds
    if (hms < 10) Serial.print(F("0")); // Print a leading zero if required
    Serial.print(hms);
    Serial.print(F("."));
    unsigned long millisecs = ubxDataStruct.iTOW % 1000; // Print the milliseconds
    if (millisecs < 100) Serial.print(F("0")); // Print the trailing zeros correctly
    if (millisecs < 10) Serial.print(F("0"));
    Serial.print(millisecs);

    long latitude = ubxDataStruct.lat; // Print the latitude
    Serial.print(F(" Lat: "));
    Serial.print(latitude);

    long longitude = ubxDataStruct.lon; // Print the longitude
    Serial.print(F(" Long: "));
    Serial.print(longitude);
    Serial.print(F(" (degrees * 10^-7)"));

    long altitude = ubxDataStruct.hMSL; // Print the height above mean sea level
    Serial.print(F(" Height above MSL: "));
    Serial.print(altitude);
    Serial.println(F(" (mm)"));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial); //Wait for user to open terminal
  Serial.println("SparkFun u-blox Example");

  Wire.begin(); // Start I2C communication with the GNSS

// On the Artemis, we can disable the internal I2C pull-ups too to help reduce bus errors
#if defined(AM_PART_APOLLO3)                      // Check for SparkFun Apollo3 (Artemis) v1
  Wire.setPullups(0);                             // Disable the internal I2C pull-ups on Apollo3 v1
#elif defined(ARDUINO_ARCH_APOLLO3)               // Else check for SparkFun Apollo3 (Artemis) (v2)
  #if defined(ARDUINO_APOLLO3_SFE_ARTEMIS_MM_PB)  // Check for the Artemis MicroMod Processor Board on Apollo3 v2
    // On Apollo3 v2 we can still disable the pull-ups but we need to do it manually
    // The IOM and pin numbers here are specific to the Artemis MicroMod Processor Board
    am_hal_gpio_pincfg_t sclPinCfg = g_AM_BSP_GPIO_IOM4_SCL;  // Artemis MicroMod Processor Board uses IOM4 for I2C communication
    am_hal_gpio_pincfg_t sdaPinCfg = g_AM_BSP_GPIO_IOM4_SDA;
    sclPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;          // Disable the pull-ups
    sdaPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;
    pin_config(PinName(39), sclPinCfg);                       // Artemis MicroMod Processor Board uses Pin/Pad 39 for SCL
    pin_config(PinName(40), sdaPinCfg);                       // Artemis MicroMod Processor Board uses Pin/Pad 40 for SDA
  #endif
#endif

  while (Serial.available()) // Make sure the Serial buffer is empty
  {
    Serial.read();
  }

  Serial.println(F("Press any key to start logging."));

  while (!Serial.available()) // Wait for the user to press a key
  {
    ; // Do nothing
  }

  delay(100); // Wait, just in case multiple characters were sent

  while (Serial.available()) // Empty the Serial buffer
  {
    Serial.read();
  }

  Serial.println("Initializing SD card...");

  // See if the card is present and can be initialized:
  if (!SD.begin(sdChipSelect))
  {
    Serial.println("Card failed, or not present. Freezing...");
    // don't do anything more:
    while (1);
  }
  Serial.println("SD card initialized.");

  // Create or open a file called "NAV_PVT.ubx" on the SD card.
  // If the file already exists, the new data is appended to the end of the file.
  myFile = SD.open("NAV_PVT.ubx", FILE_WRITE);
  if(!myFile)
  {
    Serial.println(F("Failed to create UBX data file! Freezing..."));
    while (1);
  }

  //myGNSS.enableDebugging(); // Uncomment this line to enable helpful GNSS debug messages on Serial

  // NAV PVT messages are 100 bytes long.
  // In this example, the data will arrive no faster than one message per second.
  // So, setting the file buffer size to 301 bytes should be more than adequate.
  // I.e. room for three messages plus an empty tail byte.
  myGNSS.setFileBufferSize(301); // setFileBufferSize must be called _before_ .begin

  if (myGNSS.begin() == false) //Connect to the u-blox module using Wire port
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing..."));
    while (1);
  }

  // Uncomment the next line if you want to reset your module back to the default settings with 1Hz navigation rate
  // (This will also disable any "auto" messages that were enabled and saved by other examples and reduce the load on the I2C bus)
  //myGNSS.factoryDefault(); delay(5000);

  myGNSS.setI2COutput(COM_TYPE_UBX); //Set the I2C port to output UBX only (turn off NMEA noise)
  myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); //Save (only) the communications port settings to flash and BBR

  myGNSS.setNavigationFrequency(1); //Produce one navigation solution per second

  myGNSS.setAutoPVTcallback(&printPVTdata); // Enable automatic NAV PVT messages with callback to printPVTdata

  myGNSS.logNAVPVT(); // Enable NAV PVT data logging

  Serial.println(F("Press any key to stop logging."));
}

void loop()
{
  myGNSS.checkUblox(); // Check for the arrival of new data and process it.
  myGNSS.checkCallbacks(); // Check if any callbacks are waiting to be processed.

  if (myGNSS.fileBufferAvailable() >= packetLength) // Check to see if a new packetLength-byte NAV PVT message has been stored
  {
    uint8_t myBuffer[packetLength]; // Create our own buffer to hold the data while we write it to SD card

    myGNSS.extractFileBufferData((uint8_t *)&myBuffer, packetLength); // Extract exactly packetLength bytes from the UBX file buffer and put them into myBuffer

    myFile.write(myBuffer, packetLength); // Write exactly packetLength bytes from myBuffer to the ubxDataFile on the SD card

    //printBuffer(myBuffer); // Uncomment this line to print the data as Hexadecimal bytes
  }

  if (Serial.available()) // Check if the user wants to stop logging
  {
    myFile.close(); // Close the data file
    Serial.println(F("\r\nLogging stopped. Freezing..."));
    while(1); // Do nothing more
  }

  Serial.print(".");
  delay(50);
}

// Print the buffer contents as Hexadecimal bytes
// You should see:
// SYNC CHAR 1: 0xB5
// SYNC CHAR 2: 0x62
// CLASS: 0x01 for NAV
// ID: 0x07 for PVT
// LENGTH: 2-bytes Little Endian (0x5C00 = 92 bytes for NAV PVT)
// PAYLOAD: LENGTH bytes
// CHECKSUM_A
// CHECKSUM_B
// Please see the u-blox protocol specification for more details
void printBuffer(uint8_t *ptr)
{
  for (int i = 0; i < packetLength; i++)
  {
    if (ptr[i] < 16) Serial.print("0"); // Print a leading zero if required
    Serial.print(ptr[i], HEX); // Print the byte as Hexadecimal
    Serial.print(" ");
  }
  Serial.println();
}
