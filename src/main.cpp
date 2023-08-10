#include <SPI.h>
#include <SD.h>

// note: https://forum.arduino.cc/t/some-success-with-writecid/66231/50

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

#define CID_SIZE 16
#define PROGRAM_CID_OPCODE 26
#define SAMSUNG_VENDOR_OPCODE 62

#define ERASE 8
#define LOCK_UNLOCK 4
#define CLR_PWD 2
#define SET_PWD 1

const int chipSelect = 5;

//sandisk microsd 16GB;  35344534231364780CE77BEDD1565F
uint8_t password[] = { 0x4c, 0xa4, 0x32, 0xa0, 0x9c, 0xcc, 0x71, 0x1a, 0x5a, 0xfe, 0xf8, 0xc5, 0x60, 0xe3, 0xfd, 0x54 };

//cina SD 16GB; E7EH9325LX 9F5449202020202020345BA816BD3
//uint8_t password[] = { 0x8d, 0x2f, 0x70, 0x83, 0x1e, 0x9f, 0x81, 0xd9, 0xf2, 0xff, 0xf4, 0xf8, 0x0e, 0x27, 0x08, 0x14 };

//sandisk 16GB; 1136AVD1147S; 35344534231364780CF37BE921562F
//uint8_t password[] = { 0x00, 0x08, 0x26, 0x52, 0x19, 0x50, 0x4E, 0x66, 0x38, 0x8D, 0x2D, 0x7F, 0x7C, 0x62, 0xB0, 0x71, 0xFE, 0xAA, 0x64, 0x93, 0xF2, 0x80};
//uint8_t password[] = { 0xBB, 0x37, 0x3D, 0x55, 0x43, 0xCF, 0x6E, 0x33, 0xD1, 0xAF, 0x1A, 0x37, 0x17, 0x27, 0x43, 0x07};


void samsung_unlock_cid_write(){
  int ret = 0;

  ret = card.cardCommand(SAMSUNG_VENDOR_OPCODE, 0xEFAC62EC); //enter vendor mode
  Serial.printf("enter vendor %d\n", ret);
  
  ret = card.cardCommand(SAMSUNG_VENDOR_OPCODE, 0xEF50); //unlock
  Serial.printf("unlock %d\n", ret);

  ret = card.cardCommand(SAMSUNG_VENDOR_OPCODE, 0x00DECCEE); //exit vendor mode
  Serial.printf("exit vendor %d\n", ret);
  
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      Serial.printf("no more files\n");
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

void setup() {
  int ret;
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  /*
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  */
  help();
}

void init(){
  int ret;
  Serial.print("\nInitializing SD card...");

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  ret = card.init(SPI_HALF_SPEED, chipSelect);

  if ((!ret) && card.errorCode() == SD_CARD_ERROR_LOCKED) {
    Serial.println("Card is locked. Unlocking.");
    ret = card.lockUnlockCard(0, 16, password);
    Serial.printf("Unlock result %d\n", ret);
    ret = card.init(SPI_HALF_SPEED, chipSelect);
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

  Serial.printf("Card CID: ");
  for (int i=0; i<16; i++){
    Serial.printf("%02X ", ((unsigned char*)cid)[i]);  
  }
  Serial.println("");
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

void delete_devid(){
  dir_t p;

  root.openRoot(volume);

  while (root.readDir(&p) > 0) {
    Serial.printf("%s\n", p.name);

    if(!memcmp("NAVPSF~1", p.name, 8)){
      Serial.printf("%s found\n", p.name);

      uint16_t index = root.curPosition() / 32 - 1;
      SdFile s;

      if (s.open(root, index, O_READ)) {
        uint8_t ret = s.remove(&s, "deviceid");
        Serial.printf("deviceid remove ret: %d\n", ret);
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
    Serial.printf("%02X ", password[i]);
  }
  Serial.println();
}

int halfchar() {
  int c = -1;
  
  while (((c = Serial.read()) == -1) || (c == ' ') || (c == '\n')){
  }

  Serial.printf("%c", c);

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
  char ch;
  Serial.print("Input password as 16 hex pairs: \n");
  for (int i=0; i<16; i++){
    int x = read_hex();
    if (x < 0){
      Serial.print("wrong input\n");
      return;
    }
    password[i] = x;
    Serial.printf(" byte value: %02X\n", x);
  }
  Serial.println();

  print_password();
}

void help(void) {
  Serial.println("Command");
  Serial.println("i: open the SD Card (first step)");
  Serial.println("l: lock the SD Card with the current passwort");
  Serial.println("u: unlock the SD Card with the current passwort");
  Serial.println("c: show the CID from the SD Card");
  Serial.println("d: delete the deviceid from the SD");
  Serial.println("p: print the current passwort");
  Serial.println("s: set a new passwort");
  Serial.println("f: print all file name");
  Serial.println("h: print this help");
}

void loop(void) {
  if (Serial.available() > 0) {
    int cmd = Serial.read();

    switch (cmd){
      case 'i':
        init();
        break;
      case 'l':
        lock();
        break;
      case 'u':
        unlock();
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
      case 'h':
        help();
        break;
      default:
        Serial.println("Unknown command");
        help();
    }
  }
}
