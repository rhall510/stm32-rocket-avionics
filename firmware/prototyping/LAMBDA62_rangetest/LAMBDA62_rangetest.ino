/* Basic LAMBDA62 rangetest
Transmitter sends test payload and receiver listens for each packet and records RSSI and SNR.
Configure 'receiver' boolean for uploading to transmitter and receiver arduino.
Run 'SerialMonitor.py' script on computer connected to the receiver arduino to filter and record serial output.
*/ 

#include <SPI.h>
#include "C:/Dev/Avionics/firmware/shared/LAMBDA62.h"

// Pin numbers
uint8_t nSEL_pin = 10;
uint8_t TXSW_pin = 3;
uint8_t RXSW_pin = 2;
uint8_t BUSY_pin = 4;
uint8_t DIO1_pin = 5;
uint8_t RESET_pin = 6;

// General config
bool receiver = false;
const uint16_t aggNumPackets = 50;

// Packet config
char *payload = "SX1262-RANGETEST-PACKET";
uint8_t packet[25];

// Packet sequence counters
uint16_t sequenceNum = 0;
bool firstPacket = true;

// Aggregated data
uint16_t aggPacketsReceived = 0;
uint16_t aggPacketsMissed = 0;
uint16_t aggCrcErrors = 0;
long aggRssiSum = 0;
int8_t aggRssiMin = 0;
int8_t aggRssiMax = 0;
float aggSnrSum = 0;
unsigned long aggStartTime = 0;
bool aggIntervalStarted = false;

void reset() {
  Serial.println("Resetting module...");
  digitalWrite(RESET_pin, LOW);
  delay(10);
  digitalWrite(RESET_pin, HIGH);
  delay(500);
}

void setXMode(bool RX, bool LL = false) {
  if (LL) {
    digitalWrite(TXSW_pin, LOW);
    digitalWrite(RXSW_pin, LOW);
    return;
  }
  if (RX) {
    digitalWrite(TXSW_pin, LOW);
    digitalWrite(RXSW_pin, HIGH);
  } else {
    digitalWrite(TXSW_pin, HIGH);
    digitalWrite(RXSW_pin, LOW);
  }
}

void executeOperation(uint8_t opcode, uint8_t* vals = nullptr, uint8_t nval = 0, uint8_t* buffer = nullptr, uint8_t nbuff = 0, bool skipStatus = true) {
  checkBusy();

  digitalWrite(nSEL_pin, LOW);

  // Send op-code
  SPI.transfer(opcode);

  // Send any parameters
  if (vals && nval) {
    uint8_t done = 0;
    while (done < nval) {
      SPI.transfer(vals[done]);
      done++;
    }
  }

  // Receive any return values
  if (buffer && nbuff) {
    uint8_t done = 0;
    bool skip = skipStatus;

    while (done < nbuff) {
      if (skip) {
        SPI.transfer(OP_NOP);
        skip = false;
        continue;
      }
      buffer[done] = SPI.transfer(OP_NOP);
      done++;
    }
  }

  digitalWrite(nSEL_pin, HIGH);
}

void checkBusy() {
  unsigned long startTime = millis();
  while (digitalRead(BUSY_pin) == HIGH) {
     if (millis() - startTime > 1000) {
       Serial.println("ERROR: BUSY timeout");
       break;
     }
  }
}

void readBuffer(uint8_t offset, uint8_t* buffer, uint8_t length) {
  uint8_t vals[1] = { offset };
  executeOperation(OP_RBUFF, vals, 1, buffer, length);
}

void writeBuffer(uint8_t* data, uint8_t length, uint8_t offset = 0x00) {
  uint8_t vals[length + 1];
  vals[0] = offset;

  for (int i = 1; i < length + 1; i++) {
    vals[i] = data[i - 1];
  }

  executeOperation(OP_WBUFF, vals, length + 1);
}

void getRxBufferStatus(uint8_t* payloadLen, uint8_t* bufferPtr) {
  uint8_t status[2];
  executeOperation(OP_GETRXBUFFSTATUS, nullptr, 0, status, 2);

  *payloadLen = status[0];
  *bufferPtr = status[1];
}

void clearIrqStatus(uint16_t irqMask = 0xFFFF) {
  uint8_t vals[2] = {(uint8_t)(irqMask >> 8), (uint8_t)irqMask};
  executeOperation(OP_CLEARIRQ, vals, 2);
}

uint16_t getIrqStatus() {
  uint8_t status[2];
  executeOperation(OP_GETIRQ, nullptr, 0, status, 2);

  return (uint16_t)(status[0] << 8) + (uint16_t)status[1];
}

void getPacketStatus(int8_t *rssi, float *snr) {
  uint8_t status[3];
  executeOperation(OP_GETPKTSTATUS, nullptr, 0, status, 3);

  *rssi = -status[0] / 2;
  *snr = (float)((int8_t)status[1]) / 4.0;
}

void waitDIO1(uint16_t timeout = 2000) {
  if (timeout == 0) { 
    while (digitalRead(DIO1_pin) == LOW) {}
  } else {
    unsigned long start = millis();
    while (digitalRead(DIO1_pin) == LOW) {
      if (millis() - start > timeout) {
        Serial.println("Error: DIO1 timeout");
        return;
      }
    }
  }
}

void transmit(uint8_t* message, uint8_t len = 0) {
  if (!len) {
    Serial.println("ERROR! 0 length transmit");
    return;
  }

  uint8_t pktParams[6] = { 0x00, 0x08, 0x00, len, 0x01, 0x00 };
  executeOperation(OP_SETPKTPARAMS, pktParams, 6);

  writeBuffer(message, len);

  setXMode(false);

  uint8_t txParams[3] = { 0x00, 0x00, 0x00 };
  executeOperation(OP_SETTX, txParams, 3);

  waitDIO1();
  clearIrqStatus();
}

void setup() {
  // Initialise packet with empty count header
  for (int i = 2; i < 25; i++) {
    packet[i] = (uint8_t)payload[i - 2];
  }

  Serial.begin(115200);
  while (!Serial);

  pinMode(nSEL_pin, OUTPUT);
  pinMode(TXSW_pin, OUTPUT);
  pinMode(RXSW_pin, OUTPUT);
  pinMode(BUSY_pin, INPUT);
  pinMode(DIO1_pin, INPUT);
  pinMode(RESET_pin, OUTPUT);

  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));

  digitalWrite(nSEL_pin, HIGH);
  setXMode(receiver);

  reset();

  Serial.println("Setting initial params...");
  
  { // Set packet type to LoRa
    uint8_t params[1] = { 0x01 };
    executeOperation(OP_SETPKTTYPE, params, 1);
  }
  
  { // Set modulation params (SF, BW, CR, LDR_OP)
    uint8_t params[4] = { 0x07, 0x04, 0x01, 0x00 };
    executeOperation(OP_SETMODPARAMS, params, 4);
  }

  { // Set frequency to 868MHz
    uint8_t params[4] = { 0x36, 0x40, 0x00, 0x00 };
    executeOperation(OP_SETRFFREQ, params, 4);
  }

  { // Set PA config (DutyCycle, hpMax, deviceSel, paLut)
    uint8_t params[4] = { 0x04, 0x07, 0x00, 0x01 };
    executeOperation(OP_SETPACFG, params, 4);
  }

  { // Set TX params (Power, RampTime)
    uint8_t params[2] = { 0x16, 0x04 };
    executeOperation(OP_SETTXPARAMS, params, 2);
  }
  
  clearIrqStatus();

  if (receiver) {
    // Set DIO1 to go HIGH on Rx done
    uint8_t params[8] = { 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00 };
    executeOperation(OP_SETIRQ, params, 8);

    uint8_t pktParams[6] = { 0x00, 0x0C, 0x00, 0xFF, 0x01, 0x00 };
    executeOperation(OP_SETPKTPARAMS, pktParams, 6);

    Serial.println("Listening for packets...");
    Serial.print("Aggregated stats will output every "); Serial.print(aggNumPackets); Serial.println(" packets");
    Serial.println("");
    Serial.println("RSSI(Avg) | RSSI(Min/Max/Rng) | SNR(Avg) | Miss | CRC | PPS  | Reliability");
    Serial.println("-----------------------------------------------------------------------");

  } else {
    // Set DIO1 to go HIGH on Tx done
    uint8_t params[8] = { 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
    executeOperation(OP_SETIRQ, params, 8);

    Serial.println("Starting packet transmission...");
  }
}

void loop() {
  if (receiver) {
    // Start aggregate interval timer
    if (!aggIntervalStarted) {
      aggStartTime = millis();
      aggIntervalStarted = true;
      aggRssiMin = 127; aggRssiMax = -128;
    }

    // RX single mode
    uint8_t rxParams[] = { 0x00, 0x00, 0x00 };
    executeOperation(OP_SETRX, rxParams, 3);

    // Wait for packet reception
    waitDIO1(0);
    
    // Get IRQ status and check for CRC error
    uint16_t irqStat = getIrqStatus();
    bool crc = irqStat & 0x0040;
    
    int8_t rssi = 0;
    float snr = 0.0;
    
    if (crc) {
      aggCrcErrors++;

      getPacketStatus(&rssi, &snr);
      
      Serial.print("PKT:CRC_ERR,"); Serial.print(rssi); Serial.print(","); Serial.print(snr, 2); Serial.println(",CRC");
      
    } else {
      // Obtain packet length and start offset in buffer
      uint8_t len = 0;
      uint8_t offset = 0;

      getRxBufferStatus(&len, &offset);

      // Read packet data
      uint8_t receivedData[len];
      readBuffer(offset, receivedData, len);

      getPacketStatus(&rssi, &snr);

      // Update aggregate stats
      aggPacketsReceived++;
      aggRssiSum += rssi;
      aggSnrSum += snr;
      if (rssi < aggRssiMin) aggRssiMin = rssi;
      if (rssi > aggRssiMax) aggRssiMax = rssi;

      // Get received sequence number
      uint16_t currNum = (uint16_t)(receivedData[0] << 8) + (uint16_t)receivedData[1];

      // Ignore first packet for missing calculation
      if (firstPacket) {
        sequenceNum = currNum; 
        firstPacket = false;
      }
      
      // Calculate number of missed packets
      if (sequenceNum != currNum) {
        aggPacketsMissed += (currNum - sequenceNum);
        sequenceNum = currNum;
      }

      Serial.print("PKT:"); Serial.print(currNum); Serial.print(","); Serial.print(rssi); Serial.print(","); Serial.print(snr, 2); Serial.println(",OK");
    }

    clearIrqStatus();

    sequenceNum++; 

    // Print aggregated stats once interval exceeded
    if (aggPacketsReceived >= aggNumPackets) {
      float duration = (millis() - aggStartTime) / 1000.0;
      float avgRssi = (float)aggRssiSum / aggPacketsReceived;
      float avgSnr = aggSnrSum / aggPacketsReceived;
      float pps = (float)aggPacketsReceived / duration;
      float reliability = 100.0 * ((float)aggPacketsReceived / (aggPacketsReceived + aggPacketsMissed + aggCrcErrors));

      Serial.print(avgRssi, 1); Serial.print(" dBm  | ");
      Serial.print(aggRssiMin); Serial.print("/"); Serial.print(aggRssiMax); Serial.print("/"); Serial.print(aggRssiMax - aggRssiMin); Serial.print("       | ");
      Serial.print(avgSnr, 1); Serial.print(" dB |  ");
      Serial.print(aggPacketsMissed); Serial.print("   |  ");
      Serial.print(aggCrcErrors); Serial.print("  | ");
      Serial.print(pps, 1); Serial.print("  | ");
      Serial.print(reliability, 1); Serial.println("%");

      aggPacketsReceived = 0; aggPacketsMissed = 0; aggCrcErrors = 0; aggRssiSum = 0; aggSnrSum = 0; aggIntervalStarted = false;
    }

  } else {
    // Transmit range test packet
    packet[0] = (uint8_t)(sequenceNum >> 8);
    packet[1] = (uint8_t)sequenceNum;

    transmit(packet, 25);

    sequenceNum++;
    delay(20); 
  }
}




