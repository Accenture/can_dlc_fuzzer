#include <SPI.h>
#include "src/autowp-mcp2515/mcp2515.h"
#include <Adafruit_INA219.h>

struct Config {
  bool use_ina;
  float max_current;
  float min_current;
  bool extended_id;
  bool brute_force;
  uint32_t *can_id_array;
  uint32_t can_id_count;
  CAN_SPEED can_speed;
} configuration;

CAN_SPEED can_speed_array[] = {
  CAN_5KBPS,
  CAN_10KBPS,
  CAN_20KBPS,
  CAN_31K25BPS,
  CAN_33KBPS,
  CAN_40KBPS,
  CAN_50KBPS,
  CAN_80KBPS,
  CAN_83K3BPS,
  CAN_95KBPS,
  CAN_100KBPS,
  CAN_125KBPS,
  CAN_200KBPS,
  CAN_250KBPS,
  CAN_500KBPS,
  CAN_1000KBPS
};


Adafruit_INA219 ina219;

struct can_frame can_msg;
MCP2515 mcp2515(10);

String choice;
int invers_current = 1;
float current_ma;

void get_input(String &input) {
  while (Serial.available() == 0) {}
  input = Serial.readString();
  input.trim();
}

void setup() {
  //-----------------------------------------------
  // Set serial
  Serial.begin(9600);
  while (!Serial) { delay(10); }
  Serial.println("[*] Initial Configuration started.");
  //-----------------------------------------------
  //-----------------------------------------------
  // Init INA219
  if (ina219.begin()) {
    // Use INA219
    Serial.println("[?] Do you want to use the INA 219 current sensor to detect potential crashes? [Y/N]");
    get_input(choice);
    if (choice.equals("Y") || choice.equals("y")) {
      configuration.use_ina = true;
    } else {
      configuration.use_ina = false;
    }
  }
  //-----------------------------------------------
  //-----------------------------------------------
  // Initial configuration for the run
  

  // CAN speed
  Serial.println("[?] What CAN bus speed you want to use (Use the number in square brackets from the list below to make a choice)?\n\t [0] 5KBPS\n\t [1] CAN_10KBPS\n\t [2] 20KBPS\n\t [3] 31K25BPS\n\t [4] 33KBPS\n\t [5] 40KBPS\n\t [6] 50KBPS\n\t [7] 80KBPS\n\t [8] 83K3BPS\n\t [9] 95KBPS\n\t [10] 100KBPS\n\t [11] 125KBPS\n\t [12] 200KBPS\n\t [13] 250KBPS\n\t [14] 500KBPS\n\t [15] 1000KBPS");
  get_input(choice);
  configuration.can_speed = can_speed_array[choice.toInt()];
  // Pre-configured IDs or brute-force
  Serial.println("[?] Do you have a set of IDs that should be used (Answering [N] means brute-force attack)? [Y/N]");
  get_input(choice);
  if (choice.equals("Y") || choice.equals("y")) {
    configuration.brute_force = false;
    Serial.println("[?] How many CAN IDs will be used?");
    get_input(choice);
    configuration.can_id_count = choice.toInt();
    configuration.can_id_array = malloc(sizeof(uint32_t) * configuration.can_id_count);
    Serial.println("[*] Enter the CAN ID in hex format seprated with comma (\"0x100,0x101,0x102,...\")");
    get_input(choice);
    int position = 0;
    int next_position = choice.indexOf(",", position);
    String sub;
    for (int i = 0; i < configuration.can_id_count; i++) {
      sub = choice.substring(position, next_position);
      position = next_position + 1;
      next_position = choice.indexOf(",", position);
      configuration.can_id_array[i] = (uint32_t)strtol(sub.c_str(), 0, 16);
    }
    Serial.print("[*] Done reading ");
    Serial.print(configuration.can_id_count);
    Serial.println(" CAN IDs.");
  } else {
    configuration.brute_force = true;
    configuration.can_id_count = 0x7FF;
    Serial.println("[?] Do you want to use [S]tandard or [E]xtended CAN IDs (Note that Extended CAN ID brute-force will take insanely long)?");
    get_input(choice);
    if (choice.equals("E") || choice.equals("e")) {
      configuration.extended_id = true;
      configuration.can_id_count = 0x1FFFFFFF;
    } else {
      configuration.extended_id = false;
      configuration.can_id_count = 0x7FF;
    }
  }

  //-----------------------------------------------
  //-----------------------------------------------
  // Configure CAN
  mcp2515.reset();
  mcp2515.setBitrate(configuration.can_speed, MCP_8MHZ);
  mcp2515.setNormalMode();
  //can_msg.can_id  = 0x123;
  can_msg.can_dlc = 0xf;
  can_msg.data[0] = 0x41;
  can_msg.data[1] = 0x42;
  can_msg.data[2] = 0x43;
  can_msg.data[3] = 0x44;
  can_msg.data[4] = 0x45;
  can_msg.data[5] = 0x46;
  can_msg.data[6] = 0x47;
  can_msg.data[7] = 0x48;
  //mcp2515.sendMessage(&can_msg);
  //-----------------------------------------------
  //-----------------------------------------------
  // INA219 learn min and max currents
  if (configuration.use_ina) {
    Serial.println("[*] INA219 current sensor will be used to detect potential crashes. This requires a brief learning of the current consumption under normal conditions.\n    Please make sure that the device operates as expected and that the current sensor is connected to its power supply.\n    Once you are sure that the device is in its stable state, press ENTER to start the learning process.");
    while(Serial.read() == -1) {}
    // Learning process triggered
    Serial.println("[*] Learning process initiated. Getting current consumption samples ...");
    Serial.println("[*] Press ENTER again to stop the learning process.");
    delay(500);
    current_ma = ina219.getCurrent_mA();
    if (current_ma < 0) {
      invers_current = -1;
    }
    configuration.max_current = current_ma * invers_current;
    configuration.min_current = current_ma * invers_current;
    while (1) {
      current_ma = ina219.getCurrent_mA() * invers_current;
      if (current_ma < configuration.min_current) configuration.min_current = current_ma;
      if (current_ma > configuration.max_current) configuration.max_current = current_ma;
      if (Serial.available() > 0) {
        Serial.read();
        break;
      }
    }
    configuration.max_current = configuration.max_current * 1.15;
    configuration.min_current = configuration.min_current * 0.85;
    Serial.print("[*] Min/max current detected during learning phase: ");Serial.print(configuration.min_current);Serial.print("/");Serial.print(configuration.max_current);Serial.println("mA");
  }
  //-----------------------------------------------
  delay(500);
  Serial.println("[*] Configuration is done. Press the ENTER start fuzzing.");
}

void loop() {
  float percentage = 0;
  int previous_percentage = 0;
  float percentage_split = 0;
  while(Serial.read() == -1) {}
  Serial.println("[*] Fuzzing started ...");
  for (uint32_t message_index = 0; message_index < configuration.can_id_count; message_index++) {
    if (configuration.brute_force) {
      // Pure brute-force
      can_msg.can_id = message_index;
    } else {
      // Set of IDs
      can_msg.can_id = configuration.can_id_array[message_index];
    }
    // Send CAN message
    mcp2515.sendMessage(&can_msg);
    // read INA data here
    if (configuration.use_ina) {
      for (int i = 0; i < 100; i++) {
        current_ma = ina219.getCurrent_mA() * invers_current;
        if (current_ma < configuration.min_current || current_ma > configuration.max_current) {
          Serial.print("[!] Potential crash detected with CAN ID: 0x");
          Serial.println(can_msg.can_id, HEX);
          Serial.print("[!] Anomalous current draw: ");
          Serial.print(current_ma);
          Serial.print("mA (detected min/max current before fuzzing: ");
          Serial.print(configuration.min_current);
          Serial.print("mA/");
          Serial.print(configuration.max_current);
          Serial.println("mA)");
          Serial.println("[*] If you would like to continue fuzzing, set the target into normal operation (for example reset) and press ENTER.");
          while(Serial.read() == -1) {}
          break;
        };
      }
    }
    // Display percentage
    percentage = ((float)message_index / configuration.can_id_count) * 100;
    if ((int)percentage > previous_percentage) {
      previous_percentage = (int)percentage;
      Serial.print("[");
      percentage_split = (int)percentage / 5;
      for (int j = 0; j < percentage_split; j++) Serial.print("#");
      for (int j = 0; j < 20 - percentage_split; j++) Serial.print(" ");
      Serial.print("] ");
      Serial.print(percentage);
      Serial.println("%");
    }
  }
  Serial.println("[*] Fuzzing completed.");
  while (1) {}
}
