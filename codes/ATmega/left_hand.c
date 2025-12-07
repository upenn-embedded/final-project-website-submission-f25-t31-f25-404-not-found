#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "uart.h"
#include "i2c.h"
#include "imu.h"
#include "ST7735.h"
#include "LCD_GFX.h"

// --------------------------------------------
// Configuration (IMU/Strike with Peak Detection)
// --------------------------------------------
#define PEAK_THRESHOLD      -2.0f     // Peak must be below -2.0g (downward)
#define BUFFER_SIZE         3         // Store 3 samples for peak detection
#define MIN_SAMPLES_BETWEEN 3         // ~60ms gap between taps (at 50Hz)
#define AVG_WINDOW_SIZE     10        // Rolling average over last 10 strikes

#define LED_PORT    PORTB
#define LED_DDR     DDRB
#define LED_PIN     PB5

// --------------------------------------------
// Configuration (LCD Control Pins)
// --------------------------------------------
#define TFT_CS_PIN   PB2
#define D_C_PIN      PB0
#define RESET_PIN    PB1

// --------------------------------------------
// Screen Definitions
// --------------------------------------------
#define COL_BG       rgb565(0, 0, 0)
#define COL_FG       rgb565(255, 255, 255)
#define COL_ACCENT   rgb565(40, 200, 255)
#define COL_IMPACT   rgb565(255, 220, 0)
#define COL_AVG      rgb565(100, 255, 100) // Green for average
#define COL_READY    rgb565(0, 255, 0)     // Green for ready
#define COL_ERROR    rgb565(255, 0, 0)     // Red for error

// Layout Coordinates (Landscape mode - 160x128)
#define STATUS_Y     2
#define DRUM_LABEL_Y 25
#define HAND_LABEL_Y 45
#define AVG_LABEL_Y  65

// Rolling average tracking
static float strike_history[AVG_WINDOW_SIZE] = {0};
static uint8_t strike_count = 0;
static uint8_t strike_index = 0;

// --- Helper Functions for Display ---

// Update status display
static void update_status(const char* status, uint16_t color) {
    LCD_drawBlock(0, STATUS_Y, LCD_WIDTH - 1, STATUS_Y + 15, COL_BG);
    LCD_drawString(2, STATUS_Y, "STATUS:", COL_FG, COL_BG);
    LCD_drawString(55, STATUS_Y, status, color, COL_BG);
}

// Calculate and return rolling average
static float get_rolling_average(void) {
    if (strike_count == 0) return 0.0f;
    
    float sum = 0.0f;
    uint8_t count = (strike_count < AVG_WINDOW_SIZE) ? strike_count : AVG_WINDOW_SIZE;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += strike_history[i];
    }
    
    return sum / count;
}

// Update rolling average display
static void update_avg_display(void) {
    char buf[20];
    float avg = get_rolling_average();
    
    // Clear previous average line
    LCD_drawBlock(0, AVG_LABEL_Y, LCD_WIDTH - 1, AVG_LABEL_Y + 15, COL_BG);
    
    // Display new average
    LCD_drawString(2, AVG_LABEL_Y, "AVG:", COL_FG, COL_BG);
    snprintf(buf, sizeof(buf), "%.2f g", avg);
    LCD_drawString(35, AVG_LABEL_Y, buf, COL_AVG, COL_BG);
}

// Initializes the static headers on the screen
static void draw_static_info(void) {
    // 1. Status (will be updated after IMU check)
    update_status("INIT...", COL_ACCENT);

    // 2. Drum Type Label
    LCD_drawBlock(0, DRUM_LABEL_Y, LCD_WIDTH - 1, DRUM_LABEL_Y + 15, COL_BG);
    LCD_drawString(2, DRUM_LABEL_Y, "PAD:", COL_FG, COL_BG);
    LCD_drawString(35, DRUM_LABEL_Y, "HIHAT", COL_ACCENT, COL_BG);
    
    // 3. Hand Label
    LCD_drawBlock(0, HAND_LABEL_Y, LCD_WIDTH - 1, HAND_LABEL_Y + 15, COL_BG);
    LCD_drawString(2, HAND_LABEL_Y, "HAND:", COL_FG, COL_BG);
    LCD_drawString(45, HAND_LABEL_Y, "LEFT", COL_ACCENT, COL_BG);
    
    // 4. Initialize average display
    update_avg_display();
}

// Add strike to rolling average
static void add_to_average(float force) {
    strike_history[strike_index] = force;
    strike_index = (strike_index + 1) % AVG_WINDOW_SIZE;
    
    if (strike_count < AVG_WINDOW_SIZE) {
        strike_count++;
    }
    
    // Update average display
    update_avg_display();
}

int main(void)
{
    // --- 1. System/Communication Init ---
    uart_init();
    
    // LED Output
    LED_DDR |= (1 << LED_PIN);
    PORTB &= ~(1 << LED_PIN);

    // --- 2. LCD Init ---
    DDRB |= (1 << TFT_CS_PIN) | (1 << D_C_PIN) | (1 << RESET_PIN);
    
    lcd_init(); 
    LCD_setScreen(COL_BG);
    LCD_rotate(3);
    
    draw_static_info();
    
    // --- 3. IMU Init with Status Update ---
    if (IMU_init(0x6B) < 0)
    {
        update_status("IMU NOT FOUND", COL_ERROR);
        while (1) {
            _delay_ms(1000);
        }
    }
    
    // IMU initialized successfully
    update_status("READY", COL_READY);
    
    // --- 4. Peak Detection Variables ---
    float ay_buffer[BUFFER_SIZE] = {0};
    uint8_t buf_index = 0;
    uint8_t samples_filled = 0;
    uint16_t samples_since_tap = MIN_SAMPLES_BETWEEN;
    
    uint8_t imu_buf[6];

    while (1)
    {
        // Read accelerometer
        if (IMU_readAccBytes(imu_buf) == 0)
        {
            // Read Y-axis
            int16_t raw_y = (int16_t)((imu_buf[3] << 8) | imu_buf[2]);
            float ay = raw_y / 4096.0f;

            // Store in circular buffer
            ay_buffer[buf_index] = ay;
            buf_index = (buf_index + 1) % BUFFER_SIZE;
            
            if (samples_filled < BUFFER_SIZE)
            {
                samples_filled++;
            }
            
            samples_since_tap++;

            // Peak detection: need full buffer
            if (samples_filled >= BUFFER_SIZE && samples_since_tap >= MIN_SAMPLES_BETWEEN)
            {
                // Get indices: prev, current (peak candidate), next
                uint8_t prev_idx = (buf_index + BUFFER_SIZE - 2) % BUFFER_SIZE;
                uint8_t curr_idx = (buf_index + BUFFER_SIZE - 1) % BUFFER_SIZE;
                uint8_t next_idx = buf_index;
                
                float prev = ay_buffer[prev_idx];
                float curr = ay_buffer[curr_idx];
                float next = ay_buffer[next_idx];
                
                // Check if current is a downward peak (most negative)
                if (curr < PEAK_THRESHOLD && curr < prev && curr < next)
                {
                    float strike_force = -curr;  // Convert to positive
                    
                    // Send UART immediately - no blocking
                    LED_PORT |= (1 << LED_PIN);
                    uart_send('2', NULL);
                    
                    // Add to rolling average
                    add_to_average(strike_force);
                    
                    // Brief LED flash
                    _delay_ms(50);
                    LED_PORT &= ~(1 << LED_PIN);
                    
                    samples_since_tap = 0;
                }
            }
        }
        
        _delay_ms(20); // 50 Hz loop
    }
}
