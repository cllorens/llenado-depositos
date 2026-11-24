#include <PubSubClient.h>  

void Light_discover(PubSubClient *client, String light_name, bool light_status, String MQTTBase);
void Cover_discover(PubSubClient *client, String cover_name, uint8_t cover_position, String MQTTBase);
void Push_button_discover(PubSubClient *client, int button_number, bool button_status, String MQTTBase, String MQTT_sensor_path, String base_name);

void EnergySensor_discover(PubSubClient *client, String MQTTBaseString, String sensor_name, String Units, int increasing, String device_name, String device_model, String Manufacturer, String device_ID, String icon, int device_class_energy);
void EnergySensor_publish_string(PubSubClient *client, String MQTTBaseString, String Value);
void EnergySensor_publish_float(PubSubClient *client, String MQTTBaseString, float Value);

void HASensor_discover(PubSubClient *client, String MQTTBaseString, String sensor_name, String Units, String state_class, String device_class, String device_name, String device_model, String Manufacturer, String device_ID, String icon, int expire_after = 0, String entity_category = "");
void Number_discover(PubSubClient *client, String MQTTBaseString, String name, float min, float max, float step, String Units, String device_name, String device_model, String Manufacturer, String device_ID, String icon, String entity_category = "config");
void Button_discover(PubSubClient *client, String MQTTBaseString, String name, String device_name, String device_model, String Manufacturer, String device_ID, String icon, String entity_category = "config");
void HASensor_publish_string(PubSubClient *client, String MQTTBaseString, String Value);
void HASensor_publish_float(PubSubClient *client, String MQTTBaseString, float Value);

void TextEntity_discover(PubSubClient *client, String MQTTBaseString, String object_id, String name, String device_name, String device_model, String Manufacturer, String device_ID, String icon);
void TextEntity_publish(PubSubClient *client, String MQTTBaseString, String Value);
void Switch_discover(PubSubClient *client, String MQTTBaseString, String switch_name, bool switch_status, String device_name, String device_model, String Manufacturer, String device_ID, String icon, bool retain_cmd = false);
void Switch_publish_state(PubSubClient *client, String MQTTBaseString, bool switch_status);

