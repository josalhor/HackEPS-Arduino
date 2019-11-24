#include <math.h>

#include <DHT.h>
#include <SigFox.h>

// Definimos el pin digital donde se conecta el sensor
#define DHTPIN 2
// Dependiendo del tipo de sensor

/* Offset encoding */
#define OFFSET_ENCODING 27

#define DHTTYPE DHT11
/* 12 Bytes per message as a requirement of SigFox */
#define BYTES_PER_MSG 12
/* One read takes 3 bytes */
#define BYTES_PER_LOG 3
/* ReaDs per Packet as ratio */
#define READS_PER_PACKET (BYTES_PER_MSG / BYTES_PER_LOG)

/* Send packet every 10 minutes */
#define WAIT_READ_GROUP_MS (1000 * 60 * 3)
/* Read Sensor every 2 seconds */
#define READ_EVERY_MS (1000 * 2)
/* Number Readings per log is the ratio of number of Total Wait time by
 * Reads per packet and reads per second
*/
#define NUMBER_READS ((WAIT_READ_GROUP_MS / READS_PER_PACKET) / READ_EVERY_MS)

/* Ignore percent algorithm */
#define IGNORE_PERCENT (5/100)
/* Ignore percent for double side algorithm */
#define IGNORE_PERCENT_SIDE (IGNORE_PERCENT / 2)
/* Lower index of reading after adjusting */
#define LOWER_BOUND_INFO (((int)(IGNORE_PERCENT_SIDE * NUMBER_READS)) + 1)
/* Higher index of reading after adjusting */
#define UPPER_BOUND_INFO ((int)((1 - IGNORE_PERCENT_SIDE) * NUMBER_READS))
/* Number of reads taken into account */
#define NUMBER_BOUND (UPPER_BOUND_INFO - LOWER_BOUND_INFO)

// Inicializamos el sensor DHT11
DHT dht(DHTPIN, DHTTYPE);


/*
 * Print Basic Info about SigFox and
 * start serial 9600
 */
void setup() {
  Serial.begin(9600);
  if (!SigFox.begin()) { //ensures SigFox is connected
    Serial.println("Shield error or not present!");
    return;
  }

  String version = SigFox.SigVersion();
  String ID = SigFox.ID();
  String PAC = SigFox.PAC();  Serial.println("ID  = " + ID);
  Serial.println("PAC = " + PAC);
  
  SigFox.end();
  SigFox.debug();
  dht.begin();
}

/*
 * Adjust int to byte format
 */

byte adjust_format(int input) {
  return (byte)(input + OFFSET_ENCODING); 
}

/*
 * Send Message with SigFox
 */

void send_message(byte message[12]) {
      
  SigFox.begin();
  delay(100);  // Wait 100ms for the begin
  SigFox.status();
  delay(1);

  // Start packet
  SigFox.beginPacket();

  // Write buffer
  SigFox.write(message, 12);

  // Send data
  int result = SigFox.endPacket();
  
  
  // Check result
  if(result == 0){
    Serial.println("Message sent.");
  }else{
    Serial.println("Error sending message.");
  }

  // Shutdown module
  SigFox.end();
}

/*
 * Comparator for sort
 */

int compare( const void* a, const void* b)
{
 float f_a = * ( (float*) a );
 float f_b = * ( (float*) b );

 if (f_a == f_b) {
  return 0;
 } else if (f_a < f_b) {
  return -1;
 } else {
  return 1;
 }
}

/*
 * Gets the mean from a percent adjusted algorithm
 */

float mean_array_adjusted(float to_calculate[NUMBER_READS]){
  qsort(to_calculate, NUMBER_READS, sizeof(float), compare);
  
  float medium = to_calculate[LOWER_BOUND_INFO];
  for(int i = LOWER_BOUND_INFO + 1; i < UPPER_BOUND_INFO; i++){
      medium += to_calculate[i];
  }
  return medium / NUMBER_BOUND;
}

void loop() {
  byte packet[BYTES_PER_MSG];
  /*
   * For every packet that has to be read
   */
  for(int index_packet = 0; index_packet < READS_PER_PACKET; index_packet++){
    /*
     * For every log that has to be made for the log of the packet
     */
    float temperatures[NUMBER_READS];
    float moistures[NUMBER_READS];
    
    for(int index_read = 0; index_read < NUMBER_READS; index_read++){
      /*
       * Read Value and log
       */
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      if (isnan(h) || isnan(t)) {
        Serial.println("Error getting data from DHT11, restart cycle");
        return;
      }
      
      float hic = dht.computeHeatIndex(t, h, false);
      temperatures[index_read] = hic;
      moistures[index_read] = h;
      Serial.print("Temp: ");
      Serial.println(hic);

      Serial.print("Hum: ");
      Serial.println(h);
      
      delay(READ_EVERY_MS); 
    }

    /*
     * Get Means Adjusted
     */

    float mean_temp = mean_array_adjusted(temperatures);
    float mean_hum = mean_array_adjusted(moistures);

    Serial.print("Mean Temp: ");
    Serial.println(mean_temp);

    Serial.print("Mean Hum: ");
    Serial.println(mean_hum);

    /*
     * Format the bytes as the algorithm specifies
     */
    
    char str_repr_temp[5];
    sprintf(str_repr_temp, "%4.2f", mean_temp);
    int pre_i = atoi(strtok (str_repr_temp,"."));
    int post_i = atoi(strtok (NULL, "."));
  
    byte pre = adjust_format(pre_i); 
    byte post = adjust_format(post_i);
    byte hum = adjust_format(round(mean_hum));

    int offset = index_packet * 3;

    /* Save bytes in the packet */
    
    packet[offset + 0] = pre;
    packet[offset + 1] = post;
    packet[offset + 2] = hum;

    /* Print for logging */
  
    char log_packet_buffer[100];
    sprintf(log_packet_buffer, "%02X%02X%02X", pre, post, hum);
    Serial.print("Packet: ");
    Serial.println(log_packet_buffer);
  }

  /* Send message */
  send_message(packet);
}