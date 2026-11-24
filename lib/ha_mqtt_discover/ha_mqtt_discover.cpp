#include <PubSubClient.h>  
#include <ArduinoJson.h>

#define DEBUG 0
#define manufacturer "charlie"

/*******************************************************/
// send Home assistant Light discover and subscription
// client: MQTT conection
// light_name: ligth name
// light_status: 1=light is on, 0=light is off
// MQTTBase: MQTT base address
/*******************************************************/
void Light_discover(PubSubClient *client, String light_name, bool light_status, String MQTTBase)
  {
  // JSON Object
  JsonDocument discover;
  
  // Device parameters
  discover["device"]["manufacturer"]=manufacturer;
  discover["device"]["model"]="Custom IOT ligth";
  
  // Discover common parameters
  discover["cmd_t"]="~/set";
  discover["stat_t"]="~/state";
  discover["schema"]="json";
  discover["brightness"]="false";

  // Add specific parameters to JSON
  discover["~"]=MQTTBase+"/light/"+light_name;
  discover["unique_id"]=light_name+"_ligth";
  discover["device"]["identifiers"]=light_name+"_ligth";
  discover["name"]=light_name;

  // Generate topic and pyload strins
  String payload;
  String topic=MQTTBase+"/light/"+light_name+"/config";
  serializeJson(discover, payload); 

  // Send self discover message
  client->publish(topic.c_str(), payload.c_str(),true);
  if (DEBUG) Serial.println("config: " + topic + " - " + payload);

  // Subscribe to set messages
  topic=MQTTBase+"/light/"+light_name+"/set";
  client->subscribe(topic.c_str());
  
  // Define topic for ligths
  String topic_ligth=MQTTBase+"/light/"+light_name+"/state";

  // Publish current lights status
  if (light_status) client->publish(topic_ligth.c_str(), "{\"state\": \"ON\"}");
  else client->publish(topic_ligth.c_str(), "{\"state\": \"OFF\"}");
  }

/*******************************************************/
// send Home assistant Cover discover and subscription
// client: MQTT conection
// cover_name: cover name
// cover_position: actual cover position
// MQTTBase: MQTT base address
/*******************************************************/
void Cover_discover(PubSubClient *client, String cover_name, uint8_t cover_position, String MQTTBase)
  {
  // JSON Object
  JsonDocument discover;

  // Device parameters
  discover["device"]["manufacturer"]=manufacturer;
  discover["device"]["model"]="Custom IOT cover";
  
  // Discover common parameters
  discover["set_position_topic"]="~/set_position_topic";
  discover["position_topic"]="~/position_topic";

  // Add specific parameters to JSON
  discover["~"]=MQTTBase+"/cover/"+cover_name;
  discover["unique_id"]=cover_name+"_cover";
  discover["device"]["identifiers"]=cover_name+"_cover";
  discover["name"]=cover_name;
  
  // Generate topic and pyload strins
  String payload;
  String topic=MQTTBase+"/cover/"+cover_name+"/config";
  serializeJson(discover, payload); 
  
  // Publish config message
  client->publish(topic.c_str(), payload.c_str(),true);
  if (DEBUG) Serial.println("config: " + topic + " - " + payload);
  
  // Subscribe to set messages
  topic=MQTTBase+"/cover/"+cover_name+"/set_position_topic";
  client->subscribe(topic.c_str());

  // define topic for cover position
  String topic_cover=MQTTBase+"/cover/"+cover_name+"/position_topic";
  
  char pos[10];
  client->publish(topic.c_str(), itoa(cover_position,pos,10));
  }
  
/*******************************************************/
// send Home assistant push button discover and subscription
// client: MQTT conection
// button_number: push button number
// button_status: 1=push button pressed , 0=push button released
// MQTTBase: MQTT base address
// MQTT_sensor_path: MQTT base sensor path
// base_name: push button base name
/*******************************************************/
void Push_button_discover(PubSubClient *client, int button_number, bool button_status, String MQTTBase, String MQTT_button_path, String base_name)
  {
  // JSON Object
  JsonDocument discover;

  // Device parameters
  discover["device"]["manufacturer"]=manufacturer;
  discover["device"]["model"]="Custom IOT ligth";
  
  // Discover parameters
  discover["icon"]="mdi:electric-switch";
  discover["state_topic"]="~/state";
  discover["~"]=MQTTBase+"/push_button/"+MQTT_button_path+String(button_number);
  discover["name"]=base_name+String(button_number);
  discover["unique_id"]=base_name+String(button_number);
  
  String topic=MQTTBase+"/push_button/"+MQTT_button_path+String(button_number)+"/config";
  String payload="";
  serializeJson(discover, payload); 
    
  // Publish config message
  client->publish(topic.c_str(), payload.c_str(),true);
  if (DEBUG) Serial.println("config: " + topic + " - " + payload);

  // Define topic for inputs
  String topic_button=MQTTBase+"/push_button/"+MQTT_button_path+String(button_number)+"/state";
        
  // Publish current push buttons status
  if (button_status) client->publish(topic_button.c_str(), "ON");
  else client->publish(topic_button.c_str(), "OFF"); 
  }

/*******************************************************/
// send Home assistant Energy sensor discover and subscription
// client: MQTT conection
// MQTTBaseString: MQTT Base for the sensor
// sensor_name: name for the sensor
// Units: measuring units
// increasing: total_increasing data
// device_name: device name for the sensor
// device_model: device model for the sensor
// Manufacturer: manufacturer of the sensor
// device_ID: unique device id
// icon: mdi icon
// device_class_energy: is energy class device? boolean true or false
/*******************************************************/
void EnergySensor_discover(PubSubClient *client, String MQTTBaseString, String sensor_name, String Units, int increasing, String device_name, String device_model, String Manufacturer, String device_ID, String icon, int device_class_energy)
  {
  // JSON Object
  JsonDocument discover;

  // Device parameters
  discover["device"]["identifiers"]=device_ID;
  discover["device"]["name"]=device_name;
  discover["device"]["model"]=device_model;
  discover["device"]["manufacturer"]=Manufacturer;

  // Energy Sensor parameters
  if (device_class_energy) discover["device_class"]="energy";
  if (increasing) discover["state_class"]="total_increasing";
  else discover["state_class"]="total";
  discover["icon"]=icon;
  discover["state_topic"]="~/state";
  discover["unit_of_measurement"]=Units;
  discover["~"]=MQTTBaseString;
  discover["name"]=sensor_name;
  discover["unique_id"]=device_ID+sensor_name;
  
  String topic=MQTTBaseString+"/config";
  String payload="";
  serializeJson(discover, payload); 
    
  // Publish config message
  client->publish(topic.c_str(), payload.c_str(),true);
  if (DEBUG) Serial.println("config: " + topic + " - " + payload);  
  }

void EnergySensor_publish_string(PubSubClient *client, String MQTTBaseString, String Value)
  {
  String topic=MQTTBaseString+"/state";  

  // Publish config message
  client->publish(topic.c_str(), Value.c_str(),true);
  if (DEBUG) Serial.println("config: " + topic + " - " + Value);  
  }

void EnergySensor_publish_float(PubSubClient *client, String MQTTBaseString, float Value)
  {
  String topic=MQTTBaseString+"/state";  
  char payload[20];
  sprintf(payload,"%.1f",Value);

  // Publish config message
  client->publish(topic.c_str(), payload,true);
  if (DEBUG) Serial.println("config: " + topic + " - " + String(payload));  
  }

/*******************************************************/
// send Home Assistant sensor discovery message
// client: MQTT connection
// MQTTBaseString: base topic for the sensor (e.g. homeassistant/sensor/device/sensor_name)
// sensor_name: name shown in Home Assistant
// Units: unit of measurement (e.g. "W", "kWh", "°C")
// state_class: "measurement", "total", "total_increasing"
// device_class: HA device class (e.g. "power", "energy", "temperature") or "" for none
// device_name: device name shown in HA
// device_model: hardware model
// Manufacturer: manufacturer name
// device_ID: unique device id (e.g. MAC or UID)
// icon: mdi icon string (e.g. "mdi:power-plug")
/*******************************************************/
// expire_after: segundos sin datos tras los que HA marca el sensor como no disponible (0 = nunca)
void HASensor_discover(PubSubClient *client, String MQTTBaseString, String sensor_name, String Units, String state_class, String device_class, String device_name, String device_model, String Manufacturer, String device_ID, String icon, int expire_after, String entity_category)
{
  JsonDocument discover;
  if (entity_category != "") discover["entity_category"] = entity_category;   // "diagnostic" / "config"

  // Device information
  discover["device"]["identifiers"] = device_ID;
  discover["device"]["name"] = device_name;
  discover["device"]["model"] = device_model;
  discover["device"]["manufacturer"] = Manufacturer;

  // Sensor information
  if (device_class != "") discover["device_class"] = device_class;
  if (state_class != "") discover["state_class"] = state_class;

  discover["icon"] = icon;
  discover["state_topic"] = "~/state";
  // Sin unidad (sensores de texto) no se manda: HA lo trataria como numerico
  if (Units != "") discover["unit_of_measurement"] = Units;
  if (expire_after > 0) discover["expire_after"] = expire_after;
  discover["~"] = MQTTBaseString;
  discover["name"] = sensor_name;
  // Reemplazar espacios por guiones bajos para el unique_id
  String sensor_id = sensor_name;
  sensor_id.replace(" ", "_");

  discover["unique_id"] = device_ID + "_" + sensor_id;

  String topic = MQTTBaseString + "/config";
  String payload;
  serializeJson(discover, payload);

  client->publish(topic.c_str(), payload.c_str(), true);
  if (DEBUG) Serial.println("config: " + topic + " - " + payload);
}

/*******************************************************/
// Home Assistant MQTT Discovery: numero editable (https://www.home-assistant.io/integrations/number.mqtt/)
// Ordenes en ~/set (con retain: el dispositivo puede estar dormido), estado en ~/state.
/*******************************************************/
void Number_discover(PubSubClient *client, String MQTTBaseString, String name, float min, float max, float step, String Units, String device_name, String device_model, String Manufacturer, String device_ID, String icon, String entity_category)
{
  JsonDocument discover;

  discover["device"]["identifiers"] = device_ID;
  discover["device"]["name"] = device_name;
  discover["device"]["model"] = device_model;
  discover["device"]["manufacturer"] = Manufacturer;

  discover["~"] = MQTTBaseString;
  discover["name"] = name;
  String id = name; id.replace(" ", "_");
  discover["unique_id"] = device_ID + "_" + id;
  discover["cmd_t"] = "~/set";
  discover["stat_t"] = "~/state";
  discover["min"] = min;
  discover["max"] = max;
  discover["step"] = step;
  discover["mode"] = "box";
  discover["retain"] = true;
  if (Units != "") discover["unit_of_measurement"] = Units;
  if (entity_category != "") discover["entity_category"] = entity_category;
  discover["icon"] = icon;

  String topic = MQTTBaseString + "/config";
  String payload;
  serializeJson(discover, payload);
  client->publish(topic.c_str(), payload.c_str(), true);
  if (DEBUG) Serial.println("config: " + topic + " - " + payload);
}

/*******************************************************/
// Home Assistant MQTT Discovery: pulsador (https://www.home-assistant.io/integrations/button.mqtt/)
// Publica "PRESS" en ~/set con retain; el dispositivo, al atenderlo, borra el retenido.
/*******************************************************/
void Button_discover(PubSubClient *client, String MQTTBaseString, String name, String device_name, String device_model, String Manufacturer, String device_ID, String icon, String entity_category)
{
  JsonDocument discover;

  discover["device"]["identifiers"] = device_ID;
  discover["device"]["name"] = device_name;
  discover["device"]["model"] = device_model;
  discover["device"]["manufacturer"] = Manufacturer;

  discover["~"] = MQTTBaseString;
  discover["name"] = name;
  String id = name; id.replace(" ", "_");
  discover["unique_id"] = device_ID + "_" + id;
  discover["cmd_t"] = "~/set";
  discover["payload_press"] = "PRESS";
  discover["retain"] = true;
  if (entity_category != "") discover["entity_category"] = entity_category;
  discover["icon"] = icon;

  String topic = MQTTBaseString + "/config";
  String payload;
  serializeJson(discover, payload);
  client->publish(topic.c_str(), payload.c_str(), true);
  if (DEBUG) Serial.println("config: " + topic + " - " + payload);
}

void HASensor_publish_string(PubSubClient *client, String MQTTBaseString, String Value)
{
  String topic = MQTTBaseString + "/state";
  client->publish(topic.c_str(), Value.c_str(), true);
  if (DEBUG) Serial.println("publish: " + topic + " - " + Value);
}

void HASensor_publish_float(PubSubClient *client, String MQTTBaseString, float Value)
{
  String topic = MQTTBaseString + "/state";
  char payload[20];
  sprintf(payload, "%.2f", Value);
  client->publish(topic.c_str(), payload, true);
  if (DEBUG) Serial.println("publish: " + topic + " - " + String(payload));
}

/*******************************************************/
// Home Assistant MQTT Discovery for text entities
// https://www.home-assistant.io/integrations/text.mqtt/
/*******************************************************/
void TextEntity_discover(PubSubClient *client, String MQTTBaseString, String object_id, String name, String device_name, String device_model, String Manufacturer, String device_ID, String icon)
{
  JsonDocument discover;

  // Información del dispositivo
  discover["device"]["identifiers"] = device_ID;
  discover["device"]["name"] = device_name;
  discover["device"]["model"] = device_model;
  discover["device"]["manufacturer"] = Manufacturer;

  // Configuración de la entidad de texto
  discover["~"] = MQTTBaseString;
  discover["name"] = name;
  discover["mode"] = "text";
  discover["command_topic"] = "txt/cmd";
  discover["state_topic"] = "txt/state";
  discover["unique_id"] = device_ID + "_" + object_id;
  discover["object_id"] = object_id;
  discover["state_topic"] = "~/state";
  discover["icon"] = icon;
  discover["platform"] = "mqtt";

  // Tópico de configuración
  String topic=MQTTBaseString+"/config";
  String payload = "";
  serializeJson(discover, payload);

  client->publish(topic.c_str(), payload.c_str(), true);
  if (DEBUG) Serial.println("TextEntity discover: " + topic + " - " + payload);
}

void TextEntity_publish(PubSubClient *client, String MQTTBaseString, String Value)
{
  String topic = MQTTBaseString + "/state";
  client->publish(topic.c_str(), Value.c_str(), true);
  if (DEBUG) Serial.println("TextEntity publish: " + topic + " - " + Value);
}

/*******************************************************/
// Envía discovery de Home Assistant para un interruptor
// client: conexión MQTT
// MQTTBaseString: base del topic MQTT
// switch_name: nombre del interruptor
// switch_status: estado inicial (true = ON)
// device_name: nombre amigable del dispositivo
// device_model: modelo del dispositivo
// Manufacturer: fabricante del dispositivo
// device_ID: ID único del dispositivo
// icon: ícono MDI para mostrar en la UI
/*******************************************************/
// retain_cmd: HA publica las ordenes con retain (para dispositivos que duermen y las leen al despertar)
void Switch_discover(PubSubClient *client, String MQTTBaseString, String switch_name, bool switch_status, String device_name, String device_model, String Manufacturer, String device_ID, String icon, bool retain_cmd)
{
  // JSON de configuración
  JsonDocument discover;

  // Información del dispositivo
  discover["device"]["identifiers"] = device_ID;
  discover["device"]["name"] = device_name;
  discover["device"]["model"] = device_model;
  discover["device"]["manufacturer"] = Manufacturer;

  // Parámetros del interruptor
  discover["~"] = MQTTBaseString;
  discover["name"] = switch_name;
  discover["unique_id"] = device_ID + "_" + switch_name;
  discover["cmd_t"] = "~/set";
  discover["stat_t"] = "~/state";
  discover["icon"] = icon;
  if (retain_cmd) discover["retain"] = true;

  // Construcción del topic de configuración
  String topic = MQTTBaseString + "/config";
  String payload;
  serializeJson(discover, payload);

  // Publicación del mensaje de autodiscovery
  client->publish(topic.c_str(), payload.c_str(), true);

  if (DEBUG) Serial.println("config: " + topic + " - " + payload);

  // Suscribirse al control del interruptor
  String topic_set = MQTTBaseString + "/set";
  client->subscribe(topic_set.c_str());

  // Publicar el estado actual del interruptor
  String topic_state = MQTTBaseString + "/state";
  client->publish(topic_state.c_str(), switch_status ? "ON" : "OFF", true);
}

/*******************************************************/
// Publica el estado actual de un interruptor en MQTT
// client: conexión MQTT
// MQTTBaseString: base del topic MQTT usado en el discovery
// switch_status: true = ON, false = OFF
/*******************************************************/
void Switch_publish_state(PubSubClient *client, String MQTTBaseString, bool switch_status)
{
  String topic_state = MQTTBaseString + "/state";
  const char* payload = switch_status ? "ON" : "OFF";

  client->publish(topic_state.c_str(), payload, true);  // con retain
  if (DEBUG) Serial.println("state: " + topic_state + " - " + payload);
}