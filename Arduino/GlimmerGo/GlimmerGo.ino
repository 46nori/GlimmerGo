//
//  GlimmerGo
//
#include <string.h>
#include <queue>
#include <LiquidCrystal.h>
#include "esp_netif.h"
#include "tcpip_adapter.h"

#define ISR_TIMER_TASK 1
#if ISR_TIMER_TASK == 0
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif

#include "Matter.h"
#include <app/server/OnboardingCodesUtil.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>

using namespace chip;
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;

#define DEBUG 1

//============================================================================
// Matter Device
//============================================================================
constexpr char *TAG = "GlimmerGo";    // log

// Device name
constexpr char *DeviceName      = "GlimmerGo";
constexpr char *OK_ButtonName   = "OK Button";
constexpr char *NG_ButtonName   = "NG Button";
constexpr char *FlashLightName  = "Flash Light";

// Cluster and Attribute ID
//  esp-matter/connectedhomeip/connectedhomeip/zzz_generated/app-common/app-common/zap-generated/ids/Clusters.h
//  esp-matter/connectedhomeip/connectedhomeip/zzz_generated/app-common/app-common/zap-generated/ids/Attributes.h
// ContactSensor
constexpr uint32_t CLUSTER_ID_CONTACT_SENSOR   = BooleanState::Id;
constexpr uint32_t ATTRIBUTE_ID_CONTACT_SENSOR = BooleanState::Attributes::StateValue::Id;

// OnOffLight
constexpr uint32_t CLUSTER_ID_ON_OFF_LIGHT     = OnOff::Id;
constexpr uint32_t ATTRIBUTE_ID_ON_OFF_LIGHT   = OnOff::Attributes::OnOff::Id;

// DimmerableLight
constexpr uint32_t CLUSTER_ID_DIMMABLE_LIGHT   = LevelControl::Id;
constexpr uint32_t ATTRIBUTE_ID_DIMMABLE_LIGHT = LevelControl::Attributes::CurrentLevel::Id;

// Endpoint ID
uint16_t endpoint_id_1;     // OK Button
uint16_t endpoint_id_2;     // NG Button
uint16_t endpoint_id_3;     // Signal Light

// Attribute
attribute_t *attribute_1;
attribute_t *attribute_2;         
attribute_t *attribute_3_level;
attribute_t *attribute_3_onoff;

// Device Events
enum class EventType {
  INIT_COMPLETE,
  START_FLASH,
  STOP_FLASH,
  OK_PRESSED,
  NG_PRESSED,
  TIMER_EXPIRED,
  IP_CHANGED
};
struct Event {
    EventType type;
    int param;

    Event(EventType type, int param) : type(type), param(param) {}
};
std::queue<Event> evq;

//
// Initialize Matter device
//
void InitMatterDevice()
{
  //----------------------------------------------------------------------------
  // Setup Cluster and attributes
  //----------------------------------------------------------------------------
  // contact_sensor
  contact_sensor::config_t config_contact_sensor_1;
  config_contact_sensor_1.boolean_state.state_value = false;  // sensor state is open by default
  config_contact_sensor_1.identify.cluster_revision = 4;
  config_contact_sensor_1.identify.identify_time = 0;
  config_contact_sensor_1.identify.identify_type = 0x02;

  // contact_sensor
  contact_sensor::config_t config_contact_sensor_2;
  config_contact_sensor_2.boolean_state.state_value = false;  // sensor state is open by default
  config_contact_sensor_2.identify.cluster_revision = 4;
  config_contact_sensor_2.identify.identify_time = 0;
  config_contact_sensor_2.identify.identify_type = 0x02;

  // on_off_light
  on_off_light::config_t config_on_off_light;
  config_on_off_light.on_off.on_off = false;
  config_on_off_light.on_off.lighting.start_up_on_off = nullptr;

  // dimmable_light
  dimmable_light::config_t config_dimmable_light;
  config_dimmable_light.level_control.current_level = 1;
  config_dimmable_light.level_control.lighting.start_up_current_level = 1;

  // bridge_node
  bridged_node::config_t config_bridged_node;
  config_bridged_node.bridged_device_basic_information.cluster_revision = 1;
  config_bridged_node.bridged_device_basic_information.reachable = true;

  //----------------------------------------------------------------------------
  // Create Root Node
  //----------------------------------------------------------------------------
  node::config_t node_config;
  snprintf(node_config.root_node.basic_information.node_label,
           sizeof(node_config.root_node.basic_information.node_label),
           DeviceName);
  node_t *node = node::create(&node_config, on_attribute_update, on_identification);

  //----------------------------------------------------------------------------
  // Create Endpoint and bind Cluster
  //----------------------------------------------------------------------------
  // Create the root endpoint
  endpoint_t *endpoint_0 = aggregator::create(node, ENDPOINT_FLAG_NONE, NULL);

  // Crete contact_sensor endpoint for OK Button
  endpoint_t *endpoint_1 = bridged_node::create(node, &config_bridged_node, ENDPOINT_FLAG_NONE, NULL);
  contact_sensor::add(endpoint_1, &config_contact_sensor_1);

  // Crete contact_sensor endpoint for NG Button
  endpoint_t *endpoint_2 = bridged_node::create(node, &config_bridged_node, ENDPOINT_FLAG_NONE, NULL);
  contact_sensor::add(endpoint_2, &config_contact_sensor_2);

  // Crete on_off_light/dimmable_light endpoint for Signal Light
  endpoint_t *endpoint_3 = bridged_node::create(node, &config_bridged_node, ENDPOINT_FLAG_NONE, NULL);
  on_off_light::add(endpoint_3, &config_on_off_light);
  dimmable_light::add(endpoint_3, &config_dimmable_light);

  // Set Name to the endpoint
  cluster::bridged_device_basic_information::attribute::create_node_label(cluster::get(endpoint_1, BridgedDeviceBasicInformation::Id),
                                                                          OK_ButtonName,   strlen(OK_ButtonName));
  cluster::bridged_device_basic_information::attribute::create_node_label(cluster::get(endpoint_2, BridgedDeviceBasicInformation::Id),
                                                                          NG_ButtonName,   strlen(NG_ButtonName));
  cluster::bridged_device_basic_information::attribute::create_node_label(cluster::get(endpoint_3, BridgedDeviceBasicInformation::Id),
                                                                          FlashLightName, strlen(FlashLightName));

  // Get endpoint ID
  endpoint_id_1 = endpoint::get_id(endpoint_1);
  endpoint_id_2 = endpoint::get_id(endpoint_2);
  endpoint_id_3 = endpoint::get_id(endpoint_3);

  //----------------------------------------------------------------------------
  // Bind Endpoints to the root node
  //----------------------------------------------------------------------------
  set_parent_endpoint(endpoint_1, endpoint_0);
  set_parent_endpoint(endpoint_2, endpoint_0);
  set_parent_endpoint(endpoint_3, endpoint_0);

  //----------------------------------------------------------------------------
  // Get Attribute of each Endpoint
  //----------------------------------------------------------------------------
  attribute_1       = attribute::get(cluster::get(endpoint_1, CLUSTER_ID_CONTACT_SENSOR), ATTRIBUTE_ID_CONTACT_SENSOR);
  attribute_2       = attribute::get(cluster::get(endpoint_2, CLUSTER_ID_CONTACT_SENSOR), ATTRIBUTE_ID_CONTACT_SENSOR);
  attribute_3_onoff = attribute::get(cluster::get(endpoint_3, CLUSTER_ID_ON_OFF_LIGHT),   ATTRIBUTE_ID_ON_OFF_LIGHT);
  attribute_3_level = attribute::get(cluster::get(endpoint_3, CLUSTER_ID_DIMMABLE_LIGHT), ATTRIBUTE_ID_DIMMABLE_LIGHT);

  //----------------------------------------------------------------------------
  // Start Matter device
  //----------------------------------------------------------------------------
  // Setup DAC (this is good place to also set custom commission data, passcodes etc.)
  esp_matter::set_custom_dac_provider(chip::Credentials::Examples::GetExampleDACProvider());
  esp_matter::start(on_device_event);
}

esp_matter_attr_val_t get_boolean_attribute_value(esp_matter::attribute_t *att_ref)
{
  esp_matter_attr_val_t boolean_value = esp_matter_invalid(NULL);
  attribute::get_val(att_ref, &boolean_value);
  return boolean_value;
}

//----------------------------------------------------------------------------
// Contact sensor
//----------------------------------------------------------------------------
void SetSwitchOK(bool bState)
{
  esp_matter_attr_val_t value = get_boolean_attribute_value(attribute_1);
  value.val.b = bState;
  attribute::update(endpoint_id_1, CLUSTER_ID_CONTACT_SENSOR, ATTRIBUTE_ID_CONTACT_SENSOR, &value);
}

void SetSwitchNG(bool bState)
{
  esp_matter_attr_val_t value = get_boolean_attribute_value(attribute_2);
  value.val.b = bState;
  attribute::update(endpoint_id_2, CLUSTER_ID_CONTACT_SENSOR, ATTRIBUTE_ID_CONTACT_SENSOR, &value);
}

//----------------------------------------------------------------------------
// Dimmer Light
//----------------------------------------------------------------------------
void SetAttr_FlashLight(bool bState)
{
  esp_matter_attr_val_t value = get_boolean_attribute_value(attribute_3_onoff);
  value.val.b = bState;
  attribute::update(endpoint_id_3, CLUSTER_ID_ON_OFF_LIGHT, ATTRIBUTE_ID_ON_OFF_LIGHT, &value);
}

int debug_val_b = 0;
// Update Flash light and notification message on LCD
#define MATTER_BRIGHTNESS 254
#define STANDARD_BRIGHTNESS 255
esp_err_t app_driver_attribute_update(uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
  static int dimmer_level = 0;
  if (endpoint_id == endpoint_id_3) {
    if (cluster_id == CLUSTER_ID_ON_OFF_LIGHT) {
      if (attribute_id == ATTRIBUTE_ID_ON_OFF_LIGHT) {
        Serial.print("ATTRIBUTE_ID_ON_OFF_LIGHT=0x");
        Serial.println(attribute_id, HEX);
        Serial.print("val->val.b: ");
        Serial.println(val->val.b);
        // Flash light
        if (val->val.b) {
          evq.emplace(EventType::START_FLASH, dimmer_level);
          debug_val_b = 1;  // debug
        } else {
          evq.emplace(EventType::STOP_FLASH, 0);
          debug_val_b = 0;  // debug
        }
      }
    } else if (cluster_id == CLUSTER_ID_DIMMABLE_LIGHT) {
      if (attribute_id == ATTRIBUTE_ID_DIMMABLE_LIGHT) {
        Serial.print("ATTRIBUTE_ID_DIMMABLE_LIGHT=0x");
        Serial.println(attribute_id, HEX);
        Serial.print("val->val.u8: ");
        Serial.println(val->val.u8);
        dimmer_level = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
        if (dimmer_level > 0) {
          evq.emplace(EventType::START_FLASH, dimmer_level);
        }
        debug_val_b = 2;  // debug
      }
    }
  }
  return ESP_OK;
}

//----------------------------------------------------------------------------
// Matter Event Listener
//----------------------------------------------------------------------------
// There is possibility to listen for various device events, related for example
// to setup process. Leaved as empty for simplicity.
static void on_device_event(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        evq.emplace(EventType::IP_CHANGED, 0);
        break;
    default:
        break;
    }  
}

static esp_err_t on_identification(identification::callback_type_t type,
                                   uint16_t endpoint_id,
                                   uint8_t  effect_id,
                                   uint8_t  effect_variant,
                                   void *priv_data)
{
  ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
  return ESP_OK;
}

// Listener on attribute update requests.
// In this example, when update is requested, path (endpoint, cluster and attribute) is checked
static esp_err_t on_attribute_update(attribute::callback_type_t type,
                                     uint16_t endpoint_id,
                                     uint32_t cluster_id,
                                     uint32_t attribute_id,
                                     esp_matter_attr_val_t *val,
                                     void *priv_data)
{
#if DEBUG
  // For debug
  Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Begin");
  if (cluster_id == CLUSTER_ID_DIMMABLE_LIGHT) {
    Serial.println("CLUSTER_ID_DIMMABLE_LIGHT");
  }
  if (cluster_id == CLUSTER_ID_CONTACT_SENSOR) {
    Serial.println("CLUSTER_ID_CONTACT_SENSOR");
  }
  if (type == attribute::PRE_UPDATE) {
    Serial.println("attribute::PRE_UPDATE");
  } else {
    Serial.print("attribute::0x");
  Serial.println(type, HEX);
  }
#endif // DEBUG

  esp_err_t err = ESP_OK;

  if (type == PRE_UPDATE) {
    err = app_driver_attribute_update(endpoint_id, cluster_id, attribute_id, val);
  }

#if DEBUG
  Serial.println("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< End");
#endif // DEBUG

  return err;
}

//============================================================================
// LCD
//============================================================================
// Initialize
// PIN settings
//                RS   E  D4  D5  D6  D7
LiquidCrystal lcd(26, 25, 12, 13, 14, 33);
void InitLCD() {
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
}

//
// Show Notification message
//
void LcdPrint_Message(int value)
{
#if 0
  #define PRINT_LCD_MESSAGE(idx) lcd.print(message_str[idx])
  static constexpr char * const message_str[] = {
    "Wash dishes  ",
    "Dump trash   ",
    "Take laundry ",
    "Come here    ",
    "Dinner       "
  };
#else
  #define PRINT_LCD_MESSAGE(idx) lcd.print(&message_str[idx][0])
  static constexpr char message_str[][8] = {
    {0xbb, 0xd7, 0x20, 0xb1, 0xd7, 0xb4, 0x20, 0},  // ｻﾗ ｱﾗｴ
    {0xba, 0xde, 0xd0, 0x20, 0xbd, 0xc3, 0xdb, 0},  // ｺﾞﾐ ｽﾃﾛ
    {0xbe, 0xdd, 0xc0, 0xb8, 0xd3, 0xc9, 0x20, 0},  // ｾﾝﾀｸﾓﾉ
    {0xc1, 0xae, 0xaf, 0xc4, 0xba, 0xb2, 0x20, 0},  // ﾁｮｯﾄｺｲ
    {0xba, 0xde, 0xca, 0xdd, 0xc0, 0xde, 0xd6, 0}   // ｺﾞﾊﾝﾀﾞﾖ
  };
#endif

  lcd.clear();
  if (value < 51) {
    PRINT_LCD_MESSAGE(0);
  } else if (value < 102) {
    PRINT_LCD_MESSAGE(1);
  } else if (value < 153) {
    PRINT_LCD_MESSAGE(2);
  } else if (value < 204) {
    PRINT_LCD_MESSAGE(3);
  } else {
    PRINT_LCD_MESSAGE(4);
  }

  // debug
  char val_str[4];
  snprintf(val_str, sizeof(val_str), "%03d", value);
  lcd.setCursor(13,0);
  lcd.print(val_str);
}

//
//  Show pairing code
//
void LcdPrint_PairingCode()
{
  char payloadBuffer[32];
  // Print codes needed to setup Matter device
  PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

  chip::MutableCharSpan manualPairingCode(payloadBuffer);
  if (GetManualPairingCode(manualPairingCode, chip::RendezvousInformationFlag::kBLE) == CHIP_NO_ERROR) {
    Serial.println(manualPairingCode.data());
    lcd.clear();
    lcd.print("PC:");
    lcd.print(manualPairingCode.data());
    ChipLogProgress(AppServer, "Manual pairing code: [%s]", manualPairingCode.data());
  } else {
    Serial.println("Pairing Code Error!!");
    ChipLogError(AppServer, "Getting manual pairing code failed!");
  }
}

//
//  Show IP address
//
void LcdPrint_IPaddress()
{
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
  lcd.setCursor(0,1);
  lcd.print(ip4addr_ntoa((const ip4_addr_t *)&ip_info.ip));
}

//============================================================================
// GPIO
//============================================================================
// PIN assignment
#define LCDBK_LIGHT   (32)
#define FLASH_LIGHT   (4)
#define OK_BUTTON     (22)
#define OK_BUTTON_LED (23)
#define NG_BUTTON     (18)
#define NG_BUTTON_LED (19)

void InitGPIO()
{
  pinMode(OK_BUTTON, INPUT_PULLUP);
  pinMode(NG_BUTTON, INPUT_PULLUP);
  pinMode(LCDBK_LIGHT, OUTPUT);
  pinMode(FLASH_LIGHT, OUTPUT);
  pinMode(OK_BUTTON_LED, OUTPUT);
  pinMode(NG_BUTTON_LED, OUTPUT);
  TurnOff_LED();
  TurnOff_FlashLight();
}

void TurnOn_FlashLight()
{
  digitalWrite(FLASH_LIGHT, HIGH);
}

void TurnOff_FlashLight()
{
  digitalWrite(FLASH_LIGHT, LOW);
}

void TurnOn_LED()
{
  digitalWrite(LCDBK_LIGHT, HIGH);
  digitalWrite(OK_BUTTON_LED, LOW);
  digitalWrite(NG_BUTTON_LED, LOW);
}

void TurnOff_LED()
{
  digitalWrite(LCDBK_LIGHT, LOW);
  digitalWrite(OK_BUTTON_LED, HIGH);
  digitalWrite(NG_BUTTON_LED, HIGH);
}

bool bOKEnable = false;
bool bNGEnable = false;
void SetTwinkleLED(bool bOKState, bool bNGState)
{
  bOKEnable = bOKState;
  bNGEnable = bNGState;
}

void TwinkleLED_ISR(long counter)
{
  if (bOKEnable) {
    digitalWrite(OK_BUTTON_LED,  (counter % 1000 < 500) ? LOW: HIGH);
  }
  if (bNGEnable) {
    digitalWrite(NG_BUTTON_LED,  (counter % 1000 < 500) ? LOW: HIGH);
  }
}

bool IsButtonPressed(int button)
{
  return digitalRead(button) == LOW;
}

//============================================================================
// Timer
//============================================================================
volatile unsigned long interruptCounter = 0L;
volatile unsigned long response_timer = 0L;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#if ISR_TIMER_TASK
hw_timer_t * timer = NULL;
void IRAM_ATTR onTimer()
{
	portENTER_CRITICAL_ISR(&timerMux);
	interruptCounter++;

  // Check response_timer
  if (response_timer > 0) {
    if (response_timer == 1) {
      evq.emplace(EventType::TIMER_EXPIRED, 0);
    }
    response_timer -= 1;
  }
	portEXIT_CRITICAL_ISR(&timerMux);

  TwinkleLED_ISR(interruptCounter);
}

#define TICK (1000L)
void InitTimer()
{
	timer = timerBegin(0, 80, true);        // prescaler 80MHz / 80
	timerAttachInterrupt(timer, &onTimer, true);
	timerAlarmWrite(timer, TICK, true);     // 1ms
	timerAlarmEnable(timer);
}
#else  // not ISR_TIMER_TASK
void TimerTask(void *pvParameter)
{
  while (1)
  {
    interruptCounter++;

    // Check response_timer
    if (response_timer > 0) {
      if (response_timer == 1) {
        evq.emplace(EventType::TIMER_EXPIRED, 0);
      }
      response_timer -= 1;
    }

    TwinkleLED_ISR(interruptCounter);
    vTaskDelay(pdMS_TO_TICKS(1000));      // wait 1s
  }
}
#endif

void StartTimer(unsigned long n)
{
	portENTER_CRITICAL(&timerMux);
  response_timer = n;                     // n ms
	portEXIT_CRITICAL(&timerMux);
}

void CancelTimer()
{
	portENTER_CRITICAL(&timerMux);
  response_timer = 0L;
	portEXIT_CRITICAL(&timerMux);
}

//============================================================================
// Device State Controller
//============================================================================
class Initializing;
class Idling;
class Flashing;
class RespondingOK;
class RespondingNG;

//
// State base class
//
class State {
public:
  virtual ~State() {}
  virtual void entry() {}
  virtual void exit() {}
  virtual void handle(const Event& event) = 0;
  virtual State* nextState(const Event& event) = 0;
  int get_state() { return m_state; }
protected:
  int m_state;
};

//
// Concrete States
//
class Initializing : public State {
public:
  void entry() {
    m_state = 0;
    SetTwinkleLED(true, true);
    InitMatterDevice();
    SetAttr_FlashLight(false);
    delay(3000);      // wait 3 sec to be stable
    SetTwinkleLED(false, false);
  }
  void handle(const Event& event) override {
  }
  State* nextState(const Event& event) override;
};

class Idling : public State {
public:
  void entry() {
    m_state = 1;
    SetTwinkleLED(false, false);
    TurnOff_LED();
    LcdPrint_PairingCode();
    LcdPrint_IPaddress();
  }
  void handle(const Event& event) override {
  }
  State* nextState(const Event& event) override;
};

class Flashing : public State {
public:
  void entry() {
    m_state = 2;
    TurnOn_FlashLight();
    TurnOn_LED();
  }
  void exit() {
    TurnOff_FlashLight();
    TurnOff_LED();
  }
  void handle(const Event& event) override {
  }
  State* nextState(const Event& event) override;
};

class RespondingOK : public State {
public:
  void entry() {
    m_state = 3;
  }
  void handle(const Event& event) override {
  }
  State* nextState(const Event& event) override;
};

class RespondingNG : public State {
public:
  void entry() {
    m_state = 4;
  }
  void handle(const Event& event) override {
  }
  State* nextState(const Event& event) override;
};

//
// State instances
//
Initializing  initializingState;
Idling        idlingState;
Flashing      flashingState;
RespondingOK  respondingNG_State;
RespondingNG  respondingOK_State;

//
// Transitions
//
State* Initializing::nextState(const Event& event) {
  if (event.type == EventType::INIT_COMPLETE) {
    SetSwitchOK(false);
    SetSwitchNG(false);
    return &idlingState;
  }
  return this;
}

State* Idling::nextState(const Event& event) {
  if (event.type == EventType::START_FLASH) {
    LcdPrint_Message(event.param);
    return &flashingState;
  }
  if (event.type == EventType::IP_CHANGED) {
    return this;
  }
  return this;
}

State* Flashing::nextState(const Event& event) {
  if (event.type == EventType::OK_PRESSED) {
    SetSwitchOK(true);
    SetAttr_FlashLight(false);
    SetTwinkleLED(true, false);
    StartTimer(10000);    // 10s
    return &respondingOK_State;
  }
  if (event.type == EventType::NG_PRESSED) {
    SetSwitchNG(true);
    SetAttr_FlashLight(false);
    SetTwinkleLED(false, true);
    StartTimer(10000);    // 10s
    return &respondingNG_State;
  }
  if (event.type == EventType::STOP_FLASH) {
    return &idlingState;
  }
  if (event.type == EventType::START_FLASH) {
    LcdPrint_Message(event.param);
    return this;
  }
  return this;
}

State* RespondingOK::nextState(const Event& event) {
  if (event.type == EventType::START_FLASH) {
    LcdPrint_Message(event.param);
    SetSwitchOK(false);
    CancelTimer();
    return &flashingState;
  }
  if (event.type == EventType::TIMER_EXPIRED) {
    SetSwitchOK(false);
    return &idlingState;
  }
  return this;
}

State* RespondingNG::nextState(const Event& event) {
  if (event.type == EventType::START_FLASH) {
    LcdPrint_Message(event.param);
    SetSwitchNG(false);
    CancelTimer();
    return &flashingState;
  }
  if (event.type == EventType::TIMER_EXPIRED) {
    SetSwitchNG(false);
    return &idlingState;
  }
  return this;
}

//
// State machine
//
class StateMachine {
private:
  State* currentState;
  int m_state;

public:
  StateMachine(State* initialState) : currentState(initialState) {
    currentState->entry();
  }
  void handleEvent(const Event& event) {
      currentState->handle(event);
      State* nextState = currentState->nextState(event);
      if (nextState != currentState) {
        currentState->exit();
        currentState = nextState;
        currentState->entry();
      }
      m_state = currentState->get_state();
  }
  int get_state() {
    return m_state;
  }
};

StateMachine *sm;

//============================================================================
//  Initialize
//============================================================================
// Task handle
TaskHandle_t xTimerTaskHandle = NULL;

void setup()
{
  Serial.begin(115200);     // Init serial for debug
  InitGPIO();               // Init GPIO
  InitLCD();                // Init LCD
#if ISR_TIMER_TASK
  InitTimer();              // Init timer
#else
  // Create Timer task
  BaseType_t xReturned = xTaskCreate(
      TimerTask,
      "TimerTask",
      512,              // Stack size
      NULL,             // parameter
      tskIDLE_PRIORITY, // priority
      &xTimerTaskHandle
  );
  if (xReturned != pdPASS) {
  }
#endif
  // Enable debug logging
  esp_log_level_set("*", ESP_LOG_INFO);

  // Start state machine
  sm = new StateMachine(&initializingState);
  evq.emplace(EventType::INIT_COMPLETE, 0);
 }

//============================================================================
//  Event loop
//============================================================================
void loop()
{
  if (!evq.empty()) {
    sm->handleEvent(evq.front());
    evq.pop();
  }
  lcd.setCursor(14, 1);
  lcd.print(debug_val_b, HEX);        // debug: show flash light status
  lcd.setCursor(15, 1);
  lcd.print(sm->get_state(), HEX);    // debug: show current state

  // Check button event
  if (IsButtonPressed(OK_BUTTON)) {
    evq.emplace(EventType::OK_PRESSED, OK_BUTTON);
  } else if (IsButtonPressed(NG_BUTTON)) {
    evq.emplace(EventType::NG_PRESSED, NG_BUTTON);
  }
}
