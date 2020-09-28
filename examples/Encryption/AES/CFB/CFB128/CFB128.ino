#include <Crypto.h>
#include <CryptoLegacy.h>
#include <AES.h>
#include <CFB.h>
//#include <string.h> // test only

//#define cipherdebug 1
#define MAX_PLAINTEXT_SIZE  64
#define MAX_CIPHERTEXT_SIZE 64

byte KEY[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};

byte IV[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
               0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

byte datam[64];
//size_t size = 64;
String s = "quackquack";
int sLength = s.length();
s.getBytes(datam,sLength+1);
size_t datamSize = 64;

byte ciphertext[64] = {0x3b, 0x3f, 0xd9, 0x2e, 0xb7, 0x2d, 0xad, 0x20,
                    0x33, 0x34, 0x49, 0xf8, 0xe8, 0x3c, 0xfb, 0x4a,
                    0xc8, 0xa6, 0x45, 0x37, 0xa0, 0xb3, 0xa9, 0x3f,
                    0xcd, 0xe3, 0xcd, 0xad, 0x9f, 0x1c, 0xe5, 0x8b,
                    0x26, 0x75, 0x1f, 0x67, 0xa3, 0xcb, 0xb1, 0x40,
                    0xb1, 0x80, 0x8c, 0xf1, 0x87, 0xa4, 0xf4, 0xdf,
                    0xc0, 0x4b, 0x05, 0x35, 0x7c, 0x5d, 0x1c, 0x0e,
                    0xea, 0xc4, 0xc6, 0x6f, 0x9f, 0xf7, 0xf2, 0xe6};

byte buffer[128];

CFB<AES128> cfbaes128;

void encryptData(Cipher *cipher, const uint8_t data[], uint8_t encryptedData[], size_t inc)
{
  size_t posn, len;
  for (posn = 0; posn < datamSize; posn += inc)
  {
    len = datamSize - posn;
    if (len > inc)
      len = inc;
    cipher->encrypt(encryptedData + posn, data + posn, len);
  }
  cipher->clear();
}

void decryptData(Cipher *cipher, const uint8_t encryptedData[], uint8_t data[], size_t inc)
{
  size_t posn, len;
  for (posn = 0; posn < datamSize; posn += inc)
  {
    len = datamSize - posn;
    if (len > inc)
      len = inc;
    cipher->decrypt(data + posn, encryptedData + posn, len);
  }
  cipher->clear();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Ready");
  Serial.println((char*)datam);
//  if (prepareCipher(&CIPHER,KEY,IV))
//  {
//    encryptData(&CIPHER,datam,ciphertext,tama);
//    Serial.println((char*)ciphertext);
//    memset(datam, 0xBA, tama);
//    Serial.println((char*)datam);
//    if (prepareCipher(&CIPHER,KEY,IV))
//    {
//      decryptData(&CIPHER,ciphertext,datam,tama);
//      Serial.println((char*)ciphertext);
//      Serial.println((char*)datam);
//    }
//  }
//  prepareCipher(&cbcaes128, KEY, IV);
  encryptData(&cfbaes128, datam, ciphertext, datamSize);
  
  Serial.println((char*)ciphertext);
//  memset(datam, 0xBA, datamSize);
  Serial.println((char*)datam);
  
  decryptData(&cfbaes128,ciphertext,datam,datamSize);
  Serial.println((char*)ciphertext);
  Serial.println((char*)datam);
  
}

void loop() // not used
{
  
}
