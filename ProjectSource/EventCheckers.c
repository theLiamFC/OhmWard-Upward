/****************************************************************************
 Module
   EventCheckers.c

 Revision
   1.0.1

 Description
   This is the sample for writing event checkers along with the event
   checkers used in the basic framework test harness.

 Notes
   Note the use of static variables in sample event checker to detect
   ONLY transitions.

 History
 When           Who     What/Why
 -------------- ---     --------
 08/06/13 13:36 jec     initial version
****************************************************************************/

// this will pull in the symbolic definitions for events, which we will want
// to post in response to detecting events
#include "ES_Configure.h"
// This gets us the prototype for ES_PostAll
#include "ES_Framework.h"
// this will get us the structure definition for events, which we will need
// in order to post events in response to detecting events
#include "ES_Events.h"
// if you want to use distribution lists then you need those function
// definitions too.
#include "ES_PostList.h"
// This include will pull in all of the headers from the service modules
// providing the prototypes for all of the post functions
#include "ES_ServiceHeaders.h"
// this test harness for the framework references the serial routines that
// are defined in ES_Port.c
#include "ES_Port.h"
// include our own prototypes to insure consistency between header &
// actual functionsdefinition
#include "EventCheckers.h"
#include "dbprintf.h"
#include "PWM_PIC32.h"

// This is the event checking function sample. It is not intended to be
// included in the module. It is only here as a sample to guide you in writing
// your own event checkers
#if 1
/****************************************************************************
 Function
   Check4Lock
 Parameters
   None
 Returns
   bool: true if a new event was detected
 Description
   Sample event checker grabbed from the simple lock state machine example
 Notes
   will not compile, sample only
 Author
   J. Edward Carryer, 08/06/13, 13:48
****************************************************************************/
#endif
/****************************************************************************
 Function
   Check4Keystroke
 Parameters
   None
 Returns
   bool: true if a new key was detected & posted
 Description
   checks to see if a new key from the keyboard is detected and, if so,
   retrieves the key and posts an ES_NewKey event to TestHarnessService0
 Notes
   The functions that actually check the serial hardware for characters
   and retrieve them are assumed to be in ES_Port.c
   Since we always retrieve the keystroke when we detect it, thus clearing the
   hardware flag that indicates that a new key is ready this event checker
   will only generate events on the arrival of new characters, even though we
   do not internally keep track of the last keystroke that we retrieved.
 Author
   J. Edward Carryer, 08/06/13, 13:48
****************************************************************************/
bool Check4Keystroke(void)
{
  if (IsNewKeyReady())   // new key waiting?
  {
    ES_Event_t ThisEvent;
    ThisEvent.EventType   = ES_NEW_KEY;
    ThisEvent.EventParam  = GetNewKey();
    ES_PostAll(ThisEvent);
    return true;
  }
  return false;
}


#define NUM_HALL_EFFECT 6
#define FIRST_HALL_PIN 5
#define LAST_HALL_PIN 10

#define HALL_PIN_HI 1
#define HALL_PIN_LO 0

static ES_EventType_t HallEffectEvents[] = {ES_PLANET_HIT,
                                            ES_PLANET_HIT,
                                            ES_PLANET_HIT,
                                            ES_PLANET_HIT,
                                            ES_ASTEROID_HIT,
                                            ES_BLACKHOLE_HIT};

static uint8_t LastHallEffectVals[NUM_HALL_EFFECT];
bool FirstRun = true;

static uint8_t HallEffectVal[NUM_HALL_EFFECT];

// BUG should init last vals to PORT read
bool CheckHallEffect(void)
{
    if (FirstRun){
        LastHallEffectVals[0] = PORTBbits.RB5; // planet 1
        LastHallEffectVals[1] = PORTBbits.RB8; // planet 2
        LastHallEffectVals[2] = PORTAbits.RA2; // planet 3
        LastHallEffectVals[3] = PORTAbits.RA3; // planet 4
        LastHallEffectVals[4] = PORTBbits.RB9; // asteroid
        LastHallEffectVals[5] = PORTBbits.RB10; // black hole
        FirstRun = false;
    }
    
    bool ReturnVal = false;
    HallEffectVal[0] = PORTBbits.RB5; // planet 1
    HallEffectVal[1] = PORTBbits.RB8; // planet 2
    HallEffectVal[2] = PORTAbits.RA2; // planet 3
    HallEffectVal[3] = PORTAbits.RA3; // planet 4
    HallEffectVal[4] = PORTBbits.RB9; // asteroid
    HallEffectVal[5] = PORTBbits.RB10; // black hole
            
    for (uint8_t i = 0; i <= NUM_HALL_EFFECT; i++) {        
        if ((HallEffectVal[i] != LastHallEffectVals[i]) && (HallEffectVal[i] == HALL_PIN_LO)) // event detected, so post detected event
        {
            DB_printf("HALL EFFECT EVENT\n"); 
            ES_Event_t ThisEvent;
            ThisEvent.EventType = HallEffectEvents[i];
            ThisEvent.EventParam = i;
            ES_PostAll(ThisEvent);
            ReturnVal = true;
        }
        LastHallEffectVals[i] = HallEffectVal[i];
    }

    return ReturnVal;
}

static uint8_t CurrentVal; // black hole
static uint8_t LastVal = 0; // black hole

bool CheckIRSensor(void)
{
    bool ReturnVal = false;
    static uint8_t CurrentVal; // black hole
    static uint8_t LastVal = 1; // black hole
    CurrentVal = PORTBbits.RB2; // black hole
            
    if ((CurrentVal != LastVal) && (CurrentVal == 0)) // event detected, so post detected event
    {
        ES_Event_t ThisEvent;
        ThisEvent.EventType = ES_COIN_INSERT;
        ES_PostAll(ThisEvent);
        ReturnVal = true;
    }
    LastVal = CurrentVal;

    return ReturnVal;
}


bool CheckPotSensor(void)
{   
    bool ReturnVal = false;
    static uint32_t CurrentVal;
    static uint32_t LastVal = 0;
    static bool FirstTrigger = true;
    
    uint32_t PotNum[1];
    ADC_MultiRead(PotNum);
    CurrentVal = PotNum[0];
    
    if (!FirstTrigger){
        if ((CurrentVal != LastVal) && (abs(CurrentVal - LastVal) > 10)) // event detected, so post detected event
        {      
            ES_Event_t ThisEvent;
            ThisEvent.EventType = ES_NEW_POT;
            int16_t Val = 100 * (CurrentVal) / 1023;
            if (Val <= 0){
                Val = 1;
            } else if (Val >= 100){
                Val = 100;
            }
            ThisEvent.EventParam = Val;
            ES_PostAll(ThisEvent);
            ReturnVal = true;
            LastVal = CurrentVal;
        }
    }
    else {
        FirstTrigger = false;
        LastVal = CurrentVal;
    }

    return ReturnVal;
}