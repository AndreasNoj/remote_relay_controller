// Remote Control SensESP Application using SKPutRequest
//
// This application controls a remote relay by sending a PUT request to a
// configurable SignalK path. A pushbutton (DigitalInputChange) toggles the
// local state, and the new state is sent using SKPutRequest::set(). An
// SKValueListener listens on the same path so that a status LED shows the
// current state as reported by the SignalK server.

#include <Wire.h>

#include <memory>

#include "sensesp.h"
#include "sensesp/sensors/digital_input.h"  // DigitalInputChange
#include "sensesp/sensors/digital_output.h"
#include "sensesp/signalk/signalk_put_request.h"  // Use SKPutRequest instead of SKOutput
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/ui/config_item.h"
#include "sensesp_app_builder.h"

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

  const int num_relays = 4;

  // Define arrays for button and LED pins.
  int buttonPins[num_relays] = {16, 17, 18, 19};
  int statusLEDPins[num_relays] = {12, 13, 14, 15};

  // Default SignalK paths for each relay channel.
  const char* default_sk_paths[num_relays] = {
      "electrical.switches.light.cabin.state",
      "electrical.switches.light.port.state",
      "electrical.switches.light.starboard.state",
      "electrical.switches.light.engine.state"};

  for (int i = 0; i < num_relays; i++) {
    int relayIndex = i;  // Capture index for lambda

    // Create a pushbutton input using DigitalInputChange.
    // The button is assumed active LOW (with INPUT_PULLUP).
    auto* button =
        new DigitalInputChange(buttonPins[relayIndex], INPUT_PULLUP, CHANGE);

    // Add a debounce transform (50 ms period).
    auto* debouncer = new Debounce<bool>(50);
    button->connect_to(debouncer);

    // Create an SKPutRequest to send a PUT command.
    std::string configPath =
        "/Remote/Control/Relay" + std::to_string(relayIndex + 1) + "/Value";
    const char* sk_path = default_sk_paths[relayIndex];
    std::string sk_meta_desc = "Remote control relay state for relay " +
                std::to_string(relayIndex + 1);
    std::string relay_title = "Relay " + std::to_string(relayIndex + 1) + " Path";
    auto* sk_put_request =
        new SKPutRequest<bool>(sk_path, configPath.c_str());
    // Wrap the SKPutRequest in a ConfigItem so its SignalK path is
    // configurable.
    ConfigItem(sk_put_request)
        ->set_title(relay_title.c_str())
        ->set_sort_order(100 + relayIndex);

    // Create an SKValueListener to listen for state updates.
    // (Second parameter is a sort order.)
    auto* sk_value_listener =
        new SKValueListener<bool>(sk_path, 200 + relayIndex);

    // Create a DigitalOutput for a status LED.
    auto* status_led = new DigitalOutput(statusLEDPins[relayIndex]);
    sk_value_listener->connect_to(
        new LambdaConsumer<bool>([status_led, relayIndex](bool state) {
          status_led->set(state);
          debugD("Remote Control: Received state for relay %d: %d",
                 relayIndex + 1, state);
        }));

    // Local variable to store current command state for this channel.
    bool* current_state = new bool(false);

    // When the debounced button is pressed (LOW), toggle state and send a PUT
    // command.
    debouncer->connect_to(new LambdaConsumer<bool>(
        [sk_put_request, current_state, relayIndex](bool state) {
          // LOW (false) indicates a button press with INPUT_PULLUP.
          if (!state) {
            *current_state = !(*current_state);
            sk_put_request->set(*current_state);
            debugD("Remote Control: Button for relay %d pressed, new state: %d",
                   relayIndex + 1, *current_state);
          }
        }));
  }

  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }