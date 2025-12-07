#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "uart.h"
#include "i2c.h"
#include "imu.h"

// --------------------------------------------
// Configuration
// --------------------------------------------
#define STRIKE_THRESHOLD   1.8f     // g – downward hit
#define RESET_THRESHOLD   -0.2f    // g – upward recovery

#define LED_PORT   PORTB
#define LED_DDR    DDRB
#define LED_PIN    PB5             // Built-in LED

typedef enum {
    WAITING_FOR_STRIKE = 0,
    STRIKE_DETECTED_WAIT_UP = 1
} strike_state_t;

int main(void)
{
    // UART
    uart_init();
    printf("UART Initialized.\r\n");

    // LED output
    LED_DDR |= (1 << LED_PIN);
    LED_PORT &= ~(1 << LED_PIN);

    // IMU INIT
    printf("Initializing IMU...\r\n");
    if (IMU_init(0x6B) < 0)
    {
        printf("ERROR: IMU not found!\r\n");
        while (1);
    }

    printf("IMU Ready! Starting drum strike detection...\r\n");

    uint8_t buf[6];
    float az;
    strike_state_t state = WAITING_FOR_STRIKE;

    while (1)
    {
        // Read accelerometer
        if (IMU_readAccBytes(buf) == 0)
        {
            int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);
            az = raw_z / 4096.0f;  // assuming ±16g → 4096 LSB/g

            switch(state)
            {
                case WAITING_FOR_STRIKE:
                    if (az > STRIKE_THRESHOLD)
                    {
                        //printf("DOWNWARD STRIKE DETECTED! Z=%.2f g\r\n", az);
                        LED_PORT |= (1 << LED_PIN);  // turn LED ON
                        uart_send('3', NULL);        // send '1' via UART to ESP32
                        state = STRIKE_DETECTED_WAIT_UP;
                    }
                    break;

                case STRIKE_DETECTED_WAIT_UP:
                    if (az < RESET_THRESHOLD)
                    {
                        //printf("UPWARD MOTION DETECTED. Ready for next strike. Z=%.2f g\r\n", az);
                        LED_PORT &= ~(1 << LED_PIN); // turn LED OFF
                        state = WAITING_FOR_STRIKE;
                    }
                    break;
            }
        }
        else
        {
            //printf("I2C READ ERROR\r\n");
        }

        _delay_ms(20);  // 50 Hz loop
    }
}
