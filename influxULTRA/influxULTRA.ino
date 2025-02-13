  #include <InfluxDbClient.h>
  #include <InfluxDbCloud.h>
  #include <esp_now.h>
  #include <WiFi.h>
  #include <esp_wifi.h>
  #include <cstring>

  #define DEVICE "ESP32"

  typedef struct struct_message {
    char text[32];
  } struct_message;

  struct_message incomingMessage;

  const int trigPin = 5;
  const int echoPin = 18;

  const int plasticLed = 19;
  const int paperLed = 21;
  const int otherLed = 23;

  long duration;
  float distanceCm;
  +
  const char* ssid = "Uwu";
  const char* pass = "orldhello";
  char Waste_type[8];
  
  #define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"
  #define INFLUXDB_TOKEN  "Kn7SDJ8Ai0nEFqovtToZ5v4v-Q5NEVTNNtCnFjeOtaBUxLm2U43LuVpS1DYJIQulJQzBU9lWZ7cDov1eGDDEoQ=="
  #define INFLUXDB_ORG "Smartbin"
  #define INFLUXDB_BUCKET "Cashcrow"
    int wifiChannel = WiFi.channel();
    Serial.printf("Wi-Fi Channel: %d\n", wifiChannel);
    esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
  
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }
    Serial.println();
    Serial.printf("Connected to WiFi: %s\n", ssid);
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.println();

    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    // Register receive callback
    esp_now_register_recv_cb(onReceive);
  
    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
  
    // Check server connection
    if (client.validateConnection()) {
      Serial.print("Connected to InfluxDB: ");
      Serial.println(client.getServerUrl());
    } else {
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
    }
    //sensor.addTag("device", DEVICE);
    sensor.addTag("Bin_id", "123");
  }
 void loop() {
  delay(10);

    if (recv) {
      
      recv = false; // Reset the flag
      int messageAsInt = atoi(incomingMessage.text);
      if(messageAsInt == 0){
          strcpy(Waste_type,"Plastic");
          Serial.println("Plastic");
          digitalWrite(plasticLed, HIGH);
          delay(2000);
          digitalWrite(plasticLed, LOW);

      }
      else if(messageAsInt == 1){
        strcpy(Waste_type,"Paper");
        Serial.println("Paper");
        digitalWrite(paperLed, HIGH);
        delay(2000);
        digitalWrite(paperLed, LOW);
      }
      else if(messageAsInt == 2){
        strcpy(Waste_type,"Other");
        Serial.println("Other");
        digitalWrite(otherLed, HIGH);
        delay(2000);
        digitalWrite(otherLed, LOW);
      }
      task();       // Perform the distance measurement and upload data
  }
}

void task(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = (duration * 0.034)/2;
  Serial.print("Distance (cm): ");
  Serial.println(distanceCm);
    
    sensor.clearFields();
  
    // Store measured value into point
    // Report RSSI of currently connected network
    sensor.addField("Fill_level", (static_cast<float>(distanceCm)/30)*100);
    sensor.addField("Waste_type", Waste_type);
  
    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());

  
    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
    else{
      Serial.println("DB updated with fill level");
    }    
}

void onReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int dataLen) {
      recv = true;
      memcpy(&incomingMessage, data, sizeof(incomingMessage));
      Serial.print("Message Received from MAC: ");
      for (int i = 0; i < 6; ++i) {
          Serial.printf("%02X", recvInfo->src_addr[i]);
          if (i < 5) Serial.print(":");
        }
      Serial.println();
 
      Serial.print("Bin: ");
      Serial.println(incomingMessage.text);
  }
