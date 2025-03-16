// Remote Control SensESP Application using DigitalInputChange and configurable SK path
//
// This application controls a remote relay application by publishing commands
// to a configurable SignalK path. A pushbutton toggles the remote relay state, and an
// LED displays the current remote relay state as reported by the SignalK server.
// The SK path used for control is configurable via the SensESP configuration interface.

#include <memory>
#include "sensesp.h"
#include "sensesp_app_builder.h"
#include "sensesp/sensors/digital_input.h"  // Provides DigitalInputChange
#include "sensesp/sensors/digital_output.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/ui/config_item.h"

#include <Wire.h>

#define I2C_SDA 21  
#define I2C_SCL 22  

using namespace sensesp;
using namespace reactesp;

void setup() {
  SetupLogging(ESP_LOG_DEBUG);
  Wire.begin(I2C_SDA, I2C_SCL);

  // Build the SensESP application.
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    ->set_hostname("Remote-Relay-Control")
                    ->set_wifi_client("YourSSID", "YourPassword")
                    ->get_app();

  // GPIO configuration:
  // Define the pushbutton pin used to send a command.
  const int buttonPin = 18;
  // Define the LED pin for remote status feedback.
  const int statusLEDPin = 12;

  // Create a DigitalInputChange for the pushbutton.
  // The button is configured with INPUT_PULLUP and triggers on CHANGE.
  auto* button = new DigitalInputChange(buttonPin, INPUT_PULLUP, CHANGE);

  // Create an SKOutput for publishing the new state.
  // The first parameter is the SignalK path; here it is configurable.
  // The second parameter is the configuration path for SKOutput.
  auto remote_control_metadata = std::make_shared<SKMetadata>("", "Remote control relay state");
  auto* sk_output = new SKOutput<bool>("electrical.switches.light.cabin.state",
                                       "/Remote/Control/Value",
                                       remote_control_metadata);

  // Configure the SKOutput so that the SignalK path can be modified via the web UI.
  ConfigItem(sk_output)
      ->set_title("Remote SK Output Path")
      ->set_sort_order(100);

  // Create an SKValueListener to listen for state updates from the relay application.
  // Note: This listener is still created with the default SK path.
  auto* sk_value_listener = new SKValueListener<bool>("electrical.switches.light.cabin.state", 100);

  // Create a DigitalOutput for a status LED.
  auto* status_led = new DigitalOutput(statusLEDPin);

  // Update the status LED whenever a new state is received from the SignalK server.
  sk_value_listener->connect_to(new LambdaConsumer<bool>([status_led](bool state) {
    status_led->set(state);
    debugD("Remote Control: Received remote relay state: %d", state);
  }));

  // Use a local variable to store the current command state.
  bool* current_state = new bool(false);

  // When the button is pressed (detected via a falling edge), toggle the state and publish it.
  button->connect_to(new LambdaConsumer<bool>([sk_output, current_state](bool state) {
    // For DigitalInputChange with INPUT_PULLUP, LOW (false) indicates the button is pressed.
    if (!state) {
      *current_state = !(*current_state);
      sk_output->set(*current_state);  // Use set() in SensESP v3.1
      debugD("Remote Control: Button pressed, sending new state: %d", *current_state);
    }
  }));

  // Keep the application running.
  while (true) {
    loop();
  }
}

void loop() {
  event_loop()->tick();
}
