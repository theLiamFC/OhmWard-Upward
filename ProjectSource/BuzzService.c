/****************************************************************************
Module
    BuzzService.c

Revision
    1.0.1

Description
    This is a source file to control piezo buzzer and vibration motor
    sequences during gameplay.

Notes

History
When           Who     What/Why
-------------- ---     --------
01/16/12 09:58 jec      began conversion from BuzzFSM.c
****************************************************************************/

/*----------------------------- Include Files -----------------------------*/
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "BuzzService.h"
#include "PWM_PIC32.h"
#include "dbprintf.h"

/*----------------------------- Module Defines ----------------------------*/
#define VOLUME 10
#define VIB_MOTOR  LATBbits.LATB15

/*---------------------------- Module Functions ---------------------------*/
void SoundBuzzer(uint32_t freq, uint32_t duration);

/*---------------------------- Module Variables ---------------------------*/
static uint8_t MyPriority;
static uint8_t BuzzCount = 0;
static uint32_t BuzzTime = 0;
static uint8_t BuzzInterval = 15;
static uint8_t BuzzStep = 50;
static uint8_t BuzzType = 0;
static uint32_t BuzzFreq = 1000;
static uint8_t BlackHoleCycle = 0;
static uint8_t TuneCycle = 0;

// tune to play at the start of the game
uint16_t start_tune_frequencies[] = {
    1300,  // First note (1300 Hz)
    1500,  // Second note (1500 Hz)
    1800,  // Third note (1800 Hz)
    2200,  // Fourth note (2200 Hz)
    2500,  // Fifth note (2500 Hz)
    2200,  // Sixth note (2200 Hz)
    1800,  // Seventh note (1800 Hz)
    1500   // Eighth note (1500 Hz)
};

// tune to play at the end of the game
uint16_t end_tune_frequencies[] = {
    2000,  // First note (2000 Hz)
    1800,  // Second note (1800 Hz)
    1600,  // Third note (1600 Hz)
    1400,  // Fourth note (1400 Hz)
    1200,  // Fifth note (1200 Hz)
    1500,  // Sixth note (1500 Hz - a small rise for contrast)
    1000,  // Seventh note (1000 Hz)
    1200,  // Eighth note (1200 Hz - small rise again)
    800,   // Ninth note (800 Hz)
    600,   // Tenth note (600 Hz)
    400,   // Eleventh note (400 Hz)
    300,   // Twelfth note (300 Hz)
    200    // Thirteenth note (200 Hz) - final low note
};

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
Function
    InitBuzzService

Parameters
    uint8_t : the priorty of this service

Returns
    bool, false if error in initialization, true otherwise

Description
    Saves away the priority, and does any
    other required initialization for this service
****************************************************************************/
bool InitBuzzService(uint8_t Priority)
{
    // Init PWM Channel 1
    PWMSetup_BasicConfig(2); // configure 1 pwm pin 
    PWMSetup_SetFreqOnTimer(80, _Timer2_); // frequency of timer (50Hz)
    PWMSetup_AssignChannelToTimer(1, _Timer2_); // sets 1 pwm pin to a timer name
    PWMSetup_MapChannelToOutputPin(1, PWM_RPB4); // sets channel 1 to a legal pin (need to change!!)
    PWMOperate_SetDutyOnChannel(0,1); // set volume to zero
    
    // Vibration motor
    TRISBbits.TRISB15 = 0;   //output
    
    ES_Event_t ThisEvent;

    MyPriority = Priority;
    /********************************************
    in here you write your initialization code
     *******************************************/
    // post the initial transition event
    ThisEvent.EventType = ES_INIT;
    if (ES_PostToService(MyPriority, ThisEvent) == true)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/****************************************************************************
Function
    PostBuzzService

Parameters
    EF_Event_t ThisEvent ,the event to post to the queue

Returns
    bool false if the Enqueue operation failed, true otherwise

Description
    Posts an event to this state machine's queue
****************************************************************************/
bool PostBuzzService(ES_Event_t ThisEvent)
{
    return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
Function
    RunBuzzService

Parameters
    ES_Event_t : the event to process

Returns
    ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

Description
    Handles various buzz events from GameService
****************************************************************************/
ES_Event_t RunBuzzService(ES_Event_t ThisEvent)
{
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
    
    switch (ThisEvent.EventType)
    {
        case ES_INIT:
        {

        }
        break;
        case ES_TIMEOUT:
        {
            if(ThisEvent.EventParam == 8){ //BUZZER_TIMER
                if (BuzzCount == 0){ // buzzer is over
                    PWMOperate_SetDutyOnChannel(0,1); // set volume to zero
                    BuzzType = 0;
                    
                    VIB_MOTOR = 0; // turn off vibration motors
                    
                    if (BlackHoleCycle > 0){ // if blackhole cycle is non zero restart
                        BlackHoleCycle--;

                        BuzzType = BlackHoleCycle % 2 + 1; // alternate arpeggio
                        BuzzCount = BuzzInterval;
                        BuzzTime = 500;
                        
                        VIB_MOTOR = 1; // turn on vibration motors
                        
                        SoundBuzzer(BuzzFreq, BuzzTime/BuzzInterval);
                    }
                } else if (BuzzType == 1){ // Arpeggio pitch up
                    BuzzFreq = BuzzFreq+BuzzStep;
                    SoundBuzzer(BuzzFreq, BuzzTime/BuzzInterval);
                    BuzzCount--;
                } else if (BuzzType == 2){ // Arpeggio pitch down
                    BuzzFreq = BuzzFreq-BuzzStep;
                    SoundBuzzer(BuzzFreq, BuzzTime/BuzzInterval);
                    
                    if (BuzzCount % 8 == 0){
                        VIB_MOTOR = 0;
                    } else if (BuzzCount % 8 == 4){
                        VIB_MOTOR = 1;
                    }
                    
                    BuzzCount--;
                } else if (BuzzType == 3){ // Play start tune
                    SoundBuzzer(start_tune_frequencies[8-BuzzCount], BuzzTime/BuzzInterval);
                    BuzzCount--;
                } else if (BuzzType == 4){ // Play end tune
                    SoundBuzzer(end_tune_frequencies[13-BuzzCount], BuzzTime/BuzzInterval);
                    BuzzCount--;
                } else { // Turn off
                    PWMOperate_SetDutyOnChannel(0,1); // set volume to zero
                    VIB_MOTOR = 0;
                }
            }
        }
        break;
        case ES_BUZZ:
        {
            DB_printf("ES_BUZZ\n");
            if(ThisEvent.EventParam == 1){ // planet
                BuzzCount = BuzzInterval;
                BuzzTime = 500;
                BuzzType = 1;
                BuzzFreq = 1000;
                BlackHoleCycle = 0;

                SoundBuzzer(BuzzFreq, BuzzTime/BuzzInterval);
                
                VIB_MOTOR = 1;
            } else if(ThisEvent.EventParam == 2){ // asteroid
                BuzzCount = BuzzInterval;
                BuzzTime = 500;
                BuzzType = 2;
                BuzzFreq = 800;
                BlackHoleCycle = 0;

                SoundBuzzer(BuzzFreq, BuzzTime/BuzzInterval);
                
                VIB_MOTOR = 1;
            } else if(ThisEvent.EventParam == 3){ // blackhole
                BuzzCount = BuzzInterval;
                BuzzTime = 100;
                BuzzFreq = 600;
                
                BlackHoleCycle = 4;
                BuzzType = BlackHoleCycle % 2 + 1;

                SoundBuzzer(BuzzFreq, BuzzTime/BuzzInterval);
                
                VIB_MOTOR = 1;
            } else if(ThisEvent.EventParam == 4){ // coin
                BuzzTime = 200;
                BuzzType = 0;
                BuzzFreq = 1200;

                SoundBuzzer(BuzzFreq, BuzzTime);
                
                VIB_MOTOR = 1;
            } else if(ThisEvent.EventParam == 5){ // start tune
                BuzzTime = 1600;
                BuzzType = 3;
                
                BuzzCount = 8;

                SoundBuzzer(start_tune_frequencies[12-BuzzCount], BuzzTime/BuzzInterval);
                BuzzCount--;
                
                VIB_MOTOR = 1;
            } else if(ThisEvent.EventParam == 6){ // end tune
                BuzzTime = 2400;
                BuzzType = 4;
                
                BuzzCount = 12;

                SoundBuzzer(end_tune_frequencies[13-BuzzCount], BuzzTime/BuzzInterval);
                BuzzCount--;
                
                VIB_MOTOR = 1;
            }
        }
        break;
        default:
        {}
        break;
    }

    return ReturnEvent;
}

/***************************************************************************
 private functions
 ***************************************************************************/
void SoundBuzzer(uint32_t freq, uint32_t duration){
    PWMSetup_SetFreqOnTimer(freq, _Timer2_);
    PWMOperate_SetDutyOnChannel(VOLUME,1);
    ES_Timer_InitTimer(BUZZER_TIMER, duration);
}

/*------------------------------ End of file ------------------------------*/

