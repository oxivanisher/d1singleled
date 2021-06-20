# d1singleled
Use a Wemos D1 mini to control a single LED. At the moment, no effects or such are implemented.

## Wiring the D1
Per default (see the `config.h` file), the following pins are used:

| Pin | Connected to      | Comment                                            |
|-----|-------------------|----------------------------------------------------|
| D8  | LED output        | Digital output to control the LED. Output is 3.3V. |

Remember the you also have to supply power to your D1 Mini.

## Configuration
See the `config.h.example` file which has to be copied to `config.h` to be used.

## MQTT interface

### Discovery
It publishes its MAC address regularly to `/d1singleled/discovery/MAC` with the
current version as value.

### Last will
It sets its last will to `/d1singleled/lastwill/MAC` with the MAC as message. This
message will be retained and cleared on start.

### Control the LED
It subscribes to two MQTT topics where it listens to the payload `on` and `off` to enable and disable the LED:
* `/d1singleled/all`
* `/d1singleled/MAC address`
