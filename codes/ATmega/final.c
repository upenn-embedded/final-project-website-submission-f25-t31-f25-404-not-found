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


#define STRIKE_THRESHOLD    1.8f
#define RESET_THRESHOLD    -0.2f

#define LED_PORT    PORTB
#define LED_DDR     DDRB
#define LED_PIN     PB5


#define TFT_CS_PIN   PB2
#define D_C_PIN      PB0
#define RESET_PIN    PB1


#define COL_BG       rgb565(0, 0, 0)
#define COL_FG       rgb565(255, 255, 255)
#define COL_ACCENT   rgb565(40, 200, 255)
#define COL_IMPACT   rgb565(255, 220, 0)   // Bright Yellow/Gold for strike


#define HEADER_Y     2
#define DRUM_LABEL_Y 25
#define STRIKE_LINE_Y 100 

typedef enum {
    WAITING_FOR_STRIKE = 0,
    STRIKE_DETECTED_WAIT_UP = 1
} strike_state_t;




static void draw_static_info(void) {
  
    LCD_drawBlock(0, 0, LCD_WIDTH - 1, 15, COL_BG);
    LCD_drawString(2, HEADER_Y, "V-DRUMS TRIGGER", COL_FG, COL_BG);

  
    LCD_drawBlock(0, DRUM_LABEL_Y, LCD_WIDTH - 1, DRUM_LABEL_Y + 15, COL_BG);
    LCD_drawString(2, DRUM_LABEL_Y, "PAD:", COL_FG, COL_BG);
    LCD_drawString(35, DRUM_LABEL_Y, "SNARE", COL_ACCENT, COL_BG);
}


static void display_strike_message(bool show, float az) {
    

    uint8_t text_y = STRIKE_LINE_Y;
    
 
    const uint8_t STRIKE_WIDTH_PIXELS = 6 * 7; 

    if (show) {
        char buf[20];
        

        LCD_drawBlock(0, text_y - 5, LCD_WIDTH - 1, LCD_HEIGHT - 1, COL_IMPACT);
        

        LCD_drawString((LCD_WIDTH/2) - (STRIKE_WIDTH_PIXELS / 2), text_y + 3, "STRIKE!", COL_BG, COL_IMPACT);
        
   
        snprintf(buf, sizeof(buf), "Impact: %.2f g", az);
        

        uint8_t impact_width_pixels = 6 * (uint8_t)strlen(buf);
        LCD_drawString((LCD_WIDTH/2) - (impact_width_pixels / 2), text_y + 20, buf, COL_BG, COL_IMPACT);
        

        _delay_ms(150);
        

        LCD_drawBlock(0, text_y - 5, LCD_WIDTH - 1, LCD_HEIGHT - 1, COL_BG);
    } 
}

int main(void)
{

    uart_init();
    

    LED_DDR |= (1 << LED_PIN);
    PORTB &= ~(1 << LED_PIN); 


    if (IMU_init(0x6B) < 0)
    {
        printf("ERROR: IMU not found!\r\n");
        while (1);
    }
    

    DDRB |= (1 << TFT_CS_PIN) | (1 << D_C_PIN) | (1 << RESET_PIN);
    

    lcd_init(); 
    LCD_setScreen(COL_BG);
    LCD_rotate(3); 
    

    draw_static_info();
    

    uint8_t buf[6];
    float az;
    strike_state_t state = WAITING_FOR_STRIKE;

    while (1)
    {

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
        _delay_ms(20); 
    }
}
