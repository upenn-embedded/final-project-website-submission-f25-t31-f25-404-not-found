#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "uart.h"
#include "i2c.h"
#include "imu.h"

#define PEAK_THRESHOLD      0.0f     // Peak must be below -1.0g (downward)
#define BUFFER_SIZE         3         // Store 3 samples for peak detection
#define MIN_SAMPLES_BETWEEN 20        // ~100ms gap between taps (

#define LED_PORT   PORTB
#define LED_DDR    DDRB
#define LED_PIN    PB5

int main(void)
{
    uart_init();
    
    LED_DDR |= (1 << LED_PIN);
    LED_PORT &= ~(1 << LED_PIN);

    if (IMU_init(0x6B) < 0)
    {
        while (1);
    }

    uint8_t buf[6];
    float az_buffer[BUFFER_SIZE] = {0};
    uint8_t buf_index = 0;
    uint8_t samples_filled = 0;
    uint16_t samples_since_tap = MIN_SAMPLES_BETWEEN;

    while (1)
    {
        if (IMU_readAccBytes(buf) == 0)
        {
            int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);
            float az = raw_z / 4096.0f;


            az_buffer[buf_index] = az;
            buf_index = (buf_index + 1) % BUFFER_SIZE;
            
            if (samples_filled < BUFFER_SIZE)
            {
                samples_filled++;
            }
            
            samples_since_tap++;

            if (samples_filled >= BUFFER_SIZE && samples_since_tap >= MIN_SAMPLES_BETWEEN)
            {
                uint8_t prev_idx = (buf_index + BUFFER_SIZE - 2) % BUFFER_SIZE;
                uint8_t curr_idx = (buf_index + BUFFER_SIZE - 1) % BUFFER_SIZE;
                uint8_t next_idx = buf_index;
                
                float prev = az_buffer[prev_idx];
                float curr = az_buffer[curr_idx];
                float next = az_buffer[next_idx];
                
             
                if (curr < PEAK_THRESHOLD && curr < prev && curr < next)
                {
                    LED_PORT |= (1 << LED_PIN);
                    uart_send('3', NULL);
                    samples_since_tap = 0;
                    
                    _delay_ms(50);  
                    LED_PORT &= ~(1 << LED_PIN);
                }
            }
        }

        _delay_ms(5);  
    }
}
