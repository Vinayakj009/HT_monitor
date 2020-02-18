/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <Arduino.h>
#include <Arduino_JSON.h>
#include <time.h>
#include <BearSSLHelpers.h>
#include<WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#ifndef TARGET_DEVICE_ID
#error "Target device id is not specified"
#define TARGET_DEVICE_ID NULL
#endif


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MQTT_BASE "/PT"

#define SUBSCRIBE_BASE MQTT_BASE "/C/" STR(TARGET_DEVICE_ID) "/#"
#define RESPONSE_BASE   MQTT_BASE "/R/" STR(TARGET_DEVICE_ID) "/"
#define DATA_PUBLISH_BASE MQTT_BASE "/D/" STR(TARGET_DEVICE_ID) "/"



#define DHTPIN            2
#define DHTTYPE           DHT22

DHT_Unified dht(DHTPIN, DHTTYPE);


class Command_Handler{
public:
    String handle(String topic, uint8_t* payload, unsigned int payload_size){
        if(!topic.endsWith(this->command)){
            return "";
        }
        return this->callback(topic, payload, payload_size);
    }
    Command_Handler* next() { return _next; }
    void next(Command_Handler* r) { _next = r; }
    Command_Handler(String command, String(*callback)(String , uint8_t*, unsigned int )){
        this->command = command;
        this->callback = callback;
    }
private:
    Command_Handler* _next = NULL;
    String(*callback)(String, uint8_t*, unsigned int);
    String command;
};

class MQTT_CLIENT{
public:
    bool setup_mqtt_client();
    void publish_data(String path, String data);
    void handleConnection();
    void setCommandString(String command, String(*callback)(String, uint8_t*, unsigned int));
    void reset();
    void command_received(char *topic, uint8_t *payload, unsigned int payload_size);
    MQTT_CLIENT(){};
private:
    Command_Handler *_lastHandler, *_firstHandler;
    bool connection_ready=false;
    PubSubClient *mqtt_client=NULL;
    BearSSL::WiFiClientSecure *secure_client=NULL;
    BearSSL::X509List *cert=NULL;
    BearSSL::X509List *client_crt=NULL;
    BearSSL::PrivateKey *key=NULL;
    unsigned long next_connection_test_time = millis();
};

MQTT_CLIENT mqtt_client;

const char *   host = "a200f0zfs8vrnp-ats.iot.ap-southeast-1.amazonaws.com";
const uint16_t  port = 8883;



void MQTT_CLIENT::setCommandString(String command, String(*callback)(String, uint8_t*, unsigned int )){
    Command_Handler *handler = new Command_Handler(command, callback);
    if (!this->_lastHandler) {
      this->_firstHandler = handler;
      this->_lastHandler = handler;
    }
    else {
      this->_lastHandler->next(handler);
      this->_lastHandler = handler;
    }
}

void MQTT_CLIENT::command_received(char* topic, uint8_t* payload, unsigned int payload_size){
    String command = topic;
    String return_data = "";
    Command_Handler *temp = this->_firstHandler;
    while(return_data=="" && temp!=NULL){
        return_data = temp->handle(topic, payload, payload_size);
        temp=temp->next();
    }
    if(return_data==""){
        JSONVar retval;
        retval["error"] = "Command not found";
        return_data=JSON.stringify(retval);
    }
    command = RESPONSE_BASE + command.substring(String(SUBSCRIBE_BASE).length()-1, command.length());
    this->mqtt_client->publish(command.c_str(), return_data.c_str());
}

void MQTT_callback(char* topic, uint8_t* payload, unsigned int payload_size){
    mqtt_client.command_received(topic, payload, payload_size);
}

void MQTT_CLIENT::publish_data(String path, String data){
    if(!this->connection_ready){
        return;
    }
    path = DATA_PUBLISH_BASE + path;
    this->mqtt_client->publish(path.c_str(),data.c_str());
}

void MQTT_CLIENT::handleConnection(){
    if((!this->connection_ready)||(!this->mqtt_client->connected())){
        if(next_connection_test_time>millis()){
            return;
        }
        this->connection_ready = this->setup_mqtt_client();
        next_connection_test_time +=5000;
        if(!this->connection_ready){
            return;
        }
        this->mqtt_client->subscribe(SUBSCRIBE_BASE);
    }
    this->mqtt_client->loop();
}

void MQTT_CLIENT::reset(){
    if(this->mqtt_client==NULL){
        return;
    }
    this->connection_ready = false;
    if(this->mqtt_client->connected()){
        this->mqtt_client->disconnect();
    }
    delete this->key;
    delete this->client_crt;
    delete this->cert;
    delete this->secure_client;
    delete this->mqtt_client;
}


bool MQTT_CLIENT::setup_mqtt_client() {
  static const char ca_cert[] PROGMEM = R"EOF(-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----)EOF";

static const char client_cert[] PROGMEM = R"EOF(-----BEGIN CERTIFICATE-----
MIIDWjCCAkKgAwIBAgIVAMjPxJ+UHXOKZ8/XtJ5iK1rORiIzMA0GCSqGSIb3DQEB
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t
IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yMDAyMDkxNjAy
NTdaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh
dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDv9pvtg9KnyhQsrAWY
/p01UG6J6QHJhN3AObS+Txst962LUIBUo/HWnZWSWj7pdP33Zz6MfhcOnSyRR1kt
BJx61NGgTJXH/ldLwBoaG56L32nS4xj0XzlOVkOn2mPCs+btMD9K00EjpBsMUuAu
fzRIfR5Pd1UPuCHID1nZxiX4YudpAytVZfmwk9UnsJbnYdWNP7dzFkPDOqPzzgVi
XnP0dlruPPHj2V8/cQVyJtA362TAfx16OtCr1HawCiXuBCk7v47VaF0hfmVw24uA
Ja2U/Y8MO+tEIk2CqFOdJO58ODn6Ilad3Jr3RdWTan/9x/PT9iYwu83qJcVxNY5x
DurrAgMBAAGjYDBeMB8GA1UdIwQYMBaAFBAQMAIoU2HeEmE0kF1x/ixQXtaAMB0G
A1UdDgQWBBTBGnPiG7GAqAIB/B0T4SawPEFrkDAMBgNVHRMBAf8EAjAAMA4GA1Ud
DwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAILS2xLjRC/JHAaVnhbF2zIEK
jAYT4Udw1xcqtNTnhz0N0zS2HNLQQuhiV54PauA61jbf1bNKtexOD2Ku55cse2DW
M4ZuOMwuBnjBWCUGhr/lLuThlhwubgVj5XsKIaHLjQabLbZEvIYpA3yET268v7MX
+4HQUmZNLbSNxhqdS6U6I3dsuRkZnEARDmA9BxTwDQNeuw/o8XOSlWxOIOPrVdn4
7YdpwVANqFkjJlUNbnQO1HXCLo6cRL+Wv/cTg+osSPKeeZn6iKxh19QFI7cFx5Y/
pq/aPJnC5MdkHTYT6YuiMII0ijWZKJbnuwwjRI819Dl1DWclu0kAgf5hiuxZ5g==
-----END CERTIFICATE-----)EOF";

static const char client_key[] PROGMEM = R"KEY(-----BEGIN RSA PRIVATE KEY-----
MIIEpQIBAAKCAQEA7/ab7YPSp8oULKwFmP6dNVBuiekByYTdwDm0vk8bLfeti1CA
VKPx1p2Vklo+6XT992c+jH4XDp0skUdZLQScetTRoEyVx/5XS8AaGhuei99p0uMY
9F85TlZDp9pjwrPm7TA/StNBI6QbDFLgLn80SH0eT3dVD7ghyA9Z2cYl+GLnaQMr
VWX5sJPVJ7CW52HVjT+3cxZDwzqj884FYl5z9HZa7jzx49lfP3EFcibQN+tkwH8d
ejrQq9R2sAol7gQpO7+O1WhdIX5lcNuLgCWtlP2PDDvrRCJNgqhTnSTufDg5+iJW
ndya90XVk2p//cfz0/YmMLvN6iXFcTWOcQ7q6wIDAQABAoIBAHQt39ylAC7AhfgC
6Urjq1WOtZYLvBPHQl25Eqs5PZ2J1vomZZuVLJeOAEa1btQ1EmjgEcaPnbYznspP
0vsaynAl7cBAlBwaJkXEol6VlLN/3Yp+7SwTlnk5BtSTxc0UsO+RdnNRyK3q3DWh
Qm0ApV2bRjuPOR29No3X8NahOu0AdZiBXS3g9tnl0COtj/NXzibyXDA57ULuzYe2
fYCXJ4mCfzWQq6XekWY6IvPpWQKKhtJDecx0SFYg6DGuzz41FCbJuOl+fOg/CVvp
ZO7/eg4ljjTG11jGJpAv5UgWr/rHQQZMg7TkuLdS9XncMcFDGqOyHTADtjlMA8nT
pByVmDECgYEA/m1WBNTxt91QlXlNnRDqIf+agOLhoGbNspn4aUcsXx0c7YM2DglC
EbSmKQXLDGPLyptOSxyKbuLqsVSXJYOej+BWHf4kyCgUMmafuzo5TkuBOWYJ1rpB
nuEmwQDGpmZwTj1pyFSv1zPcKeD6fBeLu9rCjrA/IaV4Ua3G2igjNH8CgYEA8XJh
3JaTXJqzO7tg7fyNBXROttdIz/yXquV12Ru5CwqMjpwQzdLcLYmqqO60LwF8Y99/
DJ43f+x/3eVCja3sjxRVSgYMKsrg7q36KjZ+bZpUt6xysDknDqeFbvt5mXrpkPmX
FuIT2of0JQjEw/Cm+ehI5oklFYAGPZydDDmcI5UCgYEA/ZWsLpSvdzq2nsSQfwPk
2I4SSHPZvi24x1J/LS8rIoG522Dz93lyyILtOeX0Qx0UeZPhrSt9LpgsoyJUo6dT
2sMWEj7EGlsYBkQS4GFfzJGk8ripBcQOs3RlU+iaFi/zr4e2b105BZ1CytrZzeUJ
+OpJED7KLZbnHUG++KEYtbECgYEA4C7XZM0+6IYPk4+pMXAEtKLIj4aXad7cKGbE
JKFUEEdsOOH5zFJT88hWeGKjVN3pVIZpXhrt5059b1f2kryB4Fv90SyKUZVsgtFx
bY6Jl1TsAlsRZkS62iV7hI/k2ThB9EV7H1ktHASOEXDx2gjx0Sr3vW5ry2nC4aQw
QfCf5H0CgYEA6d2jOMruYIXae5YpTubFBpYCuqn2uWOnjKd4XEvs0OKEtyLpU0lG
VIBOJ1Mwoq6kzreZgjKvAiWI00Fu74eoVl7md7Mbq+RtuqC1MH6wDUpsaz5pO6yn
sbXQArxlwfbBZh4Bqj0MnWGEJG6jbsdZX6HlLH8M0QW8hzszMyylzpU=
-----END RSA PRIVATE KEY-----)KEY";
    
    this->reset();
    this->secure_client = new BearSSL::WiFiClientSecure();
    this->cert = new BearSSL::X509List(ca_cert);
    this->secure_client->setTrustAnchors(this->cert);
    this->client_crt = new BearSSL::X509List(client_cert);
    this->key= new BearSSL::PrivateKey(client_key);
    this->secure_client->setClientRSACert(this->client_crt, this->key);
    this->mqtt_client = new PubSubClient(*(this->secure_client));
    this->mqtt_client->setCallback(MQTT_callback);
    this->mqtt_client->setServer(host, port);
    return this->mqtt_client->connect(STR(TARGET_DEVICE_ID));
}

String test_callback(String topic, uint8_t* payload, unsigned int payload_sie){
    String data="";
    for(int i=0;i<payload_sie;i++){
        data+=(char) payload[i];
    }
    return data;
}


void initiate_mqtt_client(){
  configTime(5.5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  mqtt_client.setCommandString("reflect",test_callback);
}


float Temperature,Humidity;
bool TemperatureValueIsValid=false;
bool HumidityValueIsValid=false;
long NextMeasurementTime;

uint32_t delayMS = 20000;


String get_sensor_data(String topic, uint8_t* payload, unsigned int payload_size){
  sensors_event_t event;
  JSONVar data;
  dht.temperature().getEvent(&event);
  if(isnan(event.temperature)){
    TemperatureValueIsValid=false;
  }else{
    TemperatureValueIsValid=true;
    Temperature=event.temperature;
  }
  dht.humidity().getEvent(&event);
  if(isnan(event.relative_humidity)){
    HumidityValueIsValid=false;
  }else{
    HumidityValueIsValid=true;
    Humidity=event.relative_humidity;
  }
  data["Temperature"] = Temperature;
  data["Humidity"] = Humidity;
  data["ValidTemp"] = TemperatureValueIsValid;
  data["ValidHumidity"] = HumidityValueIsValid;
  time_t now = time(nullptr);
  data["time_stamp"] = String(ctime(&now));
  data["success"] = true;
    return JSON.stringify(data);
}

String update_data_rate(String topic, uint8_t* payload, unsigned int payload_size){
    String jsonData = "";
    for(int i=0;i<payload_size;i++){
        jsonData+=(char)payload[i];
    }
    JSONVar jsonObject = JSON.parse(jsonData);
    JSONVar returnData;
    if(!jsonObject.hasOwnProperty("delay")){
        returnData["success"] = false;
        returnData["message"] = "delay value is not provided";
        return JSON.stringify(returnData);
    }
    delayMS = (int)jsonObject["delay"];
    returnData["success"] = true;
    returnData["delay"] = (int)delayMS;
    NextMeasurementTime = millis() + delayMS;
    return JSON.stringify(returnData);
    
}


void setup(){
    WiFi.begin("Nitro_net", "bolt9959");
    
  dht.begin();
  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  dht.humidity().getEvent(&event);
  NextMeasurementTime=millis()+delayMS;
    mqtt_client.setCommandString("getData", get_sensor_data);
    mqtt_client.setCommandString("updateDataRate", update_data_rate);
    initiate_mqtt_client();
}

void loop(){
    mqtt_client.handleConnection();
  if(NextMeasurementTime>millis()){
    return;
  }
  NextMeasurementTime +=delayMS;
  String data = get_sensor_data("", NULL, 0);
  mqtt_client.publish_data("data", data);
}