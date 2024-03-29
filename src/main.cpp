#include <SPI.h>
#include <SD.h>

// note: https://forum.arduino.cc/t/some-success-with-writecid/66231/50

/*  Wiring:

CHIP_SELECT=1 - DAT3 (1)
MISO=0 - DAT0 (7)
MOSI=3 - CMD (2)
SCK=2  - CLK (5)

*/

#define CARD_VCC 29

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

uint16_t // Returns Calculated CRC value
CalculateCRC16(
    uint16_t crc,      // Seed for CRC calculation
    const void *c_ptr, // Pointer to byte array to perform CRC on
    size_t len);

#ifdef WATCH_ENABLE
#include "sd_rx.pio.h"

static PIO pio;
static uint sm;
#endif

#define CID_SIZE 16
#define PROGRAM_CID_OPCODE 26
#define SAMSUNG_VENDOR_OPCODE 62

#define ERASE 8
#define LOCK_UNLOCK 4
#define CLR_PWD 2
#define SET_PWD 1

//sandisk microsd 16GB;  35344534231364780CE77BEDD1565F
uint8_t password[] = { 0x4c, 0xa4, 0x32, 0xa0, 0x9c, 0xcc, 0x71, 0x1a, 0x5a, 0xfe, 0xf8, 0xc5, 0x60, 0xe3, 0xfd, 0x54 };

uint8_t new_password[20];

//cina SD 16GB; E7EH9325LX 9F5449202020202020345BA816BD3
//uint8_t password[] = { 0x8d, 0x2f, 0x70, 0x83, 0x1e, 0x9f, 0x81, 0xd9, 0xf2, 0xff, 0xf4, 0xf8, 0x0e, 0x27, 0x08, 0x14 };

//sandisk 16GB; 1136AVD1147S; 35344534231364780CF37BE921562F
//uint8_t password[] = { 0xBB, 0x37, 0x3D, 0x55, 0x43, 0xCF, 0x6E, 0x33, 0xD1, 0xAF, 0x1A, 0x37, 0x17, 0x27, 0x43, 0x07};

//sandisk 32GB 0367YXGPK05Q: 03 53 44 53 43 33 32 47 80 F4 7C B7 1B 01 49 85
//uint8_t password[] = { 0x2b, 0xdf, 0xd7, 0x6e, 0x0f, 0x65, 0x4b, 0x73, 0x1e, 0x77, 0xa4, 0xe6, 0x90, 0x7a, 0xc0, 0x5b};

//sandisk 16GB 8346ZVFKN40G: 03 53 44 53 43 31 36 47 80 0A 5F 36 DB 01 29 EB 
//uint8_t password[] = { 0x35, 0x69, 0x1e, 0x6d, 0xcb, 0xdd, 0x9c, 0xd3, 0x9f, 0xbc, 0x91, 0x7a, 0xcf, 0x88, 0x97, 0x2d};


/*
void samsung_unlock_cid_write(){
  int ret = 0;

  ret = card.cardCommand(SAMSUNG_VENDOR_OPCODE, 0xEFAC62EC); //enter vendor mode
  Serial.printf("enter vendor %d\n", ret);
  
  ret = card.cardCommand(SAMSUNG_VENDOR_OPCODE, 0xEF50); //unlock
  Serial.printf("unlock %d\n", ret);

  ret = card.cardCommand(SAMSUNG_VENDOR_OPCODE, 0x00DECCEE); //exit vendor mode
  Serial.printf("exit vendor %d\n", ret);
  
}
*/

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      Serial.println("no more files");
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void card_init(){
  int ret;
  Serial.print("\nInitializing SD card...");

  digitalWrite(CARD_VCC, 0);

  pinMode(CHIP_SELECT, OUTPUT);

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  ret = card.init(SPI_HALF_SPEED, CHIP_SELECT);

  if ((!ret) && card.errorCode() == SD_CARD_ERROR_LOCKED) {
    Serial.println("Card is locked. Unlocking.");
    ret = card.lockUnlockCard(0, 16, password);
    Serial.print("Unlock result");
    Serial.println(ret);
    ret = card.init(SPI_HALF_SPEED, CHIP_SELECT);
  }

  if (!ret) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    return;
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  // print the type of card
  Serial.println();
  Serial.print("Card type:         ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    return;
  }

  Serial.print("Clusters:          ");
  Serial.println(volume.clusterCount());
  Serial.print("Blocks x Cluster:  ");
  Serial.println(volume.blocksPerCluster());

  Serial.print("Total Blocks:      ");
  Serial.println(volume.blocksPerCluster() * volume.clusterCount());
  Serial.println();

  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("Volume type is:    FAT");
  Serial.println(volume.fatType(), DEC);

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1 KB)
  Serial.print("Volume size (KB):  ");
  Serial.println(volumesize);
  Serial.print("Volume size (MB):  ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (GB):  ");
  Serial.println((float)volumesize / 1024.0);

}

void list_files(){
  Serial.println("\nFiles found on the card (name, date and size in bytes): ");

  root.openRoot(volume);

  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);

  root.close();
}

void show_cid(){
  cid_t *cid;
  cid = (cid_t*)malloc(sizeof(cid_t));
  card.readCID(cid);

  Serial.print("Card CID: ");
  for (int i=0; i<16; i++){
    Serial.print(((unsigned char*)cid)[i], HEX);
  }
  Serial.println();
}

void unlock(){
  int ret;
  int flags = CLR_PWD;
  
  ret = card.lockUnlockCard(flags, 16, password);

  Serial.println(ret);
}

void lock(){
  int ret;
  int flags = SET_PWD + LOCK_UNLOCK;

  ret = card.lockUnlockCard(flags, 16, password);

  Serial.println(ret);
}

void erase(){
  int ret;
  int flags = ERASE;

  ret = card.lockUnlockCard(flags, 16, password);

  Serial.println(ret);
}

void delete_devid(){
  dir_t p;

  root.openRoot(volume);

  while (root.readDir(&p) > 0) {
    Serial.println((char*)(p.name));

    if(!memcmp("NAVPSF~1", p.name, 8)){
      Serial.println("NAVPSF~1 found");

      uint16_t index = root.curPosition() / 32 - 1;
      SdFile s;

      if (s.open(root, index, O_READ)) {
        uint8_t ret = s.remove(&s, "deviceid");
        Serial.print("deviceid remove ret:");
        Serial.println(ret);
      }
    }
  }

  root.close();
}

/*
void read_version(){
  dir_t p;

  while (root.readDir(&p) > 0) {
    Serial.printf("%s\n", p.name);

    if(!memcmp("NAVPSF~1", p.name, 8)){
      Serial.printf("%s found\n", p.name);

      uint16_t index = root.curPosition() / 32 - 1;
      SdFile s;

      if (s.open(root, index, O_READ)) {
        if (s.open(&s, "version.txt", O_READ)){
            int ln = 1;
            while ((n = s.fgets(line, sizeof(line))) > 0) {
                // Print line number.
                Serial.print(ln++);
                Serial.print(": ");
                Serial.print(line);
                if (line[n - 1] != '\n') {
                  // Line is too long or last line is missing nl.
                  Serial.println(F(" <-- missing nl"));
                }
            }
            Serial.println(F("\nDone"));
        }
      }
    }
  }
}
*/

void print_password(){
  Serial.print("Password: ");
  for (int i=0; i<16; i++){
    Serial.print(password[i], HEX);
  }
  Serial.println();
}

int halfchar() {
  int c = -1;
  
  while (((c = Serial.read()) == -1) || (c == ' ') || (c == '\n')){
  }

  Serial.print((char)c);

  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'a') && (c <= 'f')) {
    return c - 'a' + 10;
  }
  if ((c >= 'A') && (c <= 'F')) {
    return c - 'A' + 10;
  }
  return -1;
}

int read_hex(){
  int hc;
  int val = 0;

  hc = halfchar();

  if (hc < 0){
    return -1;
  }

  val = hc*16;

  hc = halfchar();

  if (hc < 0){
    return -1;
  }

  return val + hc;
}

void set_password(){
  Serial.print("Input password as 16 hex pairs: \n");
  for (int i=0; i<16; i++){
    int x = read_hex();
    if (x < 0){
      Serial.print("wrong input\n");
      return;
    }
    password[i] = x;
    Serial.print(" byte value: ");
    Serial.print((char)x, HEX);
  }
  Serial.println();

  print_password();
}

void watch_password(){
  SPI.end();

  digitalWrite(CARD_VCC, 1);

  pinMode(MOSI, INPUT);
  digitalWrite(MOSI, 0);
  
  pinMode(MISO, INPUT);
  digitalWrite(MISO, 0);

  pinMode(SCK, INPUT);
  digitalWrite(SCK, 0);

  pinMode(CHIP_SELECT, INPUT);
  digitalWrite(CHIP_SELECT, 0);

  Serial.println("Waiting for startbit");

#ifdef WATCH_ENABLE
  pio = pio0;
  sm = 0;
  uint offset = pio_add_program(pio, &sd_rx_program);
  sd_rx_program_init(pio, sm, offset, MISO, SCK);
#endif

/*
  // not fast enough :/
  attachInterrupt(SCK, isr, RISING);

  while(digitalRead(MISO)); //wait for start bit

  while(!digitalRead(MISO)); //wait for start bit

  while(digitalRead(MISO)); //wait for start bit

  while(!digitalRead(MISO)); //wait for start bit

  while(digitalRead(MISO)); //wait for start bit

  for(int i=0; i<8*32; i++){
    while(!digitalRead(SCK)); //wait for rising

    int val = digitalRead(MISO);
    digitalWrite(TEST_PIN, !val);
    Serial.print(val);
    Serial.print(" ");
    while(digitalRead(SCK));

  }
*/


}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  pinMode(CARD_VCC, OUTPUT);
  digitalWrite(CARD_VCC, 1);

  /*
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  */
watch_password();
}

void move_password(){
  memcpy(password, new_password+2, 16);
  Serial.println("password moved");
}

void loop(void) {
  if (Serial.available() > 0) {
    int cmd = Serial.read();

    switch (cmd){
      case 'i':
        card_init();
        break;
      case 'l':
        lock();
        break;
      case 'u':
        unlock();
        break;
      case 'e':
        erase();
        break;
      case 'c':
        show_cid();
        break;
      case 'd':
        delete_devid();
        break;
      case 'p':
        print_password();
        break;
      case 's':
        set_password();
        break;
      case 'f':
        list_files();
        break;
      case 'w':
        watch_password();
        break;
      case 'm':
        move_password();
        break;
      default:
        Serial.println("Unknown command: card_[i]nit, [l]ock, [u]nlock, [e]rase, show_[c]id, [d]elete_devid, "
        "[p]rint_password, [s]et_password, list_[f]iles, [w]atch_password, [m]ove_password");
    }
  }

#ifdef WATCH_ENABLE
  static int password_part = -1;

  while(!pio_sm_is_rx_fifo_empty(pio, sm)) {
      uint32_t c = sd_rx_program_getc(pio, sm);

      if ((password_part < 0) && ((c & 0xffff0000) == 0x00100000)){
        password_part = 0;
        Serial.println("password start seen");
      }

      if (password_part > -1){
        new_password[password_part+3] = c & 0xff;
        new_password[password_part+2] = (c >> 8) & 0xff;
        new_password[password_part+1] = (c >> 16) & 0xff;
        new_password[password_part+0] = (c >> 24) & 0xff;
        password_part +=4;
      }

      if (password_part > 20){
        password_part = -1;
        Serial.print("Password found:\n");
        for (int i=0; i<20; i++){
          Serial.print(((uint8_t*)new_password)[i], HEX);
          Serial.print(" ");
        }
        uint16_t crc = CalculateCRC16(0x0, new_password, 18);
        Serial.print("\nCalculated crc: ");
        Serial.println(crc, HEX);

        if (crc == (new_password[18]*0x100 + new_password[19])){
          Serial.print("CRC match!");
        } else {
          Serial.print("CRC fail!");
        }

      }

  }

#endif

}
