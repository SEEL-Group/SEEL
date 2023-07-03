#include <SEEL_Scheduler.h>
#include <SEEL_SNode.h>
/* SEEL Parameters */
constexpr uint8_t SEEL_SNODE_ID = 1; // 0 is reserved for gateway nodes, use 0 to randomly generate ID
constexpr uint8_t SEEL_TDMA_SLOT_ASSIGNMENT = 1; // TDMA transmission slot, ignored if not using TDMA sending scheme. See SEEL documentation for advised slot configuration.

/* LoRaPHY Tranceiver Pin Assignments */
constexpr uint8_t SEEL_LoRaPHY_CS_PIN = 10;
constexpr uint8_t SEEL_LoRaPHY_RESET_PIN = 9;
constexpr uint8_t SEEL_LoRaPHY_INT_PIN = 2;
constexpr uint8_t SEEL_RNG_SEED_PIN = 0; // Make sure this pin is NOT connected

SEEL_Scheduler seel_sched; /* SEEL Variables */
//
SEEL_SNode seel_snode;

/* USER Variables */
SEEL_Task user_task;
uint16_t send_count;
bool send_ready;

// LED DEMO
constexpr uint8_t LED_PIN = 13; // Default Arduino Pro Mini LED pin
constexpr uint8_t NUM_LED_TYPES = 10; // Number of LED types allowed, used for modulo operation
constexpr uint32_t FLASH_DURATION_MILLIS = 100;
SEEL_Task LED_task;
uint8_t led_parent_id = SEEL_SNODE_ID; // Should be [0, MAX_LED_TYPES)
bool LED_task_added = false;


void print_task_info(String msg)
{
  uint32_t task_time_to_run;
  uint32_t task_delay;
  uint32_t task_id;
  if (seel_sched.get_task_info(&task_time_to_run, &task_delay, &task_id))
  {
    SEEL_Print::println(msg);
    SEEL_Print::print(F("\tTask Start Time: ")); SEEL_Print::println(task_time_to_run); // Users can use the time_to_run variable to schedule periodic events
    SEEL_Print::print(F("\tTask Delay: ")); SEEL_Print::println(task_delay);
    SEEL_Print::print(F("\tTask ID: ")); SEEL_Print::println(task_id);
  }
}

// Example user function to increment the count variable
void user_function()
{
  ++send_count;
  print_task_info(F("Running user task"));
  send_ready = true;
}

// LED DEMO
void LED_flash()
{
  uint32_t mod_time = millis() % (NUM_LED_TYPES * FLASH_DURATION_MILLIS);

  if (mod_time >= (led_parent_id * FLASH_DURATION_MILLIS) && mod_time < (led_parent_id + 1) * FLASH_DURATION_MILLIS)
  {
    digitalWrite(LED_PIN, HIGH);
    //SEEL_Print::println(F("Write High"));
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
    //SEEL_Print::println(F("Write Low"));
  }

  // Repeat task until sleep
  seel_sched.add_task(&LED_task);
}

// LED DEMO
// This callback function is called after this node receives a broadcast message but before this node can send
// out a bcast message. Thus, any modifications to "bcast_data" will be sent if the broadcast message is sent.
// Write Parameter: "bcast_data" contains misc SEEL info, defined in SEEL_Node.h
// Read-Only Parameter: "new_parent" is true if the sender of this broadcast was set as this node's new parent
void user_callback_broadcast(uint8_t bcast_data[SEEL_MSG_DATA_SIZE], const bool new_parent)
{
  if (new_parent)
  {
    // If this node is the FIRST node to set the LED parent field, then set the field as this node's ID
    // If this node is not the first node, set this node's led_parent_id to be the one in the field
    if (bcast_data[SEEL_MSG_DATA_LED_PARENT_INDEX] == 0)
    {
      led_parent_id = SEEL_SNODE_ID;
      bcast_data[SEEL_MSG_DATA_LED_PARENT_INDEX] = SEEL_SNODE_ID;
      SEEL_Print::print(F("Set LED field to Node ID ")); SEEL_Print::println(SEEL_SNODE_ID);
    }
    else
    {
      led_parent_id = bcast_data[SEEL_MSG_DATA_LED_PARENT_INDEX];
      SEEL_Print::print(F("Set LED Parent to Node ")); SEEL_Print::println(led_parent_id);
    }

    if (!LED_task_added)
    {
      seel_sched.add_task(&LED_task);  
      SEEL_Print::println(F("Added LED task"));
    }
    LED_task_added = true;
  }
}

// This callback function is called when a new message CAN be sent out.
// Write Parameter: "msg_data", which is the data packet to send.
// Read-Only Parameter: "info" contains misc SEEL info, defined in SEEL_Node.h
// Returns: true if the data message should be sent out
bool user_callback_load(uint8_t msg_data[SEEL_MSG_DATA_SIZE], SEEL_Node::SEEL_CB_Info* info)
{
  // The contents of this function are an example of what one can do with this CB function

  if (info->first_callback)
  {
    seel_sched.add_task(&user_task);
    send_ready = false;

    return false;
  }
  // Don't send out the message if the user function has not run yet
  if (!send_ready)
  {
    return false;
  }

  // SEEL_MSG_DATA_SIZE = SEEL_MSG_MISC_SIZE + SEEL_MSG_USER_SIZE
  // If more than SEEL_MSG_MISC_SIZE data bytes are needed, increase SEEL_MSG_USER_SIZE by the missing amount  
  if (SEEL_MSG_DATA_SIZE >= 21) // Safety check to prevent out of bounds access
  {
    msg_data[0] = SEEL_SNODE_ID; // original ID
    msg_data[1] = seel_snode.get_node_id(); // assigned ID
    msg_data[2] = seel_snode.get_parent_id();
    msg_data[3] = info->parent_rssi;
    msg_data[4] = info->bcast_count;
    msg_data[5] = (uint8_t)(info->wtb_millis >> 24); // WTB
    msg_data[6] = (uint8_t)(info->wtb_millis >> 16);
    msg_data[7] = (uint8_t)(info->wtb_millis >> 8);
    msg_data[8] = (uint8_t)(info->wtb_millis);
    msg_data[9] = (uint8_t)(send_count >> 8);
    msg_data[10] = (uint8_t)(send_count);
    msg_data[11] = info->missed_msgs;
    msg_data[12] = info->missed_bcasts;
    msg_data[13] = info->prev_max_data_queue_size;
    msg_data[14] = info->prev_CRC_fails;
    msg_data[15] = info->prev_flags;
    msg_data[16] = info->hop_count; // tracks downstream node hop count
    msg_data[17] = 1; // tracks upstream msg hop count, initialized to 1 and incremented with every forward
    msg_data[18] = info->prev_queue_dropped_msgs_self;
    msg_data[19] = info->prev_queue_dropped_msgs_others;
    msg_data[20] = info->prev_failed_transmissions;

    // Fill rest with zeros
    for (uint32_t i = 21; i < SEEL_MSG_DATA_SIZE; ++i)
    {
        msg_data[i] = 0;
    }
  }

  send_ready = false;

  return true;
}

// This callback function is called right before is DATA message is sent out.
// This is useful to modify the message packet for information that could change between cycles.
// Useful because a message may be generate during one cycle and not sent out until another; some fields (like the SNODE's parent) may change between these cycles.
// Write Parameter: "msg_data", which is the data packet to send
// Read-Only Parameter: "info" contains misc SEEL info, defined in SEEL_Node.h
void user_callback_presend(uint8_t msg_data[SEEL_MSG_DATA_SIZE], const SEEL_Node::SEEL_CB_Info* info)
{
  // The contents of this function are an example of what one can do with this CB function

  /*
  // Sets these fields to the immediate parent of the node and the last rssi value, to be seen by the GNODE for network debugging and analysis
  if (msg_data[2] == 0) // zero means unassigned, prevents continuous updating
  {
    msg_data[2] = seel_snode.get_parent_id();
    msg_data[3] = info->parent_rssi;
  }
  */
}

// This callback function is called when this SNODE receives a DATA message to-be-forwarded.
// Afterwards, the message is added to this SNODE's send_queue.
// Message target and sender are handeled by the SEEL protocol and is not provided to this function, only the data field of the packet is visible here.
// Write Parameter: "msg_data", which is the data packet to forward
// Read-Only Parameter: "info" contains misc SEEL info, defined in SEEL_Node.h
// Return: Whether message should be forwarded
bool user_callback_forwarding(uint8_t msg_data[SEEL_MSG_DATA_SIZE], const SEEL_Node::SEEL_CB_Info* info)
{
  if (SEEL_MSG_DATA_SIZE >= 18)
  {
    msg_data[17] += 1; // tracks upstream msg hop count, initialized to 1 and incremented with every forward
  }
    
  return true;
}

// LED DEMO
// This callback function is called right before the SNode goes to sleep.
void user_callback_sleep()
{
  LED_task_added = false;
  SEEL_Print::println(F("Called Sleep Callback"));
}

void setup()
{
  // Not a great source of entropy: https://www.academia.edu/1161820/Ardrand_The_Arduino_as_a_Hardware_Random-Number_Generator
  randomSeed(random() * analogRead(SEEL_RNG_SEED_PIN)); // Make sure pin 0 is not used

  // Intialize USER variables
  user_task = SEEL_Task(user_function);
  LED_task = SEEL_Task(LED_flash);
  send_count = 0;

  // LED DEMO
  pinMode(LED_PIN, OUTPUT);

  // Initialize SEEL printing
  Serial.begin(9600);
  SEEL_Print::init(&Serial);

  // Initialize Assert NVM
  SEEL_Assert::init_nvm();

  // Initialize sensor node and link response function
  seel_snode.init(&seel_sched, // Scheduler reference
                  user_callback_broadcast, user_callback_load, // User callback functions
                  user_callback_presend, user_callback_forwarding, user_callback_sleep, // User callback functions
                  SEEL_LoRaPHY_CS_PIN, SEEL_LoRaPHY_RESET_PIN, SEEL_LoRaPHY_INT_PIN, // Pins
                  SEEL_SNODE_ID, SEEL_TDMA_SLOT_ASSIGNMENT); // ID and TDMA slot assignments

  // Run main loop inside SEEL_Scheduler
  seel_sched.run();
}

void loop()
{
  // Empty because main loop happens in SEEL_Scheduler, called in setup()
}
