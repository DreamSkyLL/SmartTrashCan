#include <WiFi.h>
#include "esp_camera.h"
#include <VL53L0X.h>
#include <Servo.h>

//以下三个定义为调试定义
#define DebugBegin(baud_rate)    Serial.begin(baud_rate)
#define DebugPrintln(message)    Serial.println(message)
#define DebugPrint(message)    Serial.print(message)

//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#if defined(CAMERA_MODEL_ESP_EYE)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

#elif defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#endif

#define drop_pin GPIO_NUM_16//接按钮

#define servo1 GPIO_NUM_12//接第一级舵机PWM
#define servo2 GPIO_NUM_13//接第二级舵机PWM

#define XSHUT1 GPIO_NUM_4
#define XSHUT2 GPIO_NUM_2
#define XSHUT3 GPIO_NUM_1
#define XSHUT4 GPIO_NUM_3
#define I2C_SDA GPIO_NUM_15
#define I2C_SCL GPIO_NUM_14

#define SEPARATOR String("<SEPARATOR>")


const char *ssid = "yike-dev";

const char *password = "tjgxwscx";

const uint16_t recognition_port = 51888;
//const char * recognition_host = "192.168.2.38"; // ip or dns
const char * recognition_host = "192.168.31.238";

const uint16_t listen_port=80;

const uint8_t queue_size=4;

WiFiClient recognition_client;
WiFiServer bin_server;

int org_value;
int buttonState;

Servo s1,s2;

VL53L0X sensor;


camera_fb_t recent[queue_size];
uint8_t type[queue_size],back=0,front=0,full=0;

void wifi_init(){
    WiFi.mode(WIFI_STA);
    DebugPrint("Wait for Smartconfig");
    delay(2000);
    // 等待配网
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        DebugPrint(".");
    }
    DebugPrintln("WiFi connected");
    DebugPrint("IP address: ");
    DebugPrintln(WiFi.localIP());
}

void camera_init(){
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
    //                      for larger pre-allocated frame buffer.
    if(psramFound()){
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%org_value", err);
        return;
    }

    sensor_t * s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1); // flip it back
        s->set_brightness(s, 1); // up the brightness just a bit
        s->set_saturation(s, -2); // lower the saturation
    }
    // drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif
}

void servo_init(){
    s1.attach(servo1);
    s2.attach(servo2);
    s1.write(0);
    delay(1000);
    s1.write(45);
    delay(1000);
    s2.write(0);
    delay(1000);
    s2.write(45);
    delay(1000);
}

void VL53L0X_init(){
    pinMode(XSHUT1,OUTPUT);
    pinMode(XSHUT2,OUTPUT);
    pinMode(XSHUT3,OUTPUT);
    pinMode(XSHUT4,OUTPUT);
    digitalWrite(XSHUT1,LOW);
    digitalWrite(XSHUT2,LOW);
    digitalWrite(XSHUT3,LOW);
    digitalWrite(XSHUT4,LOW);
}



void photo_to_server(){
    DebugPrintln("Taking picture...");
    camera_fb_t *pic = esp_camera_fb_get();
    Serial.printf("Picture taken! Its size was: %d bytes\r\n",pic->len);

    DebugPrint("connecting to ");
    DebugPrintln(recognition_host);
    if (!recognition_client.connect(recognition_host, recognition_port)) {
        DebugPrintln("connection failed");
        DebugPrintln("wait 5 sec...");
        delay(5000);
        return;
    }

    DebugPrintln("sending picture to server...");
    recognition_client.print(String(pic->len) + SEPARATOR);
    recognition_client.write(pic->buf, pic->len);
    DebugPrintln("picture sent");
    DebugPrint("kind:");

    char c, c_next;
    while (recognition_client.connected()) {
        while (recognition_client.available()) {
            c_next = recognition_client.read();
            DebugPrintln(c_next);
            if (c_next == '\r') break;
            c = c_next;
        }
        delay(10);
    }
    int kind = c - '0';
    DebugPrintln();


//    char* s=recognition_client.read();
//    int res=s.toInt();
    Serial.printf("prediction result: %d\r\n",kind);

    s1.attach(servo1);
    s2.attach(servo2);
    if (kind != 9){
        if (kind%2) {
            s1.write(0);
            delay(3000);
            DebugPrintln("first board turn to 135");
        } else {
            s1.write(90);
            delay(3000);
            DebugPrintln("first board turn to 225");
        }
        s1.write(45);
        delay(1000);
        if (kind>1) {
            s2.write(0);
            delay(3000);
            DebugPrintln("second board turn to 135");
        } else {
            s2.write(90);
            delay(3000);
            DebugPrintln("second board turn to 225");
        }
        s2.write(45);
        delay(1000);
        DebugPrintln("rubbish dropped!");
    } else {
    DebugPrintln("cannot recognize rubbish!");
    }

    recognition_client.stop();
    esp_camera_fb_return(pic);
}

int get_length(uint8_t* value){
    digitalWrite(XSHUT1,value[0]);
    digitalWrite(XSHUT2,value[1]);
    digitalWrite(XSHUT3,value[2]);
    digitalWrite(XSHUT4,value[3]);

    Wire.begin(I2C_SDA,I2C_SCL);
    sensor.setAddress(0x29);
    sensor.setTimeout(500);
    if (!sensor.init())
    {
        DebugPrintln("Failed to detect and initialize sensor!");
        return -1;
    }
    int range=sensor.readRangeSingleMillimeters();
    if (sensor.timeoutOccurred()) { Serial.print(" TIMEOUT"); }
    delay(10);
    DebugPrintln(range);
    return range;
}

void setup() {
    DebugBegin(115200);

    DebugPrintln("initializing VL53L0X sensor...");
    VL53L0X_init();
    DebugPrintln("initializing servo...");
    servo_init();
    DebugPrintln("initializing wifi...");
    wifi_init();
    DebugPrintln("initializing switch...");
    pinMode(drop_pin, INPUT_PULLUP);
    org_value = digitalRead(drop_pin);
    DebugPrintln(org_value);
    DebugPrintln("initializing camera...");
    camera_init();
    DebugPrintln("initializing server...");
    bin_server.begin(listen_port);

    DebugPrintln("initialization over");

}

void loop() {
    buttonState = digitalRead(drop_pin);
    if (buttonState == LOW)
    {
        photo_to_server();
        DebugPrintln("photo token");
        delay(1000);
    }
    delay(10);
}
