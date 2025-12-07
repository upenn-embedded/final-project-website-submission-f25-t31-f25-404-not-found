#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> // <-- Added to fix 'bool' and 'true' errors
#include "uart.h"
#include "i2c.h"
#include "imu.h"
#include "ST7735.h"
#include "LCD_GFX.h"

// --------------------------------------------
// Configuration (IMU/Strike)
// --------------------------------------------
#define STRIKE_THRESHOLD    1.8f
#define RESET_THRESHOLD    -0.2f

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
#define COL_IMPACT   rgb565(255, 220, 0)   // Bright Yellow/Gold for strike

// Layout Coordinates (Landscape mode - 160x128)
#define HEADER_Y     2
#define DRUM_LABEL_Y 25
#define STRIKE_LINE_Y 100 // Bottom area for strike message

typedef enum {
    WAITING_FOR_STRIKE = 0,
    STRIKE_DETECTED_WAIT_UP = 1
} strike_state_t;

// --- Helper Functions for Display ---

// Initializes the static headers on the screen
static void draw_static_info(void) {
    // 1. Title Bar
    LCD_drawBlock(0, 0, LCD_WIDTH - 1, 15, COL_BG);
    LCD_drawString(2, HEADER_Y, "V-DRUMS TRIGGER", COL_FG, COL_BG);

    // 2. Drum Type Label
    LCD_drawBlock(0, DRUM_LABEL_Y, LCD_WIDTH - 1, DRUM_LABEL_Y + 15, COL_BG);
    LCD_drawString(2, DRUM_LABEL_Y, "PAD:", COL_FG, COL_BG);
    LCD_drawString(35, DRUM_LABEL_Y, "SNARE", COL_ACCENT, COL_BG);
}

// Draws the "STRIKE!" message at the bottom center without changing font size
static void display_strike_message(bool show, float az) {
    
    // Y-coordinate for the text area (bottom area)
    uint8_t text_y = STRIKE_LINE_Y;
    
    // Character width is 6 pixels (5 wide + 1 space), STRIKE! is 7 chars
    const uint8_t STRIKE_WIDTH_PIXELS = 6 * 7; 

    if (show) {
        char buf[20];
        
        // 1. Draw a prominent yellow/gold bar across the bottom for the impact
        LCD_drawBlock(0, text_y - 5, LCD_WIDTH - 1, LCD_HEIGHT - 1, COL_IMPACT);
        
        // 2. Print the word STRIKE! in the center
        LCD_drawString((LCD_WIDTH/2) - (STRIKE_WIDTH_PIXELS / 2), text_y + 3, "STRIKE!", COL_BG, COL_IMPACT);
        
        // 3. Display acceleration below the strike message
        snprintf(buf, sizeof(buf), "Impact: %.2f g", az);
        
        // Calculate centered position for the acceleration string
        uint8_t impact_width_pixels = 6 * (uint8_t)strlen(buf);
        LCD_drawString((LCD_WIDTH/2) - (impact_width_pixels / 2), text_y + 20, buf, COL_BG, COL_IMPACT);
        
        // 4. Wait briefly for visual impact (The 'flash')
        _delay_ms(150);
        
        // 5. Clear the message immediately after impact
        LCD_drawBlock(0, text_y - 5, LCD_WIDTH - 1, LCD_HEIGHT - 1, COL_BG);
    } 
}

int main(void)
{
    // --- 1. System/Communication Init ---
    uart_init();
    
    // LED Output
    LED_DDR |= (1 << LED_PIN);
    PORTB &= ~(1 << LED_PIN); // Initialize LED OFF

    // IMU INIT
    if (IMU_init(0x6B) < 0)
    {
        printf("ERROR: IMU not found!\r\n");
        while (1);
    }
    
    // --- 2. LCD Init ---
    // Configure LCD control pins as outputs
    DDRB |= (1 << TFT_CS_PIN) | (1 << D_C_PIN) | (1 << RESET_PIN);
    
    // Initialize LCD (Using the no-argument call)
    lcd_init(); 
    LCD_setScreen(COL_BG);
    LCD_rotate(3); // Landscape mode
    
    // Draw initial static information
    draw_static_info();
    
    // --- 3. Main Logic ---
    uint8_t buf[6];
    float az;
    strike_state_t state = WAITING_FOR_STRIKE;

    while (1)
    {
        // Read accelerometer
        if (IMU_readAccBytes(buf) == 0)
        {
            int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);
            az = raw_z / 4096.0f; 

            switch(state)
            {
                case WAITING_FOR_STRIKE:
                    if (az > STRIKE_THRESHOLD)
                    {
                        LED_PORT |= (1 << LED_PIN);
                        uart_send('3', NULL); 
                        
                        // Display the awesome strike message
                        display_strike_message(true, az);
                        
                        state = STRIKE_DETECTED_WAIT_UP;
                    }
                    break;

                case STRIKE_DETECTED_WAIT_UP:
                    if (az < RESET_THRESHOLD)
                    {
                        LED_PORT &= ~(1 << LED_PIN);
                        state = WAITING_FOR_STRIKE;
                    }
                    break;
            }
        }
        _delay_ms(20); // 50 Hz loop
    }
}
