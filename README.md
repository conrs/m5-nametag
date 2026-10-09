# m5-nametag

A wearable nametag for the round **M5Stack StopWatch** (ESP32-S3, 468×468 screen).

- **Name screen:** your name, scaled as large as fits the round display.
- **QR screen:** a QR code for any link, such as your LinkedIn profile.
- **Tap** the screen to switch between them.
- **Audio ring:** a rainbow ring around the edge that reacts to sound from the built-in microphone.
- **Always upright:** uses the accelerometer to smoothly rotate the content as you tilt it.
- **Hold for 3 seconds** to power off.

## Hardware

Built and tested on the [M5Stack StopWatch](https://docs.m5stack.com/). It uses [M5Unified](https://github.com/m5stack/M5Unified), which detects the board automatically, so other round M5Stack devices with PSRAM may work too. They are untested.

## Setup

1. Install [Arduino CLI](https://arduino.github.io/arduino-cli/), for example `brew install arduino-cli`.
2. Install the ESP32 board package and M5Unified:
   ```
   arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
   arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
   arduino-cli lib install M5Unified
   ```
3. Clone this repo:
   ```
   git clone https://github.com/<you>/m5-nametag.git
   cd m5-nametag
   ```

## Configure

Create `config.local.h` in the repo folder with your details:

```c
#define NAMETAG_NAME   "Ada"
#define NAMETAG_QR_URL "https://linkedin.com/in/ada"
```

This file is git-ignored, so your details stay out of version control. Any value you leave out falls back to the defaults in [`config.h`](config.h).

| Setting | Default | What it does |
|---|---|---|
| `NAMETAG_NAME` | `"Your Name"` | Text on the name screen. Shorter names display bigger. |
| `NAMETAG_QR_URL` | `"https://example.com"` | What the QR code links to. |
| `NAMETAG_ROTATION_DIRECTION` | `1` | Set to `-1` if the content turns the wrong way when tilted. |

## Flash

Plug the device in over USB and find its port with `ls /dev/cu.usbmodem*` on macOS. On Linux it is usually `/dev/ttyACM0`. Then run:

```
arduino-cli compile --upload -p /dev/cu.usbmodem101 \
  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,PSRAM=opi .
```

## Troubleshooting

- **Screen shows random lines:** PSRAM isn't enabled. Make sure `PSRAM=opi` is in the `--fqbn`.
- **"Port is busy":** another program has the serial port open, such as `screen`, `cat` or the Arduino IDE serial monitor. Close it and retry.
- **Upload fails or the device seems stuck:** hold the button while plugging in USB to force download mode, then flash again.
- **Startup log:** run `cat /dev/cu.usbmodem101` just after flashing to see `nametag ready: board=… mic=… imu=…`.

## License

[MIT](LICENSE)
