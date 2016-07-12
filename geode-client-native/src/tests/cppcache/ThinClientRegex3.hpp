/*=========================================================================
 * Copyright (c) 2010-2014 Pivotal Software, Inc. All Rights Reserved.
 * This product is protected by U.S. and international copyright
 * and intellectual property laws. Pivotal products are covered by
 * one or more patents listed at http://www.pivotal.io/patents.
 *=========================================================================
 */
#include "fw_dunit.hpp"
#include <gfcpp/GemfireCppCache.hpp>
#include <ace/OS.h>
#include <ace/High_Res_Timer.h>
#include <string>

#define ROOT_NAME "Notifications"
#define ROOT_SCOPE DISTRIBUTED_ACK

#include "CacheHelper.hpp"

using namespace gemfire;
using namespace test;

#define CLIENT1 s1p1
#define CLIENT2 s1p2
#define SERVER1 s2p1

CacheHelper* cacheHelper = NULL;
bool isLocalServer = false;
const char * endPoints = CacheHelper::getTcrEndpoints(isLocalServer, 1);

static bool isLocator = false;
static int numberOfLocators = 0;
const char* locatorsG = CacheHelper::getLocatorHostPort( isLocator, numberOfLocators);
#include "LocatorHelper.hpp"
void initClient( const bool isthinClient )
{
  if ( cacheHelper == NULL ) {
    cacheHelper = new CacheHelper(isthinClient);
  }
  ASSERT( cacheHelper, "Failed to create a CacheHelper client instance." );
}
void cleanProc()
{
  if ( cacheHelper != NULL ) {
    delete cacheHelper;
  cacheHelper = NULL;
  }
}

CacheHelper * getHelper()
{
  ASSERT( cacheHelper != NULL, "No cacheHelper initialized." );
  return cacheHelper;
}

void _verifyEntry( const char * name, const char * key, const char * val, bool noKey, bool isCreated = false )
{
  // Verify key and value exist in this region, in this process.
  const char * value = ( val == 0 ) ? "" : val;
  char * buf = (char *)malloc( 1024 + strlen( key ) + strlen( value ));
  ASSERT( buf, "Unable to malloc buffer for logging." );
  if (!isCreated) {
    if ( noKey )
      sprintf( buf, "Verify key %s does not exist in region %s", key, name );
    else if ( val == 0 )
      sprintf( buf, "Verify value for key %s does not exist in region %s", key, name );
    else
      sprintf( buf, "Verify value for key %s is: %s in region %s", key, value, name );
    LOG( buf );
  }
  free(buf);

  RegionPtr regPtr = getHelper()->getRegion( name );
  ASSERT( regPtr != NULLPTR, "Region not found." );

  CacheableKeyPtr keyPtr = createKey( key );

  // if the region is no ack, then we may need to wait...
  if (!isCreated) {
    if ( noKey == false ) { // need to find the key!
      ASSERT( regPtr->containsKey( keyPtr ), "Key not found in region." );
    }
    if (val != NULL) { // need to have a value!
      ASSERT( regPtr->containsValueForKey( keyPtr ), "Value not found in region." );
    }
  }

  // loop up to MAX times, testing condition
  uint32_t MAX = 100;
  // ARB: changed sleep from 10 ms
  uint32_t SLEEP = 1000; // milliseconds
  uint32_t containsKeyCnt = 0;
  uint32_t containsValueCnt = 0;
  uint32_t testValueCnt = 0;

  for (int i = MAX; i >= 0; i--)
  {
    if (isCreated) {
      if (!regPtr->containsKey(keyPtr))
        containsKeyCnt++;
      else
        break;
      ASSERT( containsKeyCnt < MAX, "Key has not been created in region." );
    }
    else {
      if ( noKey ) {
        if (regPtr->containsKey( keyPtr ))
          containsKeyCnt++;
        else
          break;
        ASSERT( containsKeyCnt < MAX, "Key found in region." );
      }
      if (val == NULL) {
        if ( regPtr->containsValueForKey( keyPtr ) )
          containsValueCnt++;
        else
          break;
        ASSERT( containsValueCnt < MAX, "Value found in region." );
      }

      if ( val != NULL) {
        CacheableStringPtr checkPtr = dynCast<CacheableStringPtr>( regPtr->get( keyPtr ) );

        ASSERT( checkPtr != NULLPTR, "Value Ptr should not be null." );
        char buf[1024];
        sprintf( buf, "In verify loop, get returned %s for key %s", checkPtr->asChar(), key );
        LOG( buf );
        if ( strcmp( checkPtr->asChar(), value ) != 0 ){
          testValueCnt++;
        }else{
          break;
        }
        ASSERT( testValueCnt < MAX, "Incorrect value found." );
      }
    }
    dunit::sleep( SLEEP );
  }
}

#define verifyInvalid(x,y) _verifyInvalid( x, y, __LINE__ )

void _verifyInvalid( const char * name, const char * key, int line )
{
  char logmsg[1024];
  sprintf( logmsg, "verifyInvalid() called from %d.\n", line );
  LOG( logmsg );
  _verifyEntry(name, key, 0, false );
  LOG( "Entry invalidated." );
}

#define verifyDestroyed(x,y) _verifyDestroyed( x, y, __LINE__ )

void _verifyDestroyed( const char * name, const char * key, int line )
{
  char logmsg[1024];
  sprintf( logmsg, "verifyDestroyed() called from %d.\n", line );
  LOG( logmsg );
  _verifyEntry(name, key, 0, true );
  LOG( "Entry destroyed." );
}

#define verifyEntry(x,y,z) _verifyEntry( x, y, z, __LINE__ )

void _verifyEntry( const char * name, const char * key, const char * val, int line )
{
  char logmsg[1024];
  sprintf( logmsg, "verifyEntry() called from %d.\n", line );
  LOG( logmsg );
  _verifyEntry(name, key, val, false );
  LOG( "Entry verified." );
}

#define verifyCreated(x,y) _verifyCreated( x, y, __LINE__ )

void _verifyCreated(const char *name, const char *key, int line)
{
  char logmsg[1024];
  sprintf( logmsg, "verifyCreated() called from %d.\n", line );
  LOG( logmsg );
  _verifyEntry(name, key, NULL, false,true );
  LOG( "Entry created." );
}

void createRegion( const char * name, bool ackMode, const char * endpoints ,bool clientNotificationEnabled = false)
{
  LOG( "createRegion() entered." );
  fprintf( stdout, "Creating region --  %s  ackMode is %d\n", name, ackMode );
  fflush( stdout );
  RegionPtr regPtr = getHelper()->createRegion( name, ackMode, true,
      NULLPTR, endpoints, clientNotificationEnabled );
  ASSERT( regPtr != NULLPTR, "Failed to create region." );
  LOG( "Region created." );
}
void createPooledRegion( const char * name, bool ackMode, const char * endpoints, const char* locators,const char* poolname, bool clientNotificationEnabled = false, bool cachingEnable = true)
{
  LOG( "createRegion_Pool() entered." );
  fprintf( stdout, "Creating region --  %s  ackMode is %d\n", name, ackMode );
  fflush( stdout );
  RegionPtr regPtr = getHelper()->createPooledRegion(name,ackMode,endpoints, locators, poolname ,cachingEnable, clientNotificationEnabled);
  ASSERT( regPtr != NULLPTR, "Failed to create region." );
  LOG( "Pooled Region created." );
}
void createEntry( const char * name, const char * key, const char * value = NULL )
{
  LOG( "createEntry() entered." );
  fprintf( stdout, "Creating entry -- key: %s  value: %s in region %s\n", key, value, name );
  fflush( stdout );
  // Create entry, verify entry is correct
  CacheableKeyPtr keyPtr = createKey( key );
  if (value == NULL) value = "";
  CacheableStringPtr valPtr = CacheableString::create( value );

  RegionPtr regPtr = getHelper()->getRegion( name );
  ASSERT( regPtr != NULLPTR, "Region not found." );

  ASSERT( !regPtr->containsKey( keyPtr ), "Key should not have been found in region." );
  ASSERT( !regPtr->containsValueForKey( keyPtr ), "Value should not have been found in region." );

  regPtr->create( keyPtr, valPtr );
  //regPtr->put( keyPtr, valPtr );
  LOG( "Created entry." );

  verifyEntry( name, key, value );
  LOG( "Entry created." );
}

void updateEntry( const char * name, const char * key, const char * value )
{
  LOG( "updateEntry() entered." );
  fprintf( stdout, "Updating entry -- key: %s  value: %s in region %s\n", key, value, name );
  fflush( stdout );
  // Update entry, verify entry is correct
  CacheableKeyPtr keyPtr = createKey( key );
  CacheableStringPtr valPtr = CacheableString::create( value );

  RegionPtr regPtr = getHelper()->getRegion( name );
  ASSERT( regPtr != NULLPTR, "Region not found." );

  ASSERT( regPtr->containsKey( keyPtr ), "Key should have been found in region." );
  ASSERT( regPtr->containsValueForKey( keyPtr ), "Value should have been found in region." );

  regPtr->put( keyPtr, valPtr );
  LOG( "Put entry." );

  verifyEntry( name, key, value );
  LOG( "Entry updated." );
}

void doNetsearch( const char * name, const char * key, const char * value )
{
  LOG( "doNetsearch() entered." );
  fprintf( stdout, "Netsearching for entry -- key: %s  expecting value: %s in region %s\n", key, value, name );
  fflush( stdout );
  // Get entry created in Process A, verify entry is correct
  CacheableKeyPtr keyPtr = CacheableKey::create( key );

  RegionPtr regPtr = getHelper()->getRegion( name );
  fprintf( stdout, "netsearch  region %s\n", regPtr->getName() );
  fflush( stdout );
  ASSERT( regPtr != NULLPTR, "Region not found." );

  ASSERT( !regPtr->containsKey( keyPtr ), "Key should not have been found in region." );
  ASSERT( !regPtr->containsValueForKey( keyPtr ), "Value should not have been found in region." );

  CacheableStringPtr checkPtr = dynCast<CacheableStringPtr>( regPtr->get( keyPtr) ); // force a netsearch

  if(checkPtr != NULLPTR){
    LOG("checkPtr is not null");
      char buf[1024];
      sprintf( buf, "In net search, get returned %s for key %s", checkPtr->asChar(), key );
      LOG( buf );
  }else{
    LOG("checkPtr is NULL");
  }
  verifyEntry( name, key, value );
  LOG( "Netsearch complete." );
}

//const char * testregex[] = { "Key-*1", "Key-*2", "Key-*3", "Key-*4" };
const char * keys[] = { "Key-1", "Key-2", "Key-3", "Key-4" };
const char * regionNames[] = { "DistRegionAck", "DistRegionNoAck" };
const char * vals[] = { "Value-1", "Value-2", "Value-3", "Value-4" };
const char * nvals[] = { "New Value-1", "New Value-2", "New Value-3", "New Value-4" };

const bool USE_ACK = true;
const bool NO_ACK = false;

DUNIT_TASK_DEFINITION(SERVER1, CreateServer1)
{
  LOG("Starting SERVER1...");
  if ( isLocalServer ) CacheHelper::initServer( 1, "cacheserver_notify_subscription.xml" );
  LOG("SERVER1 started");
}
END_TASK_DEFINITION
DUNIT_TASK_DEFINITION(CLIENT1, StepOne)
{
  initClient(true);
  createRegion( regionNames[0], USE_ACK, endPoints, true);
  createRegion( regionNames[1], NO_ACK, endPoints, true);
  LOG( "StepOne complete." );
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT1, StepOne_Pool_Locator)
{
  initClient(true);
  createPooledRegion( regionNames[0], USE_ACK, NULL, locatorsG, "__TESTPOOL1_", true );
  createPooledRegion( regionNames[1], NO_ACK, NULL, locatorsG, "__TESTPOOL1_", true );
  LOG( "StepOne_Pool complete." );
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT1, StepOne_Pool_EndPoint)
{
  initClient(true);
  createPooledRegion( regionNames[0], USE_ACK, endPoints, NULL, "__TESTPOOL1_", true );
  createPooledRegion( regionNames[1], NO_ACK, endPoints, NULL, "__TESTPOOL1_", true );
  LOG( "StepOne_Pool complete." );
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT2, StepTwo)
{
  initClient(true);
  createRegion( regionNames[0], USE_ACK, endPoints,true);
  createRegion( regionNames[1], NO_ACK, endPoints,true);
  LOG( "StepTwo complete." );
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT2, StepTwo_Pool_Locator)
{
  initClient(true);
  createPooledRegion( regionNames[0], USE_ACK, NULL, locatorsG, "__TESTPOOL1_", true );
  createPooledRegion( regionNames[1], NO_ACK, NULL, locatorsG, "__TESTPOOL1_", true );
  LOG( "StepTwo_Pool complete." );
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT2, StepTwo_Pool_EndPoint)
{
  initClient(true);
  createPooledRegion( regionNames[0], USE_ACK, endPoints, NULL, "__TESTPOOL1_", true );
  createPooledRegion( regionNames[1], NO_ACK, endPoints, NULL, "__TESTPOOL1_", true );
  LOG( "StepTwo_Pool complete." );
}
END_TASK_DEFINITION


DUNIT_TASK_DEFINITION(CLIENT1, StepThree)
{
  RegionPtr regPtr0 = getHelper()->getRegion(regionNames[0]);
  RegionPtr regPtr1 = getHelper()->getRegion(regionNames[1]);

  try
  {
  LOG( "Registering regex: a*" );
  regPtr0->registerRegex("a*");

  LOG( "Registering regex: *[*2-[" );
  regPtr1->registerRegex("*[*2-[");

  FAIL("Did not get expected exception!");
  }
  catch(Exception & excp)
  {
    LOG(excp.getMessage());
  }

  LOG( "StepThree complete." );
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT2, StepFour)
{
  RegionPtr regPtr0 = getHelper()->getRegion(regionNames[0]);
  RegionPtr regPtr1 = getHelper()->getRegion(regionNames[1]);

  try
  {
    LOG( "Registering empty regex" );
    regPtr0->registerRegex("");
    FAIL("Did not get expected exception!");
  }
  catch(Exception & excp)
  {
    LOG(excp.getMessage());
  }

  try
  {
    LOG( "Registering null regex" );
    regPtr1->registerRegex(NULL);
    FAIL("Did not get expected exception!");
  }
  catch(Exception & excp)
  {
    LOG(excp.getMessage());
  }

  try
  {
    LOG( "Unregistering non-existent regex" );
    regPtr1->unregisterRegex("Non*Existent*Regex");
    FAIL("Did not get expected exception!");
  }
  catch(Exception & excp)
  {
    LOG(excp.getMessage());
  }

  LOG( "StepFour complete." );
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT1,CloseCache1)
{
  LOG("cleanProc 1...");
  cleanProc();
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(CLIENT2,CloseCache2)
{
  LOG("cleanProc 2...");
  cleanProc();
}
END_TASK_DEFINITION

DUNIT_TASK_DEFINITION(SERVER1,CloseServer1)
{
  LOG("closing Server1...");
  if ( isLocalServer ) {
    CacheHelper::closeServer( 1 );
    LOG("SERVER1 stopped");
  }
}
END_TASK_DEFINITION

void runThinClientRegex3(bool poolConfig, bool withLocator = true)
{
  if( poolConfig && isLocator )
  {
    CALL_TASK(CreateLocator1);
    CALL_TASK(CreateServer1_With_Locator_XML)
  }
  else
  {
    CALL_TASK( CreateServer1 );
  }
  if( !poolConfig )
  {
    CALL_TASK(StepOne);
    CALL_TASK(StepTwo);
  }
  else if( withLocator == true)
  {
    CALL_TASK(StepOne_Pool_Locator);
    CALL_TASK(StepTwo_Pool_Locator);
  }
  else
  {
    CALL_TASK(StepOne_Pool_EndPoint);
    CALL_TASK(StepTwo_Pool_EndPoint);
  }
  CALL_TASK(StepThree);
  CALL_TASK(StepFour);
  CALL_TASK(CloseCache1);
  CALL_TASK(CloseCache2);
  CALL_TASK(CloseServer1);
  if( poolConfig && isLocator )
  {
    CALL_TASK(CloseLocator1);
  }
}