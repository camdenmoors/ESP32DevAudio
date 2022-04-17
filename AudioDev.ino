#include <WiFi.h>
#include <Wire.h>
#include "Talkie.h"
#include "Vocab_US_Large.h"
#include "Vocab_Special.h"
#include "esp_wifi.h"

Talkie voice;

String maclist[2048][3];
int listcount = 0;

String defaultTTL = "20"; // Maximum time in seconds before device is considered offline

const wifi_promiscuous_filter_t filt = {
  .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
};

typedef struct {
  unsigned frame_ctrl: 16;
  unsigned duration_id: 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl: 16;
  uint8_t addr4[6]; /* optional */
}
wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
}
wifi_ieee80211_packet_t;

#define maxCh 11 //max Channel -> US = 11, EU = 13, Japan = 14

int curChannel = 1;

const char * wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type) {
  switch (type) {
  case WIFI_PKT_MGMT:
    return "MGMT";
  case WIFI_PKT_DATA:
    return "DATA";
  default:
  case WIFI_PKT_MISC:
    return "MISC";
  }
}

void sniffer(void * buff, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t * ppkt = (wifi_promiscuous_pkt_t * ) buff;
  const wifi_ieee80211_packet_t * ipkt = (wifi_ieee80211_packet_t * ) ppkt -> payload;
  const wifi_ieee80211_mac_hdr_t * hdr = & ipkt -> hdr;

  char mac1[13];
  char mac2[13];
  char mac3[13];

  // Format MAC Address as string
  sprintf(mac1, "%02x%02x%02x%02x%02x%02x", hdr -> addr1[0], hdr -> addr1[1], hdr -> addr1[2], hdr -> addr1[3], hdr -> addr1[4], hdr -> addr1[5]);
  sprintf(mac2, "%02x%02x%02x%02x%02x%02x", hdr -> addr2[0], hdr -> addr2[1], hdr -> addr2[2], hdr -> addr2[3], hdr -> addr2[4], hdr -> addr2[5]);
  sprintf(mac3, "%02x%02x%02x%02x%02x%02x", hdr -> addr3[0], hdr -> addr3[1], hdr -> addr3[2], hdr -> addr3[3], hdr -> addr3[4], hdr -> addr3[5]);

  char * macs[] = {
    mac1,
    mac2,
    mac3
  };

  //  Serial.println(mac1);
  //  Serial.println(mac2);
  //  Serial.println(mac3);

  // For all MACs
  for (int macIndex = 0; macIndex <= 2; macIndex++) {
    // Except ffffffffffff
    if (!(strcmp("ffffffffffff", macs[macIndex]) == 0)) {
      int added = 0;

      // Checks if the MAC address has been seen before
      for (int i = 0; i <= 2047; i++) {
        if (strcmp(macs[macIndex], maclist[i][0].c_str()) == 0) {
          maclist[i][1] = defaultTTL;
          if (maclist[i][2] == "OFFLINE") {
            maclist[i][2] = "0";
          }
          added = 1;
        }
      }

      if (added == 0) {
        maclist[listcount][0] = macs[macIndex];
        maclist[listcount][1] = defaultTTL;
        listcount++;
        if (listcount >= 2047) {
          Serial.println("Too many addresses");
          listcount = 0;
        }
      }
    }
  }

}

void setup() {
  /* start Serial */
  Serial.begin(115200);

  /* setup wifi */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init( & cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter( & filt);
  esp_wifi_set_promiscuous_rx_cb( & sniffer);
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
}

// Manges TTL
void purge() { 
  for (int i = 0; i <= 2047; i++) {
    if (!(maclist[i][0] == "")) {
      int ttl = (maclist[i][1].toInt());
      ttl--;
      if (ttl <= 0) {
        maclist[i][2] = "OFFLINE";
        maclist[i][1] = defaultTTL;
      } else {
        maclist[i][1] = String(ttl);
      }
    }
  }
}

// Updates the time the device has been online for and returns a count of live devices
int updatetime() {
  int liveCount = 0;
  for (int i = 0; i <= 2047; i++) {
    if (!(maclist[i][0] == "")) {
      if (maclist[i][2] == "") maclist[i][2] = "0";
      if (!(maclist[i][2] == "OFFLINE")) {
        int timehere = (maclist[i][2].toInt());
        timehere++;
        maclist[i][2] = String(timehere);
        liveCount++;
      }

      Serial.println(maclist[i][0] + " : " + maclist[i][2]);
    }
  }
  return liveCount;
}

void loop() {
  if (curChannel > maxCh) {
    curChannel = 1;
  }
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
  delay(1000);
  sayNumber(updatetime());
  Serial.println();
  purge();
  curChannel++;
}

// Say any number between -999,999 and 999,999
void sayNumber(long n) {
  if (n < 0) {
    voice.say(sp2_MINUS);
    sayNumber(-n);
  } else if (n == 0) {
    voice.say(sp2_ZERO);
  } else {
    if (n >= 1000) {
      int thousands = n / 1000;
      sayNumber(thousands);
      voice.say(sp2_THOUSAND);
      n %= 1000;
      if ((n > 0) && (n < 100))
        voice.say(sp2_AND);
    }
    if (n >= 100) {
      int hundreds = n / 100;
      sayNumber(hundreds);
      voice.say(sp2_HUNDRED);
      n %= 100;
      if (n > 0)
        voice.say(sp2_AND);
    }
    if (n > 19) {
      int tens = n / 10;
      switch (tens) {
      case 2:
        voice.say(sp2_TWENTY);
        break;
      case 3:
        voice.say(sp2_THIR_);
        voice.say(sp2_T);
        break;
      case 4:
        voice.say(sp2_FOUR);
        voice.say(sp2_T);
        break;
      case 5:
        voice.say(sp2_FIF_);
        voice.say(sp2_T);
        break;
      case 6:
        voice.say(sp2_SIX);
        voice.say(sp2_T);
        break;
      case 7:
        voice.say(sp2_SEVEN);
        voice.say(sp2_T);
        break;
      case 8:
        voice.say(sp2_EIGHT);
        voice.say(sp2_T);
        break;
      case 9:
        voice.say(sp2_NINE);
        voice.say(sp2_T);
        break;
      }
      n %= 10;
    }
    switch (n) {
    case 1:
      voice.say(sp2_ONE);
      break;
    case 2:
      voice.say(sp2_TWO);
      break;
    case 3:
      voice.say(sp2_THREE);
      break;
    case 4:
      voice.say(sp2_FOUR);
      break;
    case 5:
      voice.say(sp2_FIVE);
      break;
    case 6:
      voice.say(sp2_SIX);
      break;
    case 7:
      voice.say(sp2_SEVEN);
      break;
    case 8:
      voice.say(sp2_EIGHT);
      break;
    case 9:
      voice.say(sp2_NINE);
      break;
    case 10:
      voice.say(sp2_TEN);
      break;
    case 11:
      voice.say(sp2_ELEVEN);
      break;
    case 12:
      voice.say(sp2_TWELVE);
      break;
    case 13:
      voice.say(sp2_THIR_);
      voice.say(sp2__TEEN);
      break;
    case 14:
      voice.say(sp2_FOUR);
      voice.say(sp2__TEEN);
      break;
    case 15:
      voice.say(sp2_FIF_);
      voice.say(sp2__TEEN);
      break;
    case 16:
      voice.say(sp2_SIX);
      voice.say(sp2__TEEN);
      break;
    case 17:
      voice.say(sp2_SEVEN);
      voice.say(sp2__TEEN);
      break;
    case 18:
      voice.say(sp2_EIGHT);
      voice.say(sp2__TEEN);
      break;
    case 19:
      voice.say(sp2_NINE);
      voice.say(sp2__TEEN);
      break;
    }
  }
}
