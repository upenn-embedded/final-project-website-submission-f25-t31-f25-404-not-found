#include <BluetoothSerial.h>
#include "driver/i2s.h"
#include "WavData.h"
HardwareSerial HC05(2);
// ---------------- BUTTON PINS ----------------
#define BTN1 13
#define BTN2 12
#define BTN3 14

// ---------------- I2S PINS ----------------
#define BCLK 27
#define LRCLK 26
#define DIN 25

// ---------------- BLUETOOTH ----------------
BluetoothSerial SerialBT;

// HC-05 MAC address
uint8_t hc05Address[6] = {0x00, 0x18, 0x91, 0xD6, 0xD7, 0x26};

// Playback structure
typedef struct {
    const unsigned char* data;
    uint32_t idx;
    uint32_t size;
    bool active;
} PLAYBACK_T;

#define MAX_SOUNDS 3
PLAYBACK_T sounds[MAX_SOUNDS];

// -----------------------------------------------------------
// I2S CONFIG
// -----------------------------------------------------------
static const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = 0,
    .tx_desc_auto_clear = true,
};

static const i2s_pin_config_t pin_config = {
    .bck_io_num = BCLK,
    .ws_io_num = LRCLK,
    .data_out_num = DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
};

// -----------------------------------------------------------
// Start playing a WAV file (non-blocking)
// -----------------------------------------------------------
void playSound(const unsigned char* wav)
{
    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        if (!sounds[i].active)
        {
            uint32_t dataSize = *(uint32_t*)(wav + 40);   // read byte count at header offset 40
            sounds[i].data = wav + 44;                    // skip header
            sounds[i].idx = 0;
            sounds[i].size = dataSize;
            sounds[i].active = true;
            return;
        }
    }
}

// -----------------------------------------------------------
// MIXER: mixes all active sounds and sends samples to I2S
// -----------------------------------------------------------
void IRAM_ATTR mixAudio()
{
    int16_t left = 0;
    int16_t right = 0;

    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        if (sounds[i].active)
        {
            if (sounds[i].idx >= sounds[i].size)
            {
                sounds[i].active = false;
                continue;
            }

            int16_t sL = *(int16_t*)(sounds[i].data + sounds[i].idx);
            int16_t sR = *(int16_t*)(sounds[i].data + sounds[i].idx + 2);

            left += sL / 2;
            right += sR / 2;

            sounds[i].idx += 4;
        }
    }

    uint8_t frame[4];
    frame[0] = left & 0xFF;
    frame[1] = left >> 8;
    frame[2] = right & 0xFF;
    frame[3] = right >> 8;

    size_t bw;
    i2s_write(I2S_NUM_0, frame, 4, &bw, portMAX_DELAY);
}

// -----------------------------------------------------------
// SETUP
// -----------------------------------------------------------
void setup()
{
    Serial.begin(9600);
    HC05.begin(9600, SERIAL_8N1, 16, 17);
    HC05.begin(9600, SERIAL_8N1);

    pinMode(BTN1, INPUT_PULLUP);
    pinMode(BTN2, INPUT_PULLUP);
    pinMode(BTN3, INPUT_PULLUP);

    // Setup I2S
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_set_sample_rates(I2S_NUM_0, 44100);

    // Setup Bluetooth
    if (!SerialBT.begin("ESP32_MASTER", true)) {  // true = master mode
        Serial.println("Failed to start BT in master mode");
        while (1) { delay(1000); }
    }
    Serial.println("ESP32 Bluetooth started in master mode");

    // Set PIN if needed
    SerialBT.setPin("1234",4);

    // Try initial connection
    if (SerialBT.connect(hc05Address)) {
        Serial.println("Connected to HC-05!");
    } else {
        Serial.println("Failed to connect to HC-05");
    }
}

// -----------------------------------------------------------
// MAIN LOOP
// -----------------------------------------------------------
void loop()
{
    // Handle physical buttons
    if (digitalRead(BTN1) == 0) playSound(snare);
    if (digitalRead(BTN2) == 0) playSound(hihat);
    if (digitalRead(BTN3) == 0) playSound(kick);

    // Handle internal Bluetooth input
    while (SerialBT.available())
    {
        char cmd = SerialBT.read();
        Serial.println(cmd);

        switch (cmd)
        {
            case '1': playSound(snare); break;
            case '2': playSound(hihat); break;
            case '3': playSound(kick); break;
        }
    }

    // Handle wired HC-05 input (UART2)
    while (HC05.available())
    {
        char cmd = HC05.read();
        Serial.println(cmd);

        switch (cmd)
        {
            case '1': playSound(snare); break;
            case '2': playSound(hihat); break;
            case '3': playSound(kick); break;
        }
    }
    // --- UART0 (Serial, used after USB is removed) ---
    while (Serial.available())
    {
        char cmd = Serial.read();
        Serial.println(cmd);

        switch (cmd)
        {
            case '1': playSound(snare); break;
            case '2': playSound(hihat); break;
            case '3': playSound(kick); break;
        }
    }


    // Optional: auto-reconnect if disconnected
    static unsigned long lastTry = 0;
    if (!SerialBT.connected() && millis() - lastTry > 5000)
    {
        lastTry = millis();
        Serial.println("Reconnecting to HC-05...");
        SerialBT.connect(hc05Address);
    }

    mixAudio(); // continuously mix + output
}
