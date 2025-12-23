# ESP32-CAM Telegram Baby Monitor

High-performance baby monitor using ESP32-CAM with Telegram bot integration. Optimized for fast response times (2-3 seconds) with LED flash support.

## Hardware Required
- **ESP32-CAM-MB** development board
- **OV3660** camera sensor (1600×1200 max resolution)
- **8MB PSRAM** minimum
- **4MB Flash** memory
- **GPIO4** LED flash (built-in)

## Features
- 📸 **Fast Photo Capture**: 2-3 second response time (flash + capture + upload)
- 💬 **Telegram Bot Commands**: /start, /photo, /help, /flash on/off
- 🔦 **LED Flash Control**: 800ms optimal exposure timing
- ⚡ **Performance Optimized**: WiFi power save disabled, buffer overflow protection
- 🎨 **XGA Resolution**: 1024×768 for speed/quality balance (23-120KB images)
- 🛡️ **Error Handling**: User-friendly messages and retry logic

## Setup Instructions

### 1. Hardware Setup
- Connect your ESP32-CAM to the programmer board
- Make sure the camera module is properly seated

### 2. Configure Credentials

**⚠️ IMPORTANT - Credentials Setup:**

Copy the example secrets file:
```bash
cp main/secrets.h.example main/secrets.h
```

Edit `main/secrets.h` with your settings:
```c
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define TELEGRAM_BOT_TOKEN "your_bot_token_from_botfather"
#define TELEGRAM_CHAT_ID "your_chat_id"
```

**Getting Telegram Credentials:**
1. Create bot with [@BotFather](https://t.me/botfather) using `/newbot`
2. Copy the bot token (format: `1234567890:ABCdefGHIjklMNOpqrsTUVwxyz`)
3. Get your chat_id from [@userinfobot](https://t.me/userinfobot)

### 3. Build and Flash

### 3. Build and Flash

Set up ESP-IDF environment:
```bash
. $HOME/esp/esp-idf/export.sh
```

Build and flash:
```bash
idf.py build
idf.py -p /dev/cu.usbserial-110 flash monitor
```

Replace `/dev/cu.usbserial-110` with your actual serial port.

### 4. Using the Bot

1. Open Telegram and search for your bot
2. Send `/start` to see available commands
3. Send `/photo` to receive a photo (2-3 seconds)
4. Use `/flash on` or `/flash off` to control LED
5. Send `/help` anytime to see commands and flash status

## Commands

| Command | Description |
|---------|-------------|
| `/start` | Show welcome message and available commands |
| `/photo` | Take a photo (2-3 seconds response time) |
| `/flash on` | Enable LED flash for photos |
| `/flash off` | Disable LED flash |
| `/help` | Show available commands and flash status |

## Performance Metrics

Actual measured performance:
- **Flash Duration**: 800ms (optimal for auto-exposure)
- **Capture Time**: 2-190ms (lighting dependent)
- **Upload Time**: 1-2 seconds (23-120KB images)
- **Total Response**: 2-3 seconds end-to-end

## Configuration

Credentials are stored in `main/secrets.h` (not tracked by git for security).

Current camera settings (in main.c):
```c
Resolution: FRAMESIZE_XGA (1024×768)
JPEG Quality: 8
Denoise: 1 (fast)
Contrast: 2
Saturation: 2
Sharpness: 3
WiFi Power Save: Disabled (for speed)
```

## Troubleshooting

### Photos not received
- Check WiFi credentials in `secrets.h`
- Verify Telegram bot token is correct
- Ensure chat_id matches your Telegram user ID
- Check serial monitor for errors

### Camera initialization failed
- Check camera ribbon cable connection
- Verify OV3660 sensor is detected in logs
- Ensure 8MB PSRAM is available
- Try power cycling the ESP32-CAM

### WiFi connection issues
- Verify SSID and password in `secrets.h`
- Check WiFi signal strength
- Ensure router is 2.4GHz (ESP32 doesn't support 5GHz)
- Look for "Connected! IP Address" in serial monitor

### Slow upload times
- WiFi power save is automatically disabled
- Check WiFi signal strength
- Reduce JPEG quality (increase quality number) if needed

### Images too dark with flash
- Flash duration is set to 800ms (tested optimal)
- For far distances in low light, increase to 1000-1500ms in main.c
- Edit around line 467: `vTaskDelay(pdMS_TO_TICKS(800));`

### Buffer overflow (FB-OVF) errors
- Code automatically flushes stale frames
- If still occurring, increase stabilization delay at line 462

## Security Notes

⚠️ **IMPORTANT - Keep Your Credentials Safe:**
- **Never commit `secrets.h`** - It contains sensitive credentials
- Keep your Telegram bot token private
- The `.gitignore` file protects `secrets.h` from git
- Consider making your GitHub repository private
- Use `secrets.h.example` as template, never put real credentials there

## Serial Monitor Output

You should see:
```
I (xxx) ESP32-CAM-TELEGRAM: ESP32-CAM Telegram Baby Monitor Starting...
I (xxx) ESP32-CAM-TELEGRAM: OV3660 camera detected
I (xxx) ESP32-CAM-TELEGRAM: Camera initialized successfully
I (xxx) ESP32-CAM-TELEGRAM: Connected! IP Address:192.168.x.x
I (xxx) ESP32-CAM-TELEGRAM: WiFi connected successfully!
I (xxx) ESP32-CAM-TELEGRAM: Bot is ready! Send /photo command

[PERF] Command received at 12345 ms
[PERF] Starting capture at 13145 ms
[PERF] Capture complete at 13147 ms
[PERF] Starting photo upload at 13150 ms
[PERF] Photo upload complete at 14212 ms
```

## Technical Details

### WiFi Optimization
- Power save disabled: `esp_wifi_set_ps(WIFI_PS_NONE)`
- Reduces latency by ~50%

### Buffer Management
- Automatic stale frame flushing before capture
- Prevents FB-OVF (framebuffer overflow) errors
- 100ms stabilization delay

### Flash Control
- GPIO4 output mode
- 800ms duration for optimal exposure
- Auto-exposure enabled for adaptive lighting

## License

MIT License - Free to use and modify

## Credits

Built with ESP-IDF v5.3.4:
- `espressif/esp32-camera` v2.1.4
- `espressif/esp_jpeg` for JPEG handling
