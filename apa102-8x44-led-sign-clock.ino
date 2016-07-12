/* Serial1ly managed message board plus clock

Version: 0.1
   Author: quarterturn
   Date: 6/9/2016
   MCU Hardware: ATMEGA 1284p

   An electronic sign which can display and edit a number of strings stored in EEPROM on an
   array made up of 72/m APA102 RGB LEDs. This density was chosen as it results in a square
   grid of LEDs when the strips are laid ajacent to each other.
   
   User interaction is via the Serial1 interface. The Serial1 baudrate is 9600.
   The interface provides a simple menu selection which should work in most
   terminal emulation programs.

   I use an HC-05 Bluetooth serial module on serial1.

   Also used is a 4-channel 433 Mhz keychain transmitter receiver pair to select on-demand messages,
   but this can be replaced by pushbuttons if desired.
   

*/

// for reading flash memory
#include <avr/pgmspace.h>
// for using eeprom memory
#include <avr/eeprom.h>
#include <Time.h>             //http://www.pjrc.com/teensy/arduino_libraries/Time.zip
#include <Wire.h>             // built-in
#include <DS3232RTC.h>        //http://github.com/JChristensen/DS3232RTC
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>

// memory size of the internal EEPROM in bytes
#define EEPROM_BYTES 4096

// pins used by 4-channel 433 MHz radio module
#define RADIO_PIN_0 A0
#define RADIO_PIN_1 A1
#define RADIO_PIN_2 A2
#define RADIO_PIN_3 A3

// pin used to select either EEPROM program mode or radio-control mode
#define MODE_PIN 14

// pin used to put HC-05 bluetooth module into AT mode
#define BT_AT_PIN 12

// Change the next 6 defines to match your matrix type and size
#define LED_PIN        5
#define CLOCK_PIN      7
#define COLOR_ORDER    BGR
#define CHIPSET        APA102

#define MATRIX_WIDTH   44
// negative number here because we start in the upper left vs lower right
#define MATRIX_HEIGHT  -8
#define MATRIX_TYPE    HORIZONTAL_ZIGZAG_MATRIX

cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_TYPE> leds;

cLEDText ScrollingMsg;

// how many strings
#define NUM_STRINGS 8
// how many message slots
#define NUM_SLOTS 16

// message string size
#define MAX_SIZE 384
// display string size
#define DISPLAY_SIZE 512
// command string size
#define COMMAND_SIZE 64

// default message time in seconds
#define DEFAULT_TIME 5

// time in ms between advancing by 1 pixel when scrolling
#define SCROLL_SPEED 10

// time in ms between messages
#define INTER_DELAY 10

// Serial1 baudrate
#define Serial1_BAUD 9600

// compare this to what is stored in the eeprom to see if it has been cleared
#define EE_MAGIC_NUMBER 0xBADC

// number of text fornatting commands
#define NUM_COMMANDS 52

// for tracking time in delay loops
// global so it can be used in any function
unsigned long previousMillis;

// slot
byte slot;

// message number
byte msgNum;

// message time
byte msgTime;

// message style
byte msgStyle;

// a global to contain Serial1 menu input
char menuChar;

// global to track main menu display
byte mainMenuCount = 0;

// global buffer for a string copied from internal FLASH or EEPROM
// initialize to '0'
char currentString[MAX_SIZE] = {0};
// global buffer for string to be sent to the display
// combines commands with the string read from EEPROM
char displayString[DISPLAY_SIZE] = {0};
// buffer for command string read from FLASH
char commandString[COMMAND_SIZE] = {0};

// RAM copy of eeprom magic number
uint16_t magicNumber;

// eeprom message string
uint8_t EEMEM ee_msgString0[MAX_SIZE];
uint8_t EEMEM ee_msgString1[MAX_SIZE];
uint8_t EEMEM ee_msgString2[MAX_SIZE];
uint8_t EEMEM ee_msgString3[MAX_SIZE];
uint8_t EEMEM ee_msgString4[MAX_SIZE];
uint8_t EEMEM ee_msgString5[MAX_SIZE];
uint8_t EEMEM ee_msgString6[MAX_SIZE];
uint8_t EEMEM ee_msgString7[MAX_SIZE];

// eeprom magic number to detect if eeprom has been cleared
uint16_t EEMEM ee_magicNumber;

// loop break flag on input
byte breakout = 0;

// arrays stored in RAM
uint8_t msgIndexes[NUM_SLOTS];
uint8_t msgTimes[NUM_SLOTS];
uint8_t msgStyles[NUM_SLOTS];

// arrays stored in EEPROM
uint8_t EEMEM ee_msgIndexes[NUM_SLOTS];
uint8_t EEMEM ee_msgTimes[NUM_SLOTS];
uint8_t EEMEM ee_msgStyles[NUM_SLOTS];

// track if display is on or off
uint8_t isDisplayOn = 1;

// track if we are in on-demand (RF remote) mode
uint8_t isRfMode = 0;

// track if time should be displayed
uint8_t isDisplayTime = 0;

// store the last second
// so we can update the display as soon as we get a new second
static uint8_t prevSecond = 61;

// temporary time variable
time_t t;
tmElements_t tm;

// commands stored in FLASH
// scroll left solid colors
// 0 white text
const char command0[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x01\x00\xff" };
// 1 grey text
const char command1[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x00\x00\x7f" };
// 2 red text
const char command2[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x00\xff\xff" };
// 3 orange text
const char command3[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x10\xff\xff" };
// 4 yellow text
const char command4[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x40\xff\xff" };
// 5 yellow-green text
const char command5[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x50\xff\xff" };
// 6 green text
const char command6[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x60\xff\xff" };
// 7 cyan text
const char command7[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\xa0\xff\xff" };
// 8 blue text
const char command8[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\xb0\xff\xff" };
// 9 purple text
const char command9[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\xc0\xff\xff" };
// 10 violet text
const char command10[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\xd0\xff\xff" };
// 11 light red
const char command11[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x00\x7f\xff" };
// 12 light yellow
const char command12[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x40\x7f\xff" };
// 13 light green
const char command13[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\x60\x7f\xff" };
// 14 light cyan
const char command14[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\xa0\x7f\xff" };
// 15 light blue
const char command15[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV "\xb0\x7f\xff" };


// scroll up solid colors
// 16 white text
const char command16[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x01\x00\xff" };
// 17 red text
const char command17[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x00\xff\xff" };
// 18 orange text
const char command18[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x10\xff\xff" };
// 19 yellow text
const char command19[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x40\xff\xff" };
// 20 yellow-green text
const char command20[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x50\xff\xff" };
// 21 green text
const char command21[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x60\xff\xff" };
// 22 cyan text
const char command22[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\xa0\xff\xff" };
// 23 blue text
const char command23[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\xb0\xff\xff" };
// 24 purple text
const char command24[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\xc0\xff\xff" };
// 25 violet text
const char command25[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\xd0\xff\xff" };
// 26 light red
const char command26[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x00\x7f\xff" };
// 27 light yellow
const char command27[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x40\x7f\xff" };
// 28 light green
const char command28[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\x60\x7f\xff" };
// 29 light cyan
const char command29[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\xa0\x7f\xff" };
// 30 light blue
const char command30[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\xb0\x7f\xff" };
// 31 light purple
const char command31[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV "\xff\x7f\xff" };

// scroll left gradients
// vertical hot text
const char command32[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_CV "\x01\xff\xff\x40\xff\xff" };
// horizontal hot text
const char command33[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_AH "\x01\xff\xff\x40\xff\xff" };
// vertical rainbow text
const char command34[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_CV "\xff\xff\xff\x10\xff\xff" };
// horizontal rainbow text
const char command35[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_AH "\xff\xff\xff\x10\xff\xff" };
// vertical spring text
const char command36[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_CV "\x40\xff\xff\x60\xff\xff" };
// horizontal spring text
const char command37[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_AH "\x40\xff\xff\x60\xff\xff" };
// vertical blues text
const char command38[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_CV "\xa0\xff\xff\xb0\xff\xff" };
// horizontal blues text
const char command39[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_AH "\xa0\xff\xff\xb0\xff\xff" };
// vertical dusk text
const char command40[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_CV "\xc0\xff\xff\xff\xff\xff" };
// horizontal dusk text
const char command41[] PROGMEM = { EFFECT_SCROLL_LEFT EFFECT_HSV_AH "\xc0\xff\xff\xff\xff\xff" };

// scroll up gradients
// vertical hot text
const char command42[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_CV "\x01\xff\xff\x40\xff\xff" };
// horizontal hot text
const char command43[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_AH "\x01\xff\xff\x40\xff\xff" };
// vertical rainbow text
const char command44[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_CV "\xff\xff\xff\x10\xff\xff" };
// horizontal rainbow text
const char command45[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_AH "\xff\xff\xff\x10\xff\xff" };
// vertical spring text
const char command46[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_CV "\x40\xff\xff\x60\xff\xff" };
// horizontal spring text
const char command47[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_AH "\x40\xff\xff\x60\xff\xff" };
// vertical blues text
const char command48[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_CV "\xa0\xff\xff\xb0\xff\xff" };
// horizontal blues text
const char command49[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_AH "\xa0\xff\xff\xb0\xff\xff" };
// vertical dusk text
const char command50[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_CV "\xc0\xff\xff\xff\xff\xff" };
// horizontal dusk text
const char command51[] PROGMEM = { EFFECT_SCROLL_UP EFFECT_HSV_AH "\xc0\xff\xff\xff\xff\xff" };

// array of commands stored in FLASH
const char * const styles[] PROGMEM = {
  command0,
  command1,
  command2,
  command3,
  command4,
  command5,
  command6,
  command7,
  command8,
  command9,
  command10,
  command11,
  command12,
  command13,
  command14,
  command15,
  command16,
  command17,
  command18,
  command19,
  command20,
  command21,
  command22,
  command23,
  command24,
  command25,
  command26,
  command27,
  command28,
  command29,
  command30,
  command31,
  command32,
  command33,
  command34,
  command35,
  command36,
  command37,
  command38,
  command39,
  command40,
  command41,
  command42,
  command43,
  command44,
  command45,
  command46,
  command47,
  command48,
  command49,
  command50
};

const unsigned char fastScroll[] = { EFFECT_FRAME_RATE "\x01" };
const unsigned char slowScroll[] = { EFFECT_FRAME_RATE "\x06" };
const unsigned char keepBackground[] = { EFFECT_BACKGND_LEAVE };
const unsigned char eraseBackground[] = { EFFECT_BACKGND_ERASE };


//// test stuff
//const unsigned char TxtDemo1[] = { "        HELLO!!" };
//const unsigned char TxtDemo2[] = { EFFECT_HSV "\x00\xff\xff" "R" EFFECT_HSV "\x20\xff\xff" "A" EFFECT_HSV "\x40\xff\xff" "I" EFFECT_HSV "\x60\xff\xff" "N" EFFECT_HSV "\xe0\xff\xff" "B" EFFECT_HSV "\xc0\xff\xff" "O"
//                                  EFFECT_HSV "\xa0\xff\xff" "W" };

//---------------------------------------------------------------------------------------------//
// setup
//---------------------------------------------------------------------------------------------//
void setup()
{
  // set up the Serial1 ports
  Serial1.begin(Serial1_BAUD);

  // set up inputs
  pinMode(RADIO_PIN_0, INPUT);
  pinMode(RADIO_PIN_1, INPUT);
  pinMode(RADIO_PIN_2, INPUT);
  pinMode(RADIO_PIN_3, INPUT);
  pinMode(MODE_PIN, INPUT); 
  digitalWrite(MODE_PIN, HIGH);

  // see if the eeprom has been cleared
  // if it has, initialize it with default values
  magicNumber = eeprom_read_word(&ee_magicNumber);
  if (magicNumber != EE_MAGIC_NUMBER)
  {
    initEeprom();
  }

  // initialize the display
  FastLED.addLeds<CHIPSET, LED_PIN, CLOCK_PIN, COLOR_ORDER>(leds[0], leds.Size());
  // brightness 0 - 255; set to half-bright
  FastLED.setBrightness(128);
  FastLED.clear(true);
  // cycle through LED dies for self-test 
  FastLED.showColor(CRGB::Red);
  delay(1000);
  FastLED.showColor(CRGB::Green);
  delay(1000);
  FastLED.showColor(CRGB::Blue);
  delay(1000);
  FastLED.showColor(CRGB::White);
  delay(1000);
  FastLED.clear(true);
  

  // read the values from the EEPROM into the arrays stored in RAM
  eeprom_read_block((void*)&msgIndexes, (const void*)&ee_msgIndexes, sizeof(msgIndexes));
  eeprom_read_block((void*)&msgTimes, (const void*)&ee_msgTimes, sizeof(msgTimes));
  eeprom_read_block((void*)&msgStyles, (const void*)&ee_msgStyles, sizeof(msgStyles));
  
  // set the local time provider
  setSyncProvider(RTC.get);

  // set the font and init the display
  ScrollingMsg.SetFont(MatriseFontData);
  ScrollingMsg.Init(&leds, leds.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  ScrollingMsg.SetText((unsigned char *)currentString, sizeof(currentString) - 1);
  ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);

} // end setup

//---------------------------------------------------------------------------------------------//
// main loop
//---------------------------------------------------------------------------------------------//
void loop()
{
  // check mode switch
  if (digitalRead(MODE_PIN) == LOW)
  {
    // use the RF remote
    isRfMode = 1;
  }
  else
  {
    // follow the program stored in the EEPROM
    isRfMode = 0;
  }
  
  // if there is something in the Serial1 buffer read it
  if (Serial1.available() >  0)
  {
    // print the main menu once
    if (mainMenuCount == 0)
    {
      clearAndHome();
      Serial1.println(F("Message Box - Main Menu"));
      Serial1.println(F("----------------------"));
      Serial1.println(F("1     Show String"));
      Serial1.println(F("2     Edit String"));
      Serial1.println(F("3     Instant Message"));
      Serial1.println(F("4     Edit Program"));
      Serial1.println(F("5     Display Program"));
      Serial1.println(F("6     Display On/Off"));
      Serial1.println(F("7     Set Time"));
      Serial1.println(F("8     Show Time"));
      Serial1.print(F("x     Exit setup"));
      Serial1.println();
      mainMenuCount = 1;
    }

    menuChar = Serial1.read();
    if (menuChar == '1')
    {
      // show the string
      clearAndHome();
      showString();
    }
    else if (menuChar == '2')
    {
      // edit the string
      clearAndHome();
      editString();
    }
    else if (menuChar == '3')
    {
      // send message to the display right now
      Serial1.println(F("Instant mode selected."));
      Serial1.println(F("Enter your message: "));
      // get a string from Serial1 for currentString
      getSerial1String();
      Serial1.println();
      Serial1.println(F("Current message: "));
      Serial1.println(currentString);
      // clear the matrix
      FastLED.clear(true);
      // display it on the matrix for 60 seconds
      displayMsg(0, 60);
      FastLED.clear(true);
    }
    else if (menuChar == '4')
    {
      // edit the program
      clearAndHome();
      editProgram();
    }
    else if (menuChar == '5')
    {
      // display the program
      clearAndHome();
      displayProgram();
    }
    else if (menuChar == '6')
    {
      // toggle display on or off
      if (isDisplayOn == 0)
      {
        // not sure how to handle this yet
        isDisplayOn = 1;
      }
      else
      {
        // lcd.display();
        isDisplayOn = 0;
      }
    }
    else if (menuChar == '7')
    {
      // set the time
      clearAndHome();
      setTheTime();
    }
    else if (menuChar == '8')
    {
      // show the time
      clearAndHome();
      printDateTime(now());
    }
    else if (menuChar == 'x')
    {
      mainMenuCount = 1;
    }
  }
  // reset the main menu if the Serial1 connection drops
  mainMenuCount = 0;

  Serial1.flush();

  // follow the display program or wait for pins set by RF receiver

  // RF remote mode
  if (isRfMode)
  {
    if (digitalRead(RADIO_PIN_0) == HIGH)
    {
      buttonA();
    }
    if (digitalRead(RADIO_PIN_1) == HIGH)
    {
      buttonB();
    }
    if (digitalRead(RADIO_PIN_2) == HIGH)
    {
      buttonC();
    }
    if (digitalRead(RADIO_PIN_3) == HIGH)
    {
      buttonD();
    }
  }
  // EEPROM program mode
  // slot
  else
  {
    for (slot = 0; slot < NUM_SLOTS; slot++)
    {
      // msg number
      msgNum = msgIndexes[slot];
      // msg time
      msgTime = msgTimes[slot];
      // msg style
      msgStyle = msgStyles[slot];
      // if msgTime = 0 skip the message
      if (msgTime > 0)
      {
        // fetch the string from EEPROM
        readEepromBlock(msgNum);
        // clear the matrix
        FastLED.clear(true);
        // display it on the matrix
        // if the string begins with "!time" display the time
        // the time is displayed for msgTime seconds
        if (strstr(currentString, "!time") != NULL)
        {   
          // set flag
          isDisplayTime = 1;
          // 1st time display
          displayTheTime(msgStyle);
          // grab now
          previousMillis = millis();
          // loop while displaying the time each seconds for msgTime seconds
          while (millis() - previousMillis < (msgTime * 1000))
          {
            // update the time
            if (isDisplayTime)
            {
              // only if the second has changed
              if (second() != prevSecond)
              {
                displayTheTime(msgStyle);
              }
            }       
          }
        }
        // otherwise show the message directly
        else
        {
          isDisplayTime = 0;
          displayMsg(msgStyle, msgTime);
        }
  
        FastLED.clear(true);
        // pause between messages
        previousMillis = millis();
        while (millis() - previousMillis < INTER_DELAY)
        {
          if (Serial1.available() > 0)
          {
            breakout = 1;
            // break the delay loop
            break;
          }
        }
        if (breakout == 1)
        {
          breakout = 0;
          // break the message program loop
          break;
        }
      }
    }
  }
}

//---------------------------------------------------------------------------------------------//
// function clearAndHome
// returns the cursor to the home position and clears the screen
//---------------------------------------------------------------------------------------------//
void clearAndHome()
{
  Serial1.write(27); // ESC
  Serial1.print("[2J"); // clear screen
  Serial1.write(27); // ESC
  Serial1.print("[H"); // cursor to home
}

//---------------------------------------------------------------------------------------------//
// function showString
// displays the string stored in EEPROM
//---------------------------------------------------------------------------------------------//
void showString()
{
  byte msgCount = 0;

  // clear the terminal
  clearAndHome();

  Serial1.flush();

  // clear the string in RAM
  sprintf(currentString, "");

  // display the menu on entry
  Serial1.println(F("Here are the strings"));
  Serial1.println(F("that are stored in the EEPROM:"));
  Serial1.println(F("-------------------------"));
  Serial1.println();
  // read the string stored in EEPROM starting at address 0 into the RAM string
  while (msgCount < NUM_STRINGS)
  {
    readEepromBlock(msgCount);
    Serial1.print("String ");
    Serial1.print(msgCount, DEC);
    Serial1.print(": ");
    Serial1.println(currentString);
    msgCount++;
  }

  Serial1.println();
  Serial1.println(F("x    return to Main Menu"));

  // poll Serial1 until exit
  while (1)
  {
    // if there is something in the Serial1 buffer read it
    if (Serial1.available() >  0)
    {
      menuChar = Serial1.read();
      if (menuChar == 'x')
      {
        // set flag to redraw menu
        mainMenuCount = 0;
        // return to main menu and return the mode
        return;
      }
    }
  }
}

//---------------------------------------------------------------------------------------------//
// function displayProgram
// shows the program stored in the EEPROM
//---------------------------------------------------------------------------------------------//
void displayProgram()
{
  // message values
  int slot;
  int msgNum;
  int msgTime;
  int msgStyle;

  Serial1.flush();

  Serial1.println(F("MessageBox - Display Message Program"));
  Serial1.println(F("This is a list of message slots, times"));
  Serial1.println(F("and messages."));
  Serial1.println(F("----------------------------------"));
  Serial1.println(F("slot:msgNum:msgTime:msgStyle:Message"));

  for (slot = 0; slot < NUM_SLOTS; slot++)
  {
    msgNum = msgIndexes[slot];
    msgTime = msgTimes[slot];
    msgStyle = msgStyles[slot];
    // clear the string in RAM
    sprintf(currentString, "");
    readEepromBlock(msgNum);
    Serial1.print(slot, DEC);
    Serial1.print(":");
    Serial1.print(msgNum, DEC);
    Serial1.print(":");
    Serial1.print(msgTime, DEC);
    Serial1.print(":");
    Serial1.print(msgStyle, DEC);
    Serial1.print(":");
    Serial1.println(currentString);
  }
  Serial1.println();
  Serial1.println(F("x    return to Main Menu"));

  // poll Serial1 until exit
  while (1)
  {
    // if there is something in the Serial1 buffer read it
    if (Serial1.available() >  0)
    {
      menuChar = Serial1.read();
      if (menuChar == 'x')
      {
        // set flag to redraw menu
        mainMenuCount = 0;
        // return to main menu and return the mode
        sprintf(currentString, "");
        return;
      }
    }
  }
}

//---------------------------------------------------------------------------------------------//
// function editProgram
// edits the display program stored in the EEPROM
//---------------------------------------------------------------------------------------------//
void editProgram()
{
  // clear the terminal
  clearAndHome();
  // track menu display
  byte displayEditProgramMenu = 0;

  // message values
  int slot;
  int msgNum;
  int msgTime;
  int msgStyle;

  // input char
  char menuChar;

  // input valid flag
  byte inputBad = 1;

  // loop flag
  byte loopFlag = 1;

  Serial1.flush();

  // display the menu on entry
  if (displayEditProgramMenu == 0)
  {
    Serial1.println(F("MessageBox - Edit Message Program"));
    Serial1.println(F("This is a list of message slots and message times."));
    Serial1.print(F("If a message is bigger than "));
    Serial1.print("44");
    Serial1.println(F(" pixels it will scroll."));
    Serial1.println(F("Otherwise, it displays directly."));
    Serial1.println(F("To skip a message set the time to zero."));
    Serial1.println(F("---------------------------"));
    Serial1.println(F("Slots available: "));
    Serial1.print("0 to ");
    Serial1.println(NUM_SLOTS - 1, DEC);

    displayEditProgramMenu = 1;
  }

  // loop until we return from the function
  while (1)
  {
    Serial1.println(F("Enter the number of the slot to edit: "));
    // loop until the input is acceptable
    while (inputBad)
    {
      slot = getSerial1Int();
      if ((slot >= 0) && (slot < NUM_SLOTS))
      {
        inputBad = 0;
      }
      else
      {
        Serial1.println(F("Error: slot "));
        Serial1.print(slot);
        Serial1.println(F(" is out of range. Try again."));

      }
    }
    // reset the input test flag for the next time around
    inputBad = 1;

    // show the choice since no echo
    Serial1.println(F("Slot: "));
    Serial1.println(slot);
    
    Serial1.println(F("Enter the message number: "));
    // loop until the input is acceptable
    while (inputBad)
    {
      msgNum = getSerial1Int();
      if ((msgNum >= 0) && (msgNum < NUM_STRINGS))
      {
        inputBad = 0;
      }
      else
      {
        Serial1.println(F("Error: message "));
        Serial1.print(msgNum);
        Serial1.println(F(" is out of range. Try again."));

      }
    }
    // reset the input test flag for the next time around
    inputBad = 1;

    // fetch the string from EEPROM
    readEepromBlock(msgNum);

    // print the string to Serial1
    Serial1.println(F("Message "));
    Serial1.print(msgNum);
    Serial1.print(": ");
    Serial1.println(currentString);
    
    Serial1.println(F("Enter the message time in seconds 0-255: "));

    // loop until the input is acceptable
    while (inputBad)
    {
      msgTime = getSerial1Int();
      if ((msgTime >= 0) && (msgTime < 256))
      {
        inputBad = 0;
      }
      else
      {
        Serial1.println(F("Error: Time "));
        Serial1.print(msgTime);
        Serial1.println(F(" is out of range. Try again."));
      }
    }
    // reset the input test flag for the next time around
    inputBad = 1;

    // show the choice since no echo
    Serial1.println(F("Time: "));
    Serial1.println(msgTime);

    Serial1.print(F("Enter the message style 0-"));
    Serial1.print(NUM_COMMANDS);
    Serial1.println(F(":"));
    Serial1.println(F("Enter 999 to see list of styles"));

    // loop until the input is acceptable
    while (inputBad)
    {
      msgStyle= getSerial1Int();
      if ((msgStyle >= 0) && (msgStyle < NUM_COMMANDS))
      {
        inputBad = 0;
      }
      else    
      {
        if (msgStyle == 999)
        {
          Serial1.println(F("0 white text scroll left"));
          Serial1.println(F("1 grey text scroll left"));
          Serial1.println(F("2 red text scroll left"));
          Serial1.println(F("3 orange text scroll left"));
          Serial1.println(F("4 yellow text scroll left"));
          Serial1.println(F("5 yellow-green text scroll left"));
          Serial1.println(F("6 green text scroll left"));
          Serial1.println(F("7 cyan text scroll left"));
          Serial1.println(F("8 blue text scroll left"));
          Serial1.println(F("9 purple text scroll left"));
          Serial1.println(F("10 violet text scroll left"));
          Serial1.println(F("11 light red scroll left"));
          Serial1.println(F("12 light yellow scroll left"));
          Serial1.println(F("13 light green scroll left"));
          Serial1.println(F("14 light cyan scroll left"));
          Serial1.println(F("15 light blue scroll left"));
          Serial1.println(F("16 white text scroll up"));
          Serial1.println(F("17 red text scroll up"));
          Serial1.println(F("18 orange text scroll up"));
          Serial1.println(F("19 yellow text scroll up"));
          Serial1.println(F("20 yellow-green text scroll up"));
          Serial1.println(F("21 green text scroll up"));
          Serial1.println(F("22 cyan text scroll up"));
          Serial1.println(F("23 blue text scroll up"));
          Serial1.println(F("24 purple text scroll up"));
          Serial1.println(F("25 violet text scroll up"));
          Serial1.println(F("26 light red scroll up"));
          Serial1.println(F("27 light yellow scroll up"));
          Serial1.println(F("28 light green scroll up"));
          Serial1.println(F("29 light cyan scroll up"));
          Serial1.println(F("30 light blue scroll up"));
          Serial1.println(F("31 light purple scroll up"));
          Serial1.println(F("32 vertical hot text scroll left"));
          Serial1.println(F("33 horizontal hot text scroll left"));
          Serial1.println(F("34 vertical rainbow text scroll left"));
          Serial1.println(F("35 horizontal rainbow text scroll left"));
          Serial1.println(F("36 vertical spring text scroll left"));
          Serial1.println(F("37 horizontal spring text scroll left"));
          Serial1.println(F("38 vertical blues text scroll left"));
          Serial1.println(F("39 horizontal blues text scroll left"));
          Serial1.println(F("40 vertical dusk text scroll left"));
          Serial1.println(F("41 horizontal dusk text scroll left"));
          Serial1.println(F("42 vertical hot text scroll up"));
          Serial1.println(F("43 horizontal hot text scroll up"));
          Serial1.println(F("44 vertical rainbow text scroll up"));
          Serial1.println(F("45 horizontal rainbow text scroll up"));
          Serial1.println(F("46 vertical spring text scroll up"));
          Serial1.println(F("47 horizontal spring text scroll up"));
          Serial1.println(F("48 vertical blues text scroll up"));
          Serial1.println(F("49 horizontal blues text scroll up"));
          Serial1.println(F("50 vertical dusk text scroll up"));
          Serial1.println(F("51 horizontal dusk text scroll up")); 
        }
        else
        {
          Serial1.println(F("Error: Style "));
          Serial1.print(msgStyle);
          Serial1.println(F(" is out of range. Try again."));
        }
      }
    }
    // reset the input test flag for the next time around
    inputBad = 1;

    // show the choice since no echo
    Serial1.println(F("Style: "));
    Serial1.println(msgStyle);
    
    // write the data to the eeprom
    Serial1.println(F("Writing data to eeprom..."));
    // write the message number into the slot
    msgIndexes[slot] = msgNum;
    // write the message time into the slot
    msgTimes[slot] = msgTime;
    // write the message style into the slot
    msgStyles[slot] = msgStyle;

    Serial1.println(F("Edit another? (y/n): "));
    
    while (loopFlag)
    {
      if (Serial1.available() > 0)
      {
        menuChar = Serial1.read();
        if (menuChar == 'n')
        {
          Serial1.println(menuChar);

          // write the program to the EEPROM before we return
          eeprom_write_block((const void*)&msgIndexes, (void*) &ee_msgIndexes, sizeof(msgIndexes));
          eeprom_write_block((const void*)&msgTimes, (void*) &ee_msgTimes, sizeof(msgTimes));
          eeprom_write_block((const void*)&msgStyles, (void*) &ee_msgStyles, sizeof(msgStyles));
          return;
        }
        if (menuChar == 'y')
        {
          loopFlag = 0;
          Serial1.println(menuChar);
          Serial1.flush();
        }
      }
    }
    loopFlag = 1;
  }
  return;
}


//---------------------------------------------------------------------------------------------//
// function editString
// edits the string and writes it to EEPROM
//---------------------------------------------------------------------------------------------//
void editString()
{

  // track how many characters entered
  byte cCount;
  // track menu display
  byte displayEditProgramMenu = 0;
  // input valid flag
  byte inputBad = 1;
  // slot number
  int msgNum;
  // track when string is done
  byte stringDone = 0;

  // clear the terminal
  clearAndHome();

  // clear the string in RAM
  sprintf(currentString, "");

  Serial1.flush();

  // display the menu on entry
  if (displayEditProgramMenu == 0)
  {
    // display the menu on entry
    Serial1.println(F("Enter a new string to be stored in EEPROM"));
    Serial1.println(F("up to "));
    Serial1.print(MAX_SIZE - 1, DEC);
    Serial1.println(F(" characters."));
    Serial1.println(F("-------------------------"));
    Serial1.print(F("Choose a message 0 to "));
    Serial1.println(NUM_STRINGS - 1, DEC);
    Serial1.println(F(" or enter "));
    Serial1.print(NUM_STRINGS);
    Serial1.print(F(" to exit"));
    Serial1.println();
  }

  // poll Serial1 until exit
  while (1)
  {
    // set the string index to 0 each time through the loop
    cCount = 0;

    Serial1.print(F("Enter the number of the message to edit: "));
    Serial1.println();
    // loop until the input is acceptable
    while (inputBad)
    {
      msgNum = getSerial1Int();
      // slots 0 to NUM_STRINGS
      // 0 is ok as it means exit
      if ((msgNum >= 0) && (msgNum <= NUM_STRINGS))
      {
        inputBad = 0;
      }
      else
      {
        Serial1.print(F("Error: message "));
        Serial1.print(msgNum);
        Serial1.println(F(" is out of range. Try again."));
      }
    }
    // reset the input test flag for the next time around
    inputBad = 1;

    // the user wants to edit a slot
    if (msgNum < NUM_STRINGS)
    {
      // show the choice since no echo
      Serial1.print(F("Message: "));
      Serial1.println(msgNum);

      // get the string for currentString
      getSerial1String();
      // reset string done flag
      stringDone = 0;
      // write the string to the EEPROM
      writeEepromBlock(msgNum);
      // display the string
      Serial1.println();
      Serial1.println(F("You entered: "));
      Serial1.println(currentString);
      Serial1.println(F("<y> to enter another or"));
      Serial1.println(F("<n> return to Main Menu"));
      Serial1.flush();
      while (menuChar != 'y')
      {
        if (Serial1.available() > 0)
        {
          menuChar = Serial1.read();
          if (menuChar == 'n')
          {
            Serial1.println(menuChar);
            // set flag to redraw menu
            mainMenuCount = 0;
            // return to main menu
            return;
          }
          if (menuChar == 'y')
          {
            Serial1.println(menuChar);
          }
        }
      } // end of the y/n input loop
    }
    // the user did not want to edit anything
    else
    {
      break;
    }
  } // end of the edit string loop
  // set flag to redraw menu
  mainMenuCount = 0;
  // return to main menu
  return;
}

//---------------------------------------------------------------------------------------------//
// function getSerial1Int
// uses Serial1 input to get an integer
//---------------------------------------------------------------------------------------------//
int getSerial1Int()
{
  char inChar;
  int in;
  int input = 0;

  Serial1.flush();
  do
  // at least once
  {
    while (Serial1.available() > 0)
    {
      inChar = Serial1.read();
      // echo the input
      Serial1.print(inChar);
      // convert 0-9 character to 0-9 int
      in = inChar - '0';
      if ((in >= 0) && (in <= 9))
      {
        // since numbers are entered left to right
        // the current number can be shifted to the left
        // to make room for the new digit by multiplying by ten
        input = (input * 10) + in;
      }
    }
  }
  // stop looping when an ^M is received
  while (inChar != 13);
  // return the number
  return input;
}

//---------------------------------------------------------------------------------------------//
// function getSerial1String
// uses Serial1 input to get a string
//---------------------------------------------------------------------------------------------//
void getSerial1String()
{
  // track how many characters entered
  byte cCount = 0;
  // input valid flag
  byte inputBad = 1;
  // track when string is done
  byte stringDone = 0;

  // clear the string in RAM
  sprintf(currentString, "");

  Serial1.flush();
  // loop until done
  while (stringDone == 0)
  {
    // if there is something in the Serial1 buffer read it
    while (Serial1.available() >  0)
    {
      // grab a character
      menuChar = Serial1.read();
      // echo the input
      Serial1.print(menuChar);

      // do stuff until we reach MAX_SIZE
      if (cCount < (MAX_SIZE - 1))
      {
        // pressed <ENTER> (either one)
        if ((menuChar == 3) || (menuChar == 13))
        {
          // set flag to redraw menu
          mainMenuCount = 0;
          // make the last character a null
          // cCount++;
          currentString[cCount] = 0;
          // mark the string done
          stringDone = 1;
        }
        // if we are not at the end of the string and <delete> or <backspace> not pressed
        else if ((menuChar != 127) || (menuChar != 8))
        {
          currentString[cCount] = menuChar;
          cCount++;
        }
        // if index is between start and end and delete or backspace is pressed
        // clear the current character and go back one in the index
        else if ((cCount > 0) && ((menuChar == 127) || (menuChar == 8)))
        {
          currentString[cCount] = 0;
          cCount--;
          // print a backspace to the screen so things get deleted
          Serial1.write(8);
        }
      }
      // we reached MAX_SIZE
      else
      {
        // set flag to redraw menu
        mainMenuCount = 0;
        // set the current character to null
        currentString[cCount] = 0;
        // mark the string as done
        stringDone = 1;
      }
    }
  } // end of the string input loop

  return;
}



//---------------------------------------------------------------------------------------------//
// function readEeepromBlock
// reads a block from eeprom into a char arry
// uses globals
// returns nothing
//---------------------------------------------------------------------------------------------//
void readEepromBlock(byte msgNum)
{
  switch (msgNum)
  {
    case 0:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString0, sizeof(currentString));
      break;
    case 1:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString1, sizeof(currentString));
      break;
    case 2:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString2, sizeof(currentString));
      break;
    case 3:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString3, sizeof(currentString));
      break;
    case 4:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString4, sizeof(currentString));
      break;
    case 5:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString5, sizeof(currentString));
      break;
    case 6:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString6, sizeof(currentString));
      break;
    case 7:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString7, sizeof(currentString));
      break;
    default:
      break;
  }
}

//---------------------------------------------------------------------------------------------//
// function writeEeepromBlock
// writes a block from a char array into the eeprom
// uses globals
// returns nothing
//---------------------------------------------------------------------------------------------//
void writeEepromBlock(byte msgNum)
{
  switch (msgNum)
  {
    case 0:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString0, sizeof(currentString));
      break;
    case 1:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString1, sizeof(currentString));
      break;
    case 2:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString2, sizeof(currentString));
      break;
    case 3:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString3, sizeof(currentString));
      break;
    case 4:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString4, sizeof(currentString));
      break;
    case 5:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString5, sizeof(currentString));
      break;
    case 6:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString6, sizeof(currentString));
      break;
    case 7:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString7, sizeof(currentString));
      break;
    default:
      break;
  }
}

//---------------------------------------------------------------------------------------------//
// function initEeprom
// loads default values into the eeprom if nothing is present based on ee_magicNumber
// not having the correct value
//---------------------------------------------------------------------------------------------//
void initEeprom()
{

  int slot;
  // make all slots default to message 0 with 0 seconds
  for (slot = 1; slot < (NUM_SLOTS - 1); slot++)
  {
    // write the message number into the slot
    // msg number
    msgIndexes[slot] = 0;
    // msg time
    msgTimes[slot] = 0;
    // msg style
    msgTimes[slot] = 0;
  }
  // set the first slot to whatever the default time is
  msgTimes[0] = DEFAULT_TIME;

  // set the default messages to blank except the first
  sprintf(currentString, "MESSAGE ONE");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString0, sizeof(currentString));
  sprintf(currentString, "!time");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString1, sizeof(currentString));
  sprintf(currentString, "MESSAGE THREE");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString2, sizeof(currentString));
  sprintf(currentString, "MESSAGE FOUR");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString3, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString4, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString5, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString6, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString7, sizeof(currentString));

  // set the magic number
  eeprom_write_word(&ee_magicNumber, EE_MAGIC_NUMBER);

  // copy the arrays in RAM to the EEPROM
  eeprom_write_block((const void*)&msgIndexes, (void*) &ee_msgIndexes, sizeof(msgIndexes));
  eeprom_write_block((const void*)&msgTimes, (void*) &ee_msgTimes, sizeof(msgTimes));
  eeprom_write_block((const void*)&msgStyles, (void*) &ee_msgStyles, sizeof(msgStyles));

  // read it back into RAM
  eeprom_read_block((void*)&msgIndexes, (const void*)&ee_msgIndexes, sizeof(msgIndexes));
  eeprom_read_block((void*)&msgTimes, (const void*)&ee_msgTimes, sizeof(msgTimes));
  eeprom_read_block((void*)&msgStyles, (const void*)&ee_msgStyles, sizeof(msgStyles));

  sprintf(currentString, "");
}

//---------------------------------------------------------------------------------------------//
// function displayMsg()
// displays a string to the LED array
// expects byte for text display style, and byte for display time in seconds
// return nothing
//---------------------------------------------------------------------------------------------//
void displayMsg(uint8_t style, uint8_t displayTime)
{

  // slow down the scrolling for vertical
  if (((style > 15) && (style < 32)) || ((style > 41) && (style < 52)))
  {
    ScrollingMsg.SetText((unsigned char *)slowScroll, sizeof(slowScroll - 1));
    ScrollingMsg.UpdateText();
  }
  else
  {
    ScrollingMsg.SetText((unsigned char *)fastScroll, sizeof(slowScroll - 1));
    ScrollingMsg.UpdateText();
  }
  
  // load the command string into the display string
  strcpy_P(displayString, (char*)pgm_read_word(&(styles[style])));

  // add the text to be displayed to the of the string
  strcat(displayString, currentString);

  uint16_t index = 0;
  uint32_t loopMillis = 0;
  
  // find where the string ends
  while (index < MAX_SIZE)
  {
    if (displayString[index] == '\0')
      break;
    index++;
  }
  
  // if it fits in the display make it static and loop until msgTime seconds expires
  if ((ScrollingMsg.FontWidth() * index) <= MATRIX_WIDTH)
  {
    ScrollingMsg.SetText((unsigned char *)displayString, index);
    ScrollingMsg.UpdateText();
    FastLED.show();

    // loop while updating the time
    loopMillis = millis();
    while (millis() - loopMillis < (displayTime * 1000))
    {   
      if (Serial1.available() > 0)
      {
        breakout = 1;
        // break the delay loop
        break;
      }
    }   
  }
  // otherwise scroll it for displayTime seconds
  else
  {
    loopMillis = millis();
    while ((millis() - loopMillis) < (displayTime * 1000))
    {
      ScrollingMsg.SetText((unsigned char *)displayString, index);
      while (ScrollingMsg.UpdateText() != -1)
      {
        FastLED.show();
        // break out to respond to Serial1
        if (Serial1.available() >  0)
        {
          return;
        }
      }
      ScrollingMsg.UpdateText();
      FastLED.show();
    }
  }
  
  return;
}


//---------------------------------------------------------------------------------------------//
// function setTheTime()
// sets the time by Serial1 data entry
// expects nothing
// returns nothing
//---------------------------------------------------------------------------------------------//
void setTheTime(void)
{

  uint16_t y;
  
  // year
  Serial1.println("Enter the 4-digit year:");
  y = getSerial1Int();
  if (y >= 100 && y < 1000)
    tm.Year = 2000;

  // month
  Serial1.println("Enter the month:");
  tm.Month = getSerial1Int();
  // day
  Serial1.println("Enter the day:");
  tm.Day = getSerial1Int();
  // hour
  Serial1.println("Enter the hour:");
  tm.Hour = getSerial1Int();
  // minute
  Serial1.println("Enter the minute:");
  tm.Minute = getSerial1Int();
  // second
  Serial1.println("Enter the second:");
  tm.Second = getSerial1Int();
  t = makeTime(tm);
  setTime(tm.Hour, tm.Minute, tm.Second, tm.Day, tm.Month, y);
  RTC.set(now());  
  Serial1.print(F("RTC set to: "));
  printDateTime(now());
  Serial1.println();
  while (1)
  {
    // if there is something in the Serial1 buffer read it
    if (Serial1.available() >  0)
    {
      menuChar = Serial1.read();
      if (menuChar == 27)
      {
        // set flag to redraw menu
        mainMenuCount = 0;
        // return to main menu
        return;
      }
    }
  }
}
//---------------------------------------------------------------------------------------------//
// function printDateTime
//---------------------------------------------------------------------------------------------//
//print date and time to Serial1
void printDateTime(time_t t)
{
    printDate(t);
    Serial1.print(" ");
    printTime(t);
}

//---------------------------------------------------------------------------------------------//
// function printTime
//---------------------------------------------------------------------------------------------//
void printTime(time_t t)
{
    printI00(hour(t), ':');
    printI00(minute(t), ':');
    printI00(second(t), ' ');
}

//---------------------------------------------------------------------------------------------//
// function printDate
//---------------------------------------------------------------------------------------------//
void printDate(time_t t)
{
    printI00(day(t), 0);
    Serial1.print(monthShortStr(month(t)));
    Serial1.print(year(t), DEC);
}

//---------------------------------------------------------------------------------------------//
// function printI00
//Print an integer in "00" format (with leading zero),
//followed by a delimiter character to Serial1.
//Input value assumed to be between 0 and 99.
//---------------------------------------------------------------------------------------------//
void printI00(int val, char delim)
{
    if (val < 10) Serial1.print('0');
    Serial1.print(val, DEC);
    if (delim > 0) Serial1.print(delim);
    return;
}

//---------------------------------------------------------------------------------------------//
// function displayTheTime()
// uses the time objects
// example: 12.34.56 Thu Mar 20
// expects nothing
// return nothing
//---------------------------------------------------------------------------------------------//
void displayTheTime(uint8_t style)
{
  // array to hold time 00:00:00 
  char timeString[8] = {0};

  // slow down the scrolling for vertical
  if (((style > 15) && (style < 32)) || ((style > 41) && (style < 52)))
  {
    ScrollingMsg.SetText((unsigned char *)slowScroll, sizeof(slowScroll - 1));
    ScrollingMsg.UpdateText();
  }
  else
  {
    ScrollingMsg.SetText((unsigned char *)fastScroll, sizeof(slowScroll - 1));
    ScrollingMsg.UpdateText();
  }

// // testing
//  ScrollingMsg.SetText((unsigned char *)keepBackground, sizeof(keepBackground - 1));
//  ScrollingMsg.UpdateText();
  
  // update prevSecond
  prevSecond = second();
  
  // hour
  if (hour() < 10)
  {
    // add leading zero
    timeString[0] = '0';
    // get ones
    timeString[1] = char(hour() + 48);
  }
  else
  {
    // get tens
    timeString[0] = char(48 + (hour() / 10));
    // get ones
    timeString[1] = char(48 + (hour() % 10));
  }
  
  // minute
  if (minute() < 10)
  {
    // add leading zero
    timeString[2] = '0';
    // get ones
    timeString[3] = char(48 + minute());
  }
  else
  {
    // get tens
    timeString[2] = char(48 + minute() / 10);
    // get ones
    timeString[3] = char(48 + minute() % 10);
  }
  // divider
  timeString[4] = '.';
  
  // second
  if (second() < 10)
  {
    // add leading zero
    timeString[5] = '0';
    // get ones
    timeString[6] = char(48 + second());
  }
  else
  {
    // get tens
    timeString[5] = char(48 + second() / 10);
    // get ones
    timeString[6] = char(48 + second() % 10);
  }

  timeString[7] = ' ';

  // load the command string into the display string
  strcpy_P(displayString, (char*)pgm_read_word(&(styles[style])));

  // add the text to be displayed to the of the string
  strcat(displayString, timeString);

//  // testing
//  // fill with green
//  // should act like background color
//  for (uint8_t xx = 0; xx < 44; xx++)
//  {
//    for (uint8_t yy = 0; yy < 8; yy++)
//    {
//      leds(xx,yy) = CHSV(128,255,32);
//    }
//  }


  ScrollingMsg.SetText((unsigned char *)displayString, sizeof(displayString) - 1);
  ScrollingMsg.UpdateText();
  FastLED.show();

//  ScrollingMsg.SetText((unsigned char *)eraseBackground, sizeof(eraseBackground - 1));
//  ScrollingMsg.UpdateText();
  
  return;
}

//---------------------------------------------------------------------------------------------//
// function HuePlasmaFrame()
// rainbow plasma background animation
// expects a uint16_t time
// return nothing
//---------------------------------------------------------------------------------------------//
void HuePlasmaFrame(uint16_t Time)
{
  #define PLASMA_X_FACTOR  24
  #define PLASMA_Y_FACTOR  24
  int16_t r, h;
  int x, y;

  for (x=0; x<MATRIX_WIDTH; x++)
  {
    for (y=0; y<-1 * MATRIX_HEIGHT; y++)
    {
      r = sin16(Time) / 256;
      h = sin16(x * r * PLASMA_X_FACTOR + Time) + cos16(y * (-r) * PLASMA_Y_FACTOR + Time) + sin16(y * x * (cos16(-Time) / 256) / 2);
      leds(x, y) = CHSV((h / 256) + 128, 255, 255);
    }
  }
}

//---------------------------------------------------------------------------------------------//
// function buttonA()
// displays the thing for button A on the RF remote
// expects nothing
// return nothing
//---------------------------------------------------------------------------------------------//
void buttonA(void)
{
  const unsigned char myText[] = { EFFECT_HSV "\x00\xff\xff" " T" EFFECT_HSV "\x20\xff\xff" "H" EFFECT_HSV "\x40\xff\xff" "A" EFFECT_HSV "\x60\xff\xff" "N" EFFECT_HSV "\xa0\xff\xff" "K" EFFECT_HSV "\x80\xff\xff" "S" };

  FastLED.clear(true);

  uint32_t loopMillis = 0;
 
  ScrollingMsg.SetText((unsigned char *)myText, sizeof(myText) - 1);
  ScrollingMsg.UpdateText();
  FastLED.show();

  // loop while polling for Serial1 input
  loopMillis = millis();
  while (millis() - loopMillis < (3 * 1000))
  {   
    if (Serial1.available() > 0)
    {
      FastLED.clear(true);
      return;
    }
  }   

  FastLED.clear(true);
  return;
}

//---------------------------------------------------------------------------------------------//
// function buttonB()
// displays the thing for button B on the RF remote
// expects nothing
// return nothing
//---------------------------------------------------------------------------------------------//
void buttonB(void)
{
  const unsigned char myText[] = { EFFECT_SCROLL_LEFT "        " EFFECT_HSV "\x00\xff\xff" "LET'S " EFFECT_HSV "\xff\x00\xff" "GO " EFFECT_HSV "\xa0\xff\xff" "TITANS! " };

  FastLED.clear(true);

  uint32_t loopMillis = 0;
  
  loopMillis = millis();
  while ((millis() - loopMillis) < (3 * 1000))
  {
    ScrollingMsg.SetText((unsigned char *)myText, sizeof(myText) - 1);
    while (ScrollingMsg.UpdateText() != -1)
    {
      FastLED.show();
      // break out to respond to Serial1
      if (Serial1.available() >  0)
      {
        FastLED.clear(true);
        return;
      }
    }
    ScrollingMsg.UpdateText();
    FastLED.show();
  }

  FastLED.clear(true);
  return;
}

//---------------------------------------------------------------------------------------------//
// function buttonC()
// displays the thing for button C on the RF remote
// expects nothing
// return nothing
//---------------------------------------------------------------------------------------------//
void buttonC(void)
{
  const unsigned char myText[] = { EFFECT_HSV "\xff\x00\xff" "SCORE!" };

  FastLED.clear(true);

  uint32_t loopMillis = 0;
  
  ScrollingMsg.SetText((unsigned char *)myText, sizeof(myText) - 1);
  ScrollingMsg.UpdateText();
  FastLED.show();

  // loop while updating the time
  loopMillis = millis();
  while (millis() - loopMillis < (3 * 1000))
  {   
    if (Serial1.available() > 0)
    {
      FastLED.clear(true);
      return;
    }
  }   

  FastLED.clear(true);
  return;
}

//---------------------------------------------------------------------------------------------//
// function buttonD()
// displays the thing for button D on the RF remote
// expects nothing
// return nothing
//---------------------------------------------------------------------------------------------//
void buttonD(void)
{
  const unsigned char myText[] = { EFFECT_SCROLL_LEFT EFFECT_RGB "\xff\xff\xff" EFFECT_BACKGND_ERASE EFFECT_COLR_EMPTY "        MAKE SOME NOISE!!!           " };

  FastLED.clear(true);

  uint32_t loopMillis = 0;

  uint16_t PlasmaTime, PlasmaShift;
  uint16_t OldPlasmaTime;

  loopMillis = millis();
  while ((millis() - loopMillis) < (4 * 1000))
  {
    HuePlasmaFrame(PlasmaTime);
    if (ScrollingMsg.UpdateText() == -1)
      ScrollingMsg.SetText((unsigned char *)myText, sizeof(myText) - 1);
    FastLED.show();
    OldPlasmaTime = PlasmaTime;
    PlasmaTime += PlasmaShift;
    if (OldPlasmaTime > PlasmaTime)
      PlasmaShift = (random8(0, 5) * 32) + 64;
    // break out to respond to Serial1
    if (Serial1.available() >  0)
    {
      FastLED.clear(true);
      return;
    }
  }
  ScrollingMsg.SetText((unsigned char *)myText, sizeof(myText) - 1);
  FastLED.clear(true);
  return;
}

