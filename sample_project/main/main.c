#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "DHT.h"
#define SensorInput 4

#include <driver/adc.h>
#define MoistureSensorPin32 ADC1_CHANNEL_4

#include <string.h>
#include "driver/i2c.h"
#include "ssd1366.h"
#include "font8x8_basic.h"
#define SDA_PIN 15
#define SCL_PIN 2
#define tag "SSD1306"

#define MaxTmp 26
#define MinTmp 23

#define MinMoi 65

#define Fan_GPIO 5
#define Hiater_GPIO 18
#define Lamp_GPIO 19
#define Pump_GPIO 21

char SensorsInfo[1024];

void i2c_master_init()
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA_PIN,
		.scl_io_num = SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 1000000
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

void ssd1306_init() {

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
	i2c_master_write_byte(cmd, 0x14, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP, true); // reverse left-right mapping
	i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE, true); // reverse up-bottom mapping

	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);
	i2c_master_stop(cmd);

	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
}

void task_ssd1306_display_clear(void *ignore) {
	i2c_cmd_handle_t cmd;

	uint8_t zero[128];
	for (uint8_t i = 0; i < 8; i++) {
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
		i2c_master_write_byte(cmd, 0xB0 | i, true);

		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
		i2c_master_write(cmd, zero, 128, true);
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
	}

	vTaskDelete(NULL);
}

void task_ssd1306_display_text(const void *arg_text) {
	char *text = (char*)arg_text;
	uint8_t text_len = strlen(text);

	i2c_cmd_handle_t cmd;

	uint8_t cur_page = 0;

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	i2c_master_write_byte(cmd, 0x00, true); // reset column
	i2c_master_write_byte(cmd, 0x10, true);
	i2c_master_write_byte(cmd, 0xB0 | cur_page, true); // reset page

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	for (uint8_t i = 0; i < text_len; i++) {
		if (text[i] == '\n') {
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
			i2c_master_write_byte(cmd, 0x00, true); // reset column
			i2c_master_write_byte(cmd, 0x10, true);
			i2c_master_write_byte(cmd, 0xB0 | ++cur_page, true); // increment page

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		} else {
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
			i2c_master_write(cmd, font8x8_basic_tr[(uint8_t)text[i]], 8, true);

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		}
	}

	vTaskDelete(NULL);
}

void FanOn(){
    
	printf(" FAN ON\n ");
	gpio_set_level(Fan_GPIO, 1);
}

void FanOff(){
    
	printf(" FAN OFF\n ");
	gpio_set_level(Fan_GPIO, 0);
}

void HiaterON(){

    printf(" HIATER ON\n ");
	gpio_set_level(Hiater_GPIO, 1);
}

void HiaterOff(){
    
	printf(" HIATER OFF\n ");
	gpio_set_level(Hiater_GPIO, 0);
}

void PumpOn(){
    
	printf(" PUMP ON\n ");
	gpio_set_level(Pump_GPIO, 1);
	vTaskDelay(2000 /  portTICK_RATE_MS);

	printf(" PUMP OFF\n ");
	gpio_set_level(Pump_GPIO, 0);

}
void Sensors_TaskAndContrl(void *parameters){

        setDHTgpio(SensorInput);
        
        while (1)
        {
         
           readDHT();
           printf(" AirHum: %.1f AirTmp: %.1f\n", getHumidity(), getTemperature());

           int CurrTemp = getTemperature();

		   if(CurrTemp >= MaxTmp){

              FanOn();

		   }else { FanOff(); }

		   if (CurrTemp <= MinTmp){

			   HiaterON();

		   }else { HiaterOff(); }
		   
           double moi = adc1_get_raw(MoistureSensorPin32);
		   printf(" SoilMoist: %.1f \n ", moi);
           double moiVal = (moi / 2900) * 100;
           printf(" SoilMoistP: %.1f \n ", moiVal);

		   int CurrMoi = moiVal;

		   if(CurrMoi <= MinMoi){
			  
			  PumpOn();
		   }

		   snprintf(SensorsInfo, sizeof(SensorsInfo),
		    "                 \n  .AirHum: %.1f    \n  .AirTmp: %.1f     \n  .SoMoi:  %.1f    \n                 \n                 \n  .MaxTmp: %d       \n  .MinTmp: %d         \n", getHumidity(), getTemperature(), moiVal, MaxTmp, MinTmp);
           xTaskCreate(&task_ssd1306_display_text, "ssd1306_display_text",  2048, (char *) SensorsInfo, 3, NULL);

		   vTaskDelay( 2000 / portTICK_RATE_MS );
		   
		}
	}

void Lamp_Task(void *parameters){

	gpio_reset_pin(Lamp_GPIO);
    gpio_set_direction(Lamp_GPIO, GPIO_MODE_OUTPUT);

    while (1){
     
	 printf(" LAMP ON\n ");
     gpio_set_level(Lamp_GPIO, 1);
     vTaskDelay(21600000 / portTICK_PERIOD_MS);
     
	 printf(" LAMP OFF\n ");
	 gpio_set_level(Lamp_GPIO, 0);
     vTaskDelay(64800000 / portTICK_PERIOD_MS);
	}
}


void app_main(void)
{ 
    
	i2c_master_init();
	ssd1306_init();

	gpio_reset_pin(Hiater_GPIO);
    gpio_set_direction(Hiater_GPIO, GPIO_MODE_OUTPUT);

	gpio_reset_pin(Fan_GPIO);
    gpio_set_direction(Fan_GPIO, GPIO_MODE_OUTPUT);

	gpio_reset_pin(Pump_GPIO);
    gpio_set_direction(Pump_GPIO, GPIO_MODE_OUTPUT);

	xTaskCreate(&task_ssd1306_display_clear, "ssd1306_display_clear",  2048, NULL, 6, NULL);
	vTaskDelay(100/portTICK_PERIOD_MS);

	xTaskCreate(&Sensors_TaskAndContrl, "Sensors_TaskAndContrl", 5098, NULL, 5, NULL);

	xTaskCreate(&Lamp_Task, "Lamp_Task", 2048, NULL, 5, NULL);

}