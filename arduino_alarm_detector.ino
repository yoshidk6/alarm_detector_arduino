#include "arduino_secrets.h"
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PDM.h>

// WiFi settings
WiFiClient wifiClient;
char ssid[] = SECRET_SSID;      
char pass[] = SECRET_WIFI_PASS;
int status = WL_IDLE_STATUS;

// NTP settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -28800, 60000); // GMT-8 (US Pacific Standard time)

// Audio FFT settings
const int samples = 256; // Number of samples for the FFT
const int samplingFrequency = 14000; // Sampling frequency
short sampleBuffer[64];
short streamBuffer[samples]; // Sample stream buffer
int approxBuffer[samples]; // ApproxFFT sample buffer
volatile int samplesRead = 0;
volatile int sampleCount = 0;

int frequencyOfInterest = 2050; // Frequency to detect (2050Hz)
int amplitudeAtFrequency = 0;
int indexFreqSample = frequencyOfInterest * samples / samplingFrequency;

// Variables for max peak volume calculation
unsigned long startTime = 0;
const unsigned long extractionInterval = 5000000; // 5 seconds in microseconds
int maxAmplitudeDuringInterval = 0;

// Variables for alarm duration calculation
int amplitudeThreshold = 5000;
int limitTime = 1000000; // Silence time to determine the end of the sound (in microseconds)
int high_total_time_output;
int total_time_output;
int volume_at_freq;

// Pushover settings
const char* pushoverAPI = "api.pushover.net";
const char* userKey = SECRET_PUSHOVER_USER_KEY;
const char* apiToken = SECRET_PUSHOVER_API_KEY;
const char* messageTitle = "Bread is ready!";

// Function declarations
void setup();
void loop();
void onPDMdata();
void processContSignal();
bool sendNotification();

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  delay(1500);

  // Initialize WiFi
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {}

  // Initialize NTP client
  timeClient.begin();
  timeClient.update();

  // Initialize PDM (microphone)
  PDM.onReceive(onPDMdata);
  PDM.setGain(20);
  if (!PDM.begin(1, samplingFrequency)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }
}

void loop() {
  // Accumulate samples for FFT
  if (samplesRead) {
    for (int i = 0; i < samplesRead; i++) {
      streamBuffer[i + sampleCount] = sampleBuffer[i];
      approxBuffer[i + sampleCount] = sampleBuffer[i] / 2;
    }
    sampleCount += samplesRead;
    samplesRead = 0;
  }

  // Perform FFT when enough samples are accumulated
  if (sampleCount >= samples) {
    Approx_FFT(approxBuffer, samples, samplingFrequency);
    amplitudeAtFrequency = approxBuffer[indexFreqSample];

    sampleCount = 0;

    // Extract maximum amplitude
    if (amplitudeAtFrequency > maxAmplitudeDuringInterval) {
      maxAmplitudeDuringInterval = amplitudeAtFrequency;
    }

    // Check if 5 seconds have elapsed
    if (micros() - startTime >= extractionInterval) {
      Serial.print("Maximum amplitude during the last 5 seconds: ");
      Serial.println(maxAmplitudeDuringInterval);

      volume_at_freq = maxAmplitudeDuringInterval;

      maxAmplitudeDuringInterval = 0;
      startTime = micros();
    }

    // Process continuous signal
    processContSignal();
  }
}

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

// Process continuous signal duration
int totalTime = 0;
int highTotalTime = 0;
int lowTime;
int firstRisePoint;
int risePoint;
int fallPoint;
bool isPrevSignalHigh = false;
int countSignalHigh = 0;

void processContSignal() {
  unsigned long currentTime = micros();

  // Check if the amplitude is above the threshold
  if (amplitudeAtFrequency > amplitudeThreshold) {
    if (!isPrevSignalHigh) {
      // Moment when the signal goes HIGH (rising edge)
      isPrevSignalHigh = true;
      risePoint = currentTime;
      countSignalHigh++;
      firstRisePoint = (countSignalHigh == 1) ? currentTime : firstRisePoint;
      Serial.println("Rising edge");
    }
  } else {
    if (isPrevSignalHigh) {
      // Moment when the signal goes LOW (falling edge)
      isPrevSignalHigh = false;
      fallPoint = currentTime;
      highTotalTime += currentTime - risePoint;
      Serial.print("Falling edge: current highTotalTime of ");
      Serial.print(highTotalTime / 1000);
      Serial.println(" ms");
    } else {
      lowTime = currentTime - fallPoint;

      // Check if the signal has been low for a sufficient duration
      if (countSignalHigh > 0 && lowTime > limitTime) {
        totalTime = currentTime - firstRisePoint - lowTime;
        Serial.println("End of alert:");
        Serial.print("current highTotalTime of ");
        Serial.print(highTotalTime / 1000);
        Serial.println(" ms");
        Serial.print("current totalTime of ");
        Serial.print(totalTime / 1000);
        Serial.println(" ms");

        // Check if the total time and high total time meet the criteria
        if (totalTime > 300000) {
          Serial.print(timeClient.getFormattedTime());
          Serial.print(" - ");
          Serial.print(totalTime / 1000);
          Serial.print("[");
          Serial.print(highTotalTime / 1000);
          Serial.println("]");

          total_time_output = totalTime / 1000;
          high_total_time_output = highTotalTime / 1000;

          // // Check specific duration criteria before sending notification
          // if (totalTime > 500000) {
          //   sendNotification(); // Test code & learn the duration of alarm
          // }
          if (totalTime     > 7000000 && totalTime     < 8000000 && 
              highTotalTime > 2000000 && highTotalTime < 3000000) {
            sendNotification();
          }
        }

        // Reset variables for the next interval
        totalTime = 0;
        highTotalTime = 0;
        countSignalHigh = 0;
      }
    }
  }
}

// Send notification using Pushover API
bool sendNotification() {
  Serial.println("Starting notification process...");

  // Attempt to connect to the Pushover API server with SSL
  if (!wifiClient.connectSSL("api.pushover.net", 443)) {
    Serial.println("Failed to connect to Pushover API server!");
    return false;
  }
 
  Serial.println("Connected to Pushover API server.");

  // Construct the HTTP POST payload
  String postData = "token=" + String(apiToken) +
                    "&user=" + String(userKey) +
                    "&title=" + String(messageTitle) +
                    "&message=" + String(timeClient.getFormattedTime()) +
                    "- Total time: " + String(totalTime) + 
                    "- High total time: " + String(highTotalTime);
 
  // Construct and send the HTTP POST request
  wifiClient.println("POST /1/messages.json HTTP/1.1");
  wifiClient.println("Host: api.pushover.net");
  wifiClient.println("Connection: close");
  wifiClient.println("Content-Type: application/x-www-form-urlencoded");
  wifiClient.print("Content-Length: ");
  wifiClient.println(postData.length());
  wifiClient.println();
  wifiClient.print(postData);

  Serial.println("POST request sent. Waiting for response...");

  // Wait for the response from the server
  unsigned long timeout = millis();
  while (wifiClient.connected() && !wifiClient.available()) {
    if (millis() - timeout > 5000) { // 5-second timeout
      Serial.println("Timeout waiting for server response!");
      wifiClient.stop();
      return false;
    }
  }

  // Read the response from the server
  String response = "";
  while (wifiClient.available()) {
    response += wifiClient.readStringUntil('\n');
  }

  // Debugging: Print the response
  Serial.println("Response received:");
  Serial.println(response);

  // Close the connection
  wifiClient.stop();

  // Check for success in the response
  if (response.indexOf("\"status\":1") > -1) {
    Serial.println("Notification sent successfully!");
    return true;
  } else {
    Serial.println("Failed to send notification. Check the API response.");
    return false;
  }
}
