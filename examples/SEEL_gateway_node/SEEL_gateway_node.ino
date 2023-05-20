#include <SEEL_Scheduler.h>
#include <SEEL_GNode.h>

#include <FileIO.h>

/* SEEL Parameters */
constexpr uint8_t SEEL_TDMA_SLOT_ASSIGNMENT = 0; // TDMA transmission slot, ignored if not using TDMA sending scheme. See SEEL documentation for advised slot configuration.

// Dictates how long SNODEs will be awake/asleep for. Sleep time is calculated by cycle time - awake time, both are converted to millis during calculation
// Make sure awake/sleep times can be converted to millis without overflow
 // Units are in seconds to reduce field sizes in msg packet
constexpr uint32_t SEEL_CYCLE_PERIOD_SECS = 240;
constexpr uint32_t SEEL_SNODE_AWAKE_TIME_SECS = 180;

/* LoRaPHY Tranceiver Pin Assignments */
constexpr uint8_t SEEL_LoRaPHY_CS_PIN = 10; // Don't change these if using Dragino LG01
constexpr uint8_t SEEL_LoRaPHY_RESET_PIN = 9; // Don't change these if using Dragino LG01
constexpr uint8_t SEEL_LoRaPHY_INT_PIN = 2; // Don't change these if using Dragino LG01
constexpr uint8_t SEEL_RNG_SEED_PIN = 0; // Make sure this pin is NOT connected

/* File Write Paramters */
const char* LOG_FILE_PATH = ""; // Path on gateway node file system

/* SEEL Variables */
SEEL_Scheduler seel_scheduler;
SEEL_GNode seel_gnode;

// Example function on how to write strings to file (must have capable hardware setup)
void write_to_file(const String& to_write)
{
  // Append setting adds on, instead of clearing all previous lines in log
  File f = FileSystem.open(LOG_FILE_PATH, FILE_APPEND);

  if (f) // If the file opened successfully
  {
    // Writes the message to file
    f.println(to_write);

    // Close file after use because file is opened in this function's scope
    // Also helps with interruptions (power outage)
    f.close();

    SEEL_Print::print(F("LOG: ")); // Logged msg
  }
  else
  {
    SEEL_Print::print(F("FOE: ")); // File Open Error
  }
  
  // Also write the message to console for convenience
  SEEL_Print::println(to_write);
}

// This callback function is called right after the GNODE sends out its broadcast message
// Read/Write Parameter: "msg_data" which is the data packet of the broadcast message
void user_callback_broadcast(uint8_t msg_data[SEEL_MSG_DATA_SIZE], uint16_t prev_any_trans, SEEL_Node::SEEL_CB_Info* info)
{
  // The contents of this function are an example of what one can do with this CB function
  
  String msg_string = "";
  msg_string = F("BT: "); // Broadcast Time
  msg_string += String(millis(), DEC) + F("\n");
  msg_string += F("PT: "); // Previous Transmissions (any type) sent
  msg_string += String(prev_any_trans, DEC);
  write_to_file(msg_string);
  
  msg_string = "";
  msg_string += F("BD: "); // Broadcast Data
  for (uint32_t i = 0; i < SEEL_MSG_DATA_SIZE; ++i)
  {
    String msg_data_str = String(msg_data[i], DEC) + F(" ");
    msg_string += msg_data_str;
  }

  write_to_file(msg_string);
}

// This callback function is called when the GNODE receives a data message
// Read-Only Parameter: "msg_data" which is the received data packet
// Read-Only Parameter: "msg_rssi" contains the RSSI of the received message
void user_callback_data(const uint8_t msg_data[SEEL_MSG_DATA_SIZE], const int8_t msg_rssi)
{
  // The contents of this function are an example of what one can do with this CB function
  
  String msg_string = "";
  
  for (uint32_t i = 0; i < SEEL_MSG_DATA_SIZE; ++i)
  {
    String msg_data_str = String(msg_data[i], DEC) + F(" ");
    String index_str = "";
    uint8_t field_value = msg_data[i];
    // Record GNODE reception RSSI if GNODE is first parent
    if (i == 3 && msg_data[2] == SEEL_GNODE_ID) // Corresponds to message locations hard-coded in the SNODE .ino file
    {
      field_value = msg_rssi;
    }
    msg_string += msg_data_str;
    index_str += F("[");
    index_str += String(i, DEC);
    index_str += F("]");
    SEEL_Print::print(index_str + msg_data_str);
  }
  SEEL_Print::println("");

  write_to_file(msg_string);
}

void setup()
{
  // Not a great source of entropy: https://www.academia.edu/1161820/Ardrand_The_Arduino_as_a_Hardware_Random-Number_Generator
  randomSeed(random() * analogRead(SEEL_RNG_SEED_PIN)); // Make sure pin 0 is not used
  
  // Enables file logging
  FileSystem.begin();

  /*
  // Initialize Serial (Serial based comms)
  Serial.begin(9600);
  SEEL_Print::init(&Serial);
  */
  ///*
  // Initialize Console (Console bridge based comms, Dragino)
  Bridge.begin(115200);
  Console.begin();
  SEEL_Print::init(&Console);
  //*/

  // Initialize Assert NVM
  SEEL_Assert::init_nvm();

  SEEL_Assert::assert(SEEL_CYCLE_PERIOD_SECS >= SEEL_SNODE_AWAKE_TIME_SECS, SEEL_ASSERT_FILE_NUM_USER0, __LINE__, false);
  
  // Initialize gateway node and link logging function
  seel_gnode.init(&seel_scheduler,  // Scheduler reference
    user_callback_broadcast, user_callback_data, // Callback functions
    SEEL_LoRaPHY_CS_PIN, SEEL_LoRaPHY_RESET_PIN, SEEL_LoRaPHY_INT_PIN, // Pins
    SEEL_CYCLE_PERIOD_SECS, SEEL_SNODE_AWAKE_TIME_SECS, // Cycle info
    SEEL_TDMA_SLOT_ASSIGNMENT); // TDMA slot assignment

  // Run main loop inside SEEL_Scheduler
  seel_scheduler.run();
}

void loop()
{
  // Empty because main loop happens in SEEL_Scheduler, called in setup()
}
