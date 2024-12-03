/****************************************************************************
Module
    ShiftService.c

Description
    This is a source file to control all LEDs by way of shift register updates.
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
/* include header files for this state machine as well as any machines at the
   next lower level in the hierarchy that are sub-machines to this machine
*/

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ShiftService.h"
#include "LEDService.h"
#include "PIC32_AD_Lib.h"
#include "PIC32_SPI_HAL.h"
#include "GameService.h"
#include "PWM_PIC32.h"
#include "ES_DeferRecall.h"

#include "dbprintf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*----------------------------- Module Defines ----------------------------*/
#define ENTER_POST     ((MyPriority<<3)|0)
#define ENTER_RUN      ((MyPriority<<3)|1)
#define ENTER_TIMEOUT  ((MyPriority<<3)|2)

// Shift register
#define DATA    LATBbits.LATB11
#define CLK     LATBbits.LATB12
#define OUTPUT  LATBbits.LATB13
#define NUM_SHIFT 13 // number of outputs used on shift registers

/*---------------------------- Module Variables ---------------------------*/
// State Machine Variables
static uint8_t MyPriority;
static ShiftState_t CurrentState;
static ShiftState_t NextState;

// Shift register variables
static uint8_t ShiftsRemaining = NUM_SHIFT;
static uint16_t ShiftInterval = 1; // keep as low as possible
extern uint8_t ShiftRegisterVals[13]; // extern from GameService

// Deferral queue variables
static ES_Event_t DeferralQueue[5];

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
Function
    InitShiftService

Parameters
    uint8_t : the priorty of this service

Returns
    bool, false if error in initialization, true otherwise

Description
    Saves away the priority, and does any
    other required initialization for this service
Notes

Author
    J. Edward Carryer, 01/16/12, 10:00
****************************************************************************/
bool InitShiftService(uint8_t Priority)
{

    // Init the shift register
    TRISBbits.TRISB11 = 0;   //output
    TRISBbits.TRISB12 = 0;   //output
    TRISBbits.TRISB13 = 0;   //output
    OUTPUT = 1;
    
    // State machine vars
    ES_Event_t ThisEvent;
    NextState = Waiting2Coins_P;
    MyPriority = Priority;

    DB_printf("InitShiftService\n");
    
    ES_InitDeferralQueueWith(DeferralQueue, ARRAY_SIZE(DeferralQueue));
    
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
    PostShiftService

Parameters
    EF_Event_t ThisEvent ,the event to post to the queue

Returns
    bool false if the Enqueue operation failed, true otherwise

Description
    Posts an event to this state machine's queue
****************************************************************************/
bool PostShiftService(ES_Event_t ThisEvent)
{
    return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
Function
    RunShiftService

Parameters
    ES_Event_t : the event to process

Returns
    ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

Description
    Waits for shift event and then updates the shift registers via
    clock, data line, and output latch sequence.
****************************************************************************/
ES_Event_t RunShiftService(ES_Event_t ThisEvent)
{
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
    
    CurrentState = NextState;
    
    switch (CurrentState)
    {
        case Waiting:
        {
            if (ThisEvent.EventType == ES_INIT){
                // Reset LEDs to initial state (all off)
                uint8_t newVals[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                memcpy(ShiftRegisterVals, newVals, sizeof(ShiftRegisterVals));

                // Post shift update event
                static ES_Event_t ShiftEvent;
                ShiftEvent.EventType = ES_UPDATE_SHIFT;
                ShiftEvent.EventParam = MyPriority;
                PostShiftService(ShiftEvent);
            }
            else if (ThisEvent.EventType == ES_UPDATE_SHIFT){
                OUTPUT = 1; // turn off output
                NextState = UpdatingShift_P;
                ES_Timer_InitTimer(SHIFT_TIMER, ShiftInterval);
            }
        }
        break;
        case UpdatingShift:
        {
            static uint8_t ShiftStep = 0;
            static bool LatchHi = true;

            if(ThisEvent.EventParam == 9 && ThisEvent.EventType == ES_TIMEOUT){
                if (ShiftsRemaining > 0){ // still shifting bits
                    if (ShiftStep == 0){ // setting data line
                        DATA = ShiftRegisterVals[ShiftsRemaining-1];
                        ES_Timer_InitTimer(SHIFT_TIMER, ShiftInterval);
                        ShiftStep++;
                    } else if (ShiftStep == 1){ // setting clock high
                        CLK = 1;
                        ES_Timer_InitTimer(SHIFT_TIMER, ShiftInterval);
                        ShiftStep++;
                    } else if (ShiftStep == 2){ // setting clock low
                        CLK = 0;
                        ES_Timer_InitTimer(SHIFT_TIMER, ShiftInterval);
                        ShiftStep = 0;
                        ShiftsRemaining--;
                    }
                } else  if (LatchHi){ // shift complete, pulse output latch
                    OUTPUT = 0;
                    ES_Timer_InitTimer(SHIFT_TIMER, ShiftInterval);
                    LatchHi = false;
                } else { // reset output latch
                    OUTPUT = 1;
                    LatchHi = true;
                    NextState = GameOn_P;
                    ShiftsRemaining = NUM_SHIFT;
                }
            }
        }
        break;
        default:
        {}
        break;
    }

    return ReturnEvent;
}

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/

