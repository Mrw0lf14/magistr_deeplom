//------------------------------------------------------------------------------------------------------------------------
// 
// Title: SD Card Benchmark for ESP2 using 1Bit SPI
//
// Description:
// SD Card Benchmark is based on "bench.ino" from "SdFat 2.1.2".
// Because there are many differences and SdFat does no support ESP32 this is almost a rewrite.
//------------------------------------------------------------------------------------------------------------------------

//
// Includes
// SD Card library, usually part of the standard installation
#include "SD.h"

//------------------------------------------------------------------------------------------------------------------------
// Defines
// Digital I/O used for SD card
#define SD_CS         10          // SD Card chip select
#define SPI_MOSI      11
#define SPI_MISO      13
#define SPI_SCK       12

//------------------------------------------------------------------------------------------------------------------------

// Constants
// Array of frequencies to be tested for connection
//const uint32_t frequencies[] =
//{
//   1000000,  2000000,  4000000,  8000000,
//  10000000, 12000000, 14000000, 16000000,
//  18000000, 20000000, 21000000, 22000000,
//  23000000, 24000000, 25000000
//};
const uint32_t frequencies[] =
{
  25000000, 40000000
};

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = true;

// Size of read/write.
const size_t BUF_SIZE = 512;

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 5;

// Write pass count.
const uint8_t WRITE_COUNT = 5;

// Read pass count.
const uint8_t READ_COUNT = 5;

//------------------------------------------------------------------------------------------------------------------------
// Structures and also variables
//------------------------------------------------------------------------------------------------------------------------
// File size in bytes.
const uint32_t FILE_SIZE = 1000000UL * FILE_SIZE_MB;

// Ensure 4-byte alignment.
uint32_t buf32[ ( BUF_SIZE + 3 ) / 4 ];
uint8_t* buf = (uint8_t*)buf32;

// Test file
File file;

void setup()
{
  Serial.begin( 115200 );
  // Wait for USB Serial
  while( !Serial )
    delay( 100 );
  delay( 1000 );

  Serial.println( "Use a freshly formatted SD for best performance." );
  Serial.println( "FILE_SIZE_MB = " + String( FILE_SIZE_MB ) );
  Serial.println( "BUF_SIZE = " + String( BUF_SIZE ) + " bytes" );
}

void loop()
{
  for( uint8_t numfreq = 0; numfreq < sizeof( frequencies ) / sizeof( uint32_t ); numfreq++ )
  {
    Serial.println( "" );
    if( SDCardInit( frequencies[ numfreq ] ) )
    {
      // Open file for write access.
      // Random access is not possible with SD.h
      file = SD.open( "/bench.dat", FILE_WRITE );
      if( !file )
      {
        Serial.println( "File open failed." );
      }
      else
      {
        if( WriteTest() )
        {
          delay( 500 );
          if( CheckFileContent() )
          {
            ReadTest();
          }
        }
      }

      SDCardUnmount();
    }
    delay( 5000 );
  }

  Serial.println( "Done" );

  esp_deep_sleep_start();
}

// Initialize SD card
bool SDCardInit( uint32_t frequency )
{
  bool init = false;
    
  // Configure SD ChipSelect pin as output
  pinMode( SD_CS, OUTPUT );

  // SD card chips select, must use GPIO 5 (ESP32 SS)
  digitalWrite( SD_CS, HIGH );

  Serial.println( "Trying to connect to SD card with " + String( frequency / 1000000 ) + " MHz." );
  // Depending on the SD card a higher frequency might help to get data fast enough
  if( !SD.begin( SD_CS, SPI, frequency ) )
  {
    Serial.println( "Error talking to SD card!" );
  }
  else
  {
    Serial.println( "SD card initialized successfull" );
    Serial.print( "SD card type=" );
    switch( SD.cardType() )
    {
      case CARD_NONE:
        Serial.println( "None" );
        break;
      case CARD_MMC:
        Serial.println( "MMC" );
        break;
      case CARD_SD:
        Serial.println( "SD" );
        break;
      case CARD_SDHC:
        Serial.println( "SDHC" );
        break;
      case CARD_UNKNOWN:
        Serial.println( "Unknown" );
        break;
      default:
        Serial.println( "Unknown" );
    }

    Serial.print( "Card size:   " ); Serial.print( SD.cardSize() / 1000000 ); Serial.println( "MB" );
    Serial.print( "Total bytes: " ); Serial.print( SD.totalBytes() / 1000000 ); Serial.println( "MB" );
    Serial.print( "Used bytes:  " ); Serial.print( SD.usedBytes() / 1000000 ); Serial.println( "MB" );

    init = true;
  }

  return init;
}

// Unmount SD card
void SDCardUnmount()
{
  Serial.print( "Unmounting SD card..." );

  // Unmount SD card
  SD.end();

  Serial.println( "done." );
}

// Start write test
bool WriteTest()
{
  float s;
  uint64_t t, m;
  uint64_t maxLatency;
  uint64_t minLatency;
  uint64_t totalLatency;
  uint32_t avgLatency;
  bool skipLatency;
  uint32_t n = FILE_SIZE / BUF_SIZE;

  Serial.println( "Starting write test, please wait." );

  // First fill buffer "buf" with known data that can be checked later
  if( BUF_SIZE > 1 )
  {
    for( size_t i = 0; i < ( BUF_SIZE - 2 ); i++ )
      buf[ i ] = 'A' + ( i % 26 );
    buf[ BUF_SIZE - 2 ] = '\r';
  }
  buf[ BUF_SIZE - 1 ] = '\n';

  Serial.println( "write speed and latency" );
  Serial.println( "speed,max,min,avg" );
  Serial.println( "KB/Sec,usec,usec,usec" );
  for( uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++ )
  {
    file.seek( 0 );

    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;

    t = micros();
    for( uint32_t i = 0; i < n; i++ )
    {
      m = micros();
      if( file.write( buf, BUF_SIZE ) != BUF_SIZE )
      {
        Serial.println( "write failed" );
        file.close();
        return false;
      }
      m = micros() - m;
      totalLatency += m;
      if( skipLatency )
      {
        // Wait until first write to SD, not just a copy to the cache.
        skipLatency = file.position() < 512;
      }
      else
      {
        if( maxLatency < m )
          maxLatency = m;
        if( minLatency > m )
          minLatency = m;
      }
    }
    file.flush();
    t = micros() - t;
    s = file.size();
    
    if( SKIP_FIRST_LATENCY )
      avgLatency = (uint32_t)( totalLatency / ( n - 1 ) );
    else
      avgLatency = (uint32_t)( totalLatency / n );
    
    Serial.println( String( (uint32_t)( s * 1000 / t ) ) + ',' + String( (uint32_t)maxLatency ) + ',' + String( (uint32_t)minLatency ) + ',' + String( avgLatency ) );
  }

  file.close();
  Serial.println( "Write test finished successfully." );
  return true;
}

// Check the file content
bool CheckFileContent()
{
  // Open fie for reading
  file = SD.open( "/bench.dat", FILE_READ );
  if( !file )
  {
    Serial.println( "File open failed." );
    return false;
  }

  Serial.println( "Checking file content, please wait." );
  uint32_t n = FILE_SIZE / BUF_SIZE;
  for( uint32_t i = 0; i < n; i++ )
  {
    int32_t nr = file.read( buf, BUF_SIZE );
    if( nr != BUF_SIZE )
    {
      Serial.println( "read failed, bytes read: " + String( nr ) );
      file.close();
      return false;
    }
    for( size_t i = 0; i < ( BUF_SIZE - 2 ); i++ )
    {
      if( buf[ i ] != ( 'A' + ( i % 26 ) ) )
      {
        Serial.println( "data check error" );
        file.close();
        return false;
      }
    }
    if( buf[ BUF_SIZE - 2 ] != '\r' )
    {
      Serial.println( "data check error" );
      file.close();
      return false;
    }
    if( buf[ BUF_SIZE - 1 ] != '\n' )
    {
      Serial.println( "data check error" );
      file.close();
      return false;
    }
  }
  
  file.close();
  Serial.println( "File content correct." );
  return true;
}

// Read test
bool ReadTest()
{
  float s;
  uint64_t t, m;
  uint64_t maxLatency;
  uint64_t minLatency;
  uint64_t totalLatency;
  uint32_t avgLatency;
  bool skipLatency;
  uint32_t n = FILE_SIZE / BUF_SIZE;

  // Open fie for reading
  file = SD.open( "/bench.dat", FILE_READ );
  if( !file )
  {
    Serial.println( "File open failed." );
    return false;
  }

  Serial.println( "Starting read test, please wait." );
  Serial.println( "read speed and latency" );
  Serial.println( "speed,max,min,avg" );
  Serial.println( "KB/Sec,usec,usec,usec" );

  for( uint8_t nTest = 0; nTest < READ_COUNT; nTest++ )
  {
    file.seek( 0 );
          
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;
          
    t = micros();
    for( uint32_t i = 0; i < n; i++ )
    {
      buf[ BUF_SIZE - 1 ] = 0;
      m = micros();
      int32_t nr = file.read( buf, BUF_SIZE );
      if( nr != BUF_SIZE )
      {
        Serial.println( "read failed, bytes read: " + String( nr ) );
        file.close();
        return false;
      }
      m = micros() - m;
      totalLatency += m;
      if( skipLatency )
      {
        skipLatency = false;
      }
      else
      {
        if( maxLatency < m )
          maxLatency = m;
        if( minLatency > m )
          minLatency = m;
      }
    }
    t = micros() - t;
    s = file.size();
    
    if( SKIP_FIRST_LATENCY )
      avgLatency = (uint32_t)( totalLatency / ( n - 1 ) );
    else
      avgLatency = (uint32_t)( totalLatency / n );
      
    Serial.println( String( (uint32_t)( s * 1000 / t ) ) + ',' + String( (uint32_t)maxLatency ) + ',' + String( (uint32_t)minLatency ) + ',' + String( avgLatency ) );
  }

  file.close();
  Serial.println( "Read test finished successfully." );
  return true;
}