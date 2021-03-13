#include <Arduino.h>
#include <U8x8lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

#include <Wire.h>
#include <VL53L0X.h>
#include <stdio.h>

#define HARMFUL 12
#define OTHER 14
#define XSHUT3 GPIO_NUM_1
#define XSHUT4 GPIO_NUM_3

VL53L0X sensor1;
VL53L0X sensor2;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display

sernsor_init(void){
    //设置引脚输出低电平（此时传感器处于关机状态）
    pinMode(HARMFUL, OUTPUT);
    pinMode(OTHER, OUTPUT);
    digitalWrite(OTHER, LOW);
    digitalWrite(HARMFUL, LOW);
    delay(500);
    Wire.begin();
    //先后开启两个sensor，为其设置地址
    pinMode(OTHER, INPUT);
    delay(150);
    sensor1.init(true);
    delay(100);
    sensor1.setAddress((uint8_t)22);
  
    pinMode(HARMFUL, INPUT);
    delay(150);
    sensor2.init(true);
    delay(100);
    sensor2.setAddress((uint8_t)25);
  
    Serial.println("addresses set");
}


void setup(void){
    Serial.begin (9600);
    
    sensor_init();
    
    u8x8.begin();
    u8x8.setPowerSave(0);
}

void loop(void)
{
    char s[10];
    //读传感器1数值
    int range = sensor1.readRangeSingleMillimeters();
    //换算成百分比
    int percent = (300-range)/3;
    if(percent < 0) percent = 1;
    Serial.print("range:");Serial.println(range);
    sprintf(s, "%d", percent);
    if (sensor2.timeoutOccurred()) Serial.println(" TIMEOUT");
    
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.draw1x2String(1, 1, "                ");
    u8x8.draw1x2String(1, 1, "Hazardous: ");
    u8x8.draw1x2String(1, 4, "                ");
    u8x8.draw1x2String(1, 4, s);
    u8x8.draw1x2String(3, 4, "%");

    u8x8.refreshDisplay();    // only required for SSD1606/7 

    delay(5000);



    //读传感器2数值
    range = sensor2.readRangeSingleMillimeters();
    Serial.println("OK5");
    percent = (300-range)/3;
    if(percent < 0) percent = 1;
    Serial.print("range:");Serial.println(range);
    sprintf(s, "%d", percent);
    if (sensor2.timeoutOccurred()) Serial.print(" TIMEOUT");

    u8x8.draw1x2String(1, 1, "                ");
    u8x8.draw1x2String(1, 1, "Other: ");
    u8x8.draw1x2String(1, 4, "                ");
    u8x8.draw1x2String(1, 4, s);
    u8x8.draw1x2String(3, 4, "%");

    u8x8.refreshDisplay();    // only required for SSD1606/7 

    delay(5000);
}
