# ESP32-CAM Telegram Baby Monitor

This module allows you to capture photos from an ESP32-CAM and receive them via Telegram bot.

## Hardware Required
- ESP32-CAM (AI-Thinker) with OV2640 camera
- FTDI programmer or ESP32-CAM-MB programmer board for flashing

## Features
- 📸 VGA resolution (640x480) photos
- 📱 Telegram bot integration
- 👨‍👩‍👧‍👦 Multi-user support (family members can all use it)
- 🔄 On-demand photo capture with `/photo` command
- 📶 Auto-reconnect WiFi

## Setup Instructions

### 1. Hardware Setup
- Connect your ESP32-CAM to the programmer board
- Make sure the camera module is properly seated

### 2. Build and Flash

First, activate ESP-IDF environment:
```bash
cd baby-monitor
source setup-esp-idf.sh
```

Then build and flash:
```bash
cd esp32-cam-telegram
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-* flash monitor
```

Replace `/dev/cu.usbserial-*` with your actual serial port.

### 3. Using the Bot

1. Open Telegram and search for your bot (the one with token you created)
2. Send `/start` to begin
3. Send `/photo` to receive a photo from the camera
4. All family members can add the bot and use it!

## Configuration

WiFi and Telegram credentials are in `main/main.c`:
- WIFI_SSID: 
- WIFI_PASS: 
- TELEGRAM_BOT_TOKEN: "7209354818:AAE33MWSO8Yg2XLEb97-I30sAMODrD2pLko"

## Troubleshooting

### Camera not working
- Check camera ribbon cable connection
- Ensure OV2640 is properly detected in logs

### WiFi connection issues
- Verify SSID and password are correct
- Check WiFi signal strength
- Make sure router is 2.4GHz (ESP32 doesn't support 5GHz)

### Telegram not responding
- Verify bot token is correct
- Check internet connectivity
- Look at serial monitor for error messages

### Photo quality issues
- Adjust `jpeg_quality` in camera_init() (lower = better quality, but larger file)
- Change `frame_size` for different resolutions

## Serial Monitor Output

You should see:
```
I (xxx) ESP32-CAM-TELEGRAM: ESP32-CAM Telegram Baby Monitor Starting...
I (xxx) ESP32-CAM-TELEGRAM: OV2640 camera detected
I (xxx) ESP32-CAM-TELEGRAM: Camera initialized successfully
I (xxx) ESP32-CAM-TELEGRAM: Connected! IP Address:192.168.x.x
I (xxx) ESP32-CAM-TELEGRAM: WiFi connected successfully!
I (xxx) ESP32-CAM-TELEGRAM: Bot is ready! Send /photo command in Telegram to get a photo.
```

## Commands

- `/start` - Start the bot
- `/photo` - Capture and receive a photo

## Notes

- The bot polls Telegram every 30 seconds for new messages
- Photos are captured at VGA resolution (640x480)
- JPEG quality is set to 12 (good balance between quality and file size)
- Multiple users can use the bot simultaneously
