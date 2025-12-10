#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "uart.h"
#include "i2c.h"
#include "imu.h"


#define STRIKE_THRESHOLD   1.8f    
#define RESET_THRESHOLD   -0.2f    

#define LED_PORT   PORTB
#define LED_DDR    DDRB
#define LED_PIN    PB5             

typedef enum {
    WAITING_FOR_STRIKE = 0,
    STRIKE_DETECTED_WAIT_UP = 1
} strike_state_t;

int main(void)
{

    uart_init();
    printf("UART Initialized.\r\n");

 
    LED_DDR |= (1 << LED_PIN);
    LED_PORT &= ~(1 << LED_PIN);

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
        
        if (IMU_readAccBytes(buf) == 0)
        {
            int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);
            az = raw_z / 4096.0f;  

            switch(state)
            {
                case WAITING_FOR_STRIKE:
                    if (az > STRIKE_THRESHOLD)
                    {
                        printf("DOWNWARD STRIKE DETECTED! Z=%.2f g\r\n", az);
                        LED_PORT |= (1 << LED_PIN);  
                        state = STRIKE_DETECTED_WAIT_UP;
                    }
                    break;

                case STRIKE_DETECTED_WAIT_UP:
                    if (az < RESET_THRESHOLD)
                    {
                        printf("UPWARD MOTION DETECTED. Ready for next strike. Z=%.2f g\r\n", az);
                        LED_PORT &= ~(1 << LED_PIN); 
                        state = WAITING_FOR_STRIKE;
                    }
                    break;
            }
        }

        _delay_ms(20);  
    }
}
