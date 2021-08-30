#include <SEEL_Scheduler.h>
#include <SEEL_SNode.h>

/* SEEL Parameters */
const uint8_t SEEL_SNODE_ID = 1; // 0 is reserved for gateway nodes, use 0 to randomly generate ID
const uint8_t SEEL_TDMA_SLOT_ASSIGNMENT = 1; // TDMA transmission slot, ignored if not using TDMA sending scheme. See SEEL documentation for advised slot configuration.

/* Pin Assignments */
const uint8_t SEEL_RFM95_CS_PIN = 10;
const uint8_t SEEL_RFM95_INT_PIN = 2;
const uint8_t SEEL_RNG_SEED_PIN = 0; // Make sure this pin is NOT connected

/* SEEL Variables */
SEEL_Scheduler seel_sched;
SEEL_SNode seel_snode;

/* USER Variables */
SEEL_Task user_task;
uint16_t send_count;
bool send_ready;

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

// This callback function is called when a new message CAN be sent out
// Write Parameter: "msg_data", which is the data packet to send
// Read-Only Parameter: "info" contains misc SEEL info, defined in SEEL_Node.h
// Read-Only Parameter: "send_queue_full" denotes whether send_queue for this device is full
// Returns: true if the data message should be sent out
bool user_callback_load(uint8_t msg_data[SEEL_MSG_DATA_SIZE], const SEEL_Node::SEEL_CB_Info* info)
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

  // Pseudo check for if we're using the right message size to prevent out of bounds access
  if (SEEL_MSG_MISC_SIZE >= 13)
  {
    msg_data[0] = SEEL_SNODE_ID; // original ID
    msg_data[1] = seel_snode.get_node_id(); // assigned ID
    msg_data[2] = SEEL_GNODE_ID; // parent ID. Set to 0 (since if GNODE is parent, no modification necessary) for now, modify in fwd function by first parent node
    msg_data[3] = 0; // parent RSSI. Also set in fwd function by parent node
    msg_data[4] = (uint8_t)(info->wtb_millis >> 24); // WTB
    msg_data[5] = (uint8_t)(info->wtb_millis >> 16);
    msg_data[6] = (uint8_t)(info->wtb_millis >> 8);
    msg_data[7] = (uint8_t)(info->wtb_millis);
    msg_data[8] = info->prev_data_transmissions;
    msg_data[9] = info->missed_bcasts;
    msg_data[10] = info->data_queue_size;
    msg_data[11] = (uint8_t)(send_count >> 8);
    msg_data[12] = (uint8_t)(send_count);

    // Fill rest with zeros
    for (uint32_t i = 13; i < SEEL_MSG_DATA_SIZE; ++i)
    {
        msg_data[i] = 0;
    }
  }

  send_ready = false;

  return true;
}


// This callback function is called when this SNODE receives a message to-be-forwarded (Either data or id_check msg)
// Afterwards, the message is added to this SNODE's send_queue
// Message target and sender are handeled by the SEEL protocol and is not provided to this function, only the data field of the packet is visible here
// Read/Write Parameter: "msg_data" which is the data packet of the forwarded message
// Read-Only Parameter: "msg_rssi" contains the RSSI of the received message to be forwarded
void user_callback_forwarding(uint8_t msg_data[SEEL_MSG_DATA_SIZE], const int8_t msg_rssi)
{
  // The contents of this function are an example of what one can do with this CB function
  
  if (msg_data[2] == SEEL_GNODE_ID) // Only set for FIRST parent (If first parent is GNODE, already set)
  {
    // Sets the immediate forwarder SNODE's ID and RSSI, to be seen by the GNODE for network debugging and analysis
    msg_data[2] = seel_snode.get_node_id();
    msg_data[3] = msg_rssi;
  }
}

void setup()
{
  // Not a great source of entropy: https://www.academia.edu/1161820/Ardrand_The_Arduino_as_a_Hardware_Random-Number_Generator
  randomSeed(random() * analogRead(SEEL_RNG_SEED_PIN)); // Make sure pin 0 is not used
  
  // Intialize USER variables
  user_task = SEEL_Task(user_function);
  send_count = 0;

  // Initialize SEEL printing
  Serial.begin(9600);
  SEEL_Print::init(&Serial);

  // Initialize sensor node and link response function
  seel_snode.init(&seel_sched, // Scheduler reference
  user_callback_load, user_callback_forwarding, // User callback functions
  SEEL_RFM95_CS_PIN, SEEL_RFM95_INT_PIN, // Pins
  SEEL_SNODE_ID, SEEL_TDMA_SLOT_ASSIGNMENT); // ID and TDMA slot assignments

  // Run main loop inside SEEL_Scheduler
  seel_sched.run();
}

void loop()
{
  // Empty because main loop happens in SEEL_Scheduler, called in setup()
}
