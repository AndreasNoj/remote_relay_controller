// Remote Control SensESP Application using SKPutRequest
//
// This application controls a remote relay by sending a PUT request to a 
// configurable SignalK path. A pushbutton (DigitalInputChange) toggles the local state,
// and the new state is sent using SKPutRequest::set(). An SKValueListener listens on
// the same path so that a status LED shows the current state as reported by the SignalK server.

#include <memory>
#include "sensesp.h"
#include "sensesp_app_builder.h"
#include "sensesp/sensors/digital_input.h"    // DigitalInputChange
#include "sensesp/sensors/digital_output.h"
#include "sensesp/signalk/signalk_put_request.h" // Use SKPutRequest instead of SKOutput
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
                    ->set_wifi_client("Obelix", "obelix2idefix")
                    ->get_app();

  // GPIO configuration:
  // Button pin to send the command.
  const int buttonPin = 16;
  // LED pin for remote status feedback.
  const int statusLEDPin = 12;

  // Create a DigitalInputChange for the pushbutton (active LOW using INPUT_PULLUP).
  auto* button = new DigitalInputChange(buttonPin, INPUT_PULLUP, CHANGE);

  // Shared SignalK path (this must exactly match the path used in the relay application).
  const char* sk_path = "electrical.switches.light.cabin.state";

  // Create an SKPutRequest to send a PUT command on the shared SignalK path.
  // (The second parameter is a configuration path so the SK path is configurable.)
  auto remote_control_metadata = std::make_shared<SKMetadata>("", "Remote control relay state");
  auto* sk_put_request = new SKPutRequest<bool>(sk_path, "/Remote/Control/Relay1/Value");
  ConfigItem(sk_put_request)
       ->set_title("Remote SK PUT Path")
       ->set_sort_order(100);

  // Create an SKValueListener so the remote controller can display the current state.
  // (Here the second parameter is a sort order.)
  auto* sk_value_listener = new SKValueListener<bool>(sk_path, 200);

  // Create a DigitalOutput for a status LED.
  auto* status_led = new DigitalOutput(statusLEDPin);
  sk_value_listener->connect_to(new LambdaConsumer<bool>([status_led](bool state) {
    status_led->set(state);
    debugD("Remote Control: Received state: %d", state);
  }));

  // Use a local variable to hold the current command state.
  bool* current_state = new bool(false);

  // When the button is pressed (detected by DigitalInputChange as a falling edge),
  // toggle the current state and send it via SKPutRequest.
  button->connect_to(new LambdaConsumer<bool>([sk_put_request, current_state](bool state) {
    // For a button using INPUT_PULLUP, LOW (false) indicates a press.
    if (!state) {
      *current_state = !(*current_state);
      sk_put_request->set(*current_state);  // This issues a PUT request.
      debugD("Remote Control: Button pressed, sending new state: %d", *current_state);
    }
  }));

  while (true) {
    loop();
  }
}

void loop() {
  event_loop()->tick();
}
