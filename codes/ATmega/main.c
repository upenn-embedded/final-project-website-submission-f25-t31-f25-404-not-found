#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "uart.h"
#include "i2c.h"
#include "imu.h"

int main(void)
{

    uart_init();
    printf("UART Initialized.\r\n");


    printf("Initializing IMU...\r\n");
    if (IMU_init(0x6B) < 0)
    {
        printf("ERROR: IMU not found at address 0x6B!\r\n");
        printf("Check wiring: SDA->PC4, SCL->PC5, 3.3V, GND\r\n");
        while (1);
    }

    printf("IMU Ready! Reading acceleration...\r\n");

    uint8_t buf[6];
    float ax, ay, az;

    while (1) {

        if (IMU_readAccBytes(buf) == 0) {

            printf("RAW BYTES: %02X %02X  %02X %02X  %02X %02X\r\n",
                   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

  
            int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
            int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
            int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);


            ax = raw_x / 4096.0f;
            ay = raw_y / 4096.0f;
            az = raw_z / 4096.0f;

            printf("ACC (g): X=%.3f  Y=%.3f  Z=%.3f\r\n", ax, ay, az);
        }
        else {
            printf("I2C READ ERROR\r\n");
        }

        _delay_ms(10000);
    }
}
