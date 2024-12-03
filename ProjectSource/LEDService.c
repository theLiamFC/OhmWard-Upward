/****************************************************************************
 Module
   LED Service.c
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
// This module
#include "../ProjectHeaders/LEDService.h"

// debugging printf()

// Hardware
#include <xc.h>
//#include <proc/p32mx170f256b.h>

// Event & Services Framework
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_DeferRecall.h"
#include "ES_Port.h"
#include "terminal.h"
#include "dbprintf.h"
#include "DM_Display.h"
#include "PIC32_SPI_HAL.h"
#include <string.h>


/*---------------------------- Module Variables ---------------------------*/
static uint8_t MyPriority;
static LED_State_t CurrentState;
static ES_Event_t DeferralQueue[3 + 1];

static char *ScrollString = NULL;
static uint8_t ScrollIndex = 0;
static uint8_t ScrollLen = 0;

/*------------------------------ Module Code ------------------------------*/

/****************************************************************************
Function
    InitLEDService

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
bool InitLEDService(uint8_t Priority)
{
    DB_printf("InitLEDService\n");
    ES_Event_t ThisEvent;
    clrScrn();

    CurrentState = IDLE;

    SPISetup_BasicConfig(SPI_SPI1);
    SPISetup_SetLeader(SPI_SPI1,SPI_SMP_MID);//Set master and SMP
    SPISetup_SetBitTime(SPI_SPI1, 10000); //Set BRG = 100kHz
    SPISetup_MapSSOutput(SPI_SPI1, SPI_RPA0); //Map SS output
    SPISetup_MapSDOutput(SPI_SPI1, SPI_RPA1); //Map SD output
    SPISetup_SetClockIdleState(SPI_SPI1, SPI_CLK_HI); //Set CKP = 1
    SPISetup_SetActiveEdge(SPI_SPI1, SPI_SECOND_EDGE); //Set CKE = 0
    SPISetup_SetXferWidth(SPI_SPI1, SPI_16BIT); // 32 bits
    //No need to map SCK since only one pin

    SPI1BUF; //clear receive buffer

    SPISetEnhancedBuffer(SPI_SPI1, true); //Set enhanced buffeR

    SPISetup_EnableSPI(SPI_SPI1); //Clear ON bit

    IFS0CLR = _IFS0_INT4IF_MASK;
    while(false == DM_TakeInitDisplayStep()){}

    ES_InitDeferralQueueWith(DeferralQueue, ARRAY_SIZE(DeferralQueue));

    MyPriority = Priority;

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
    PostLEDService

Parameters
    ES_Event ThisEvent ,the event to post to the queue

Returns
    bool false if the Enqueue operation failed, true otherwise

Description
    Posts an event to this state machine's queue
****************************************************************************/
bool PostLEDService(ES_Event_t ThisEvent)
{
    return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
Function
    RunLEDService

Parameters
    ES_Event : the event to process

Returns
    ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise
****************************************************************************/
ES_Event_t RunLEDService(ES_Event_t ThisEvent)
{
    ES_Event_t ReturnEvent;
    ES_Event_t myEvent;
    ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
    static char DeferredChar = '1';

    #ifdef _INCLUDE_BYTE_DEBUG_
      _HW_ByteDebug_SetValueWithStrobe( ENTER_RUN );
    #endif  

    switch (CurrentState)
    {
        case IDLE:
        {
            if (ThisEvent.EventType == ES_TIMEOUT){
                if (ThisEvent.EventParam == 6){ // SCROLL_TIMER
                     // Ensure ScrollString is valid and not NULL
                    if (ScrollString != NULL) {
                        // Check if the current character is valid
                        if (ScrollIndex >= ScrollLen) {
                            ScrollIndex = 0;
                        }
                        // Scroll the current character (Display it)
                        DB_printf("SCROLL_TIMER: %c\n", ScrollString[ScrollIndex]);
                        DB_printf("SCROLL LENGTH: %d\n", ScrollLen);
                        DB_printf("SCROLL I: %d\n", ScrollIndex);

                        // Add character to display buffer
                        DM_ScrollDisplayBuffer(4, 2);  // Clear the previous buffer
                        DM_AddChar2DisplayBuffer(2, (unsigned char)ScrollString[ScrollIndex]);  // Display the character
                        ScrollIndex++;  // Move to the next character

                        // Update the state to UPDATING
                        myEvent.EventType = ES_ROWUPDATE;
                        CurrentState = UPDATING;
                        PostLEDService(myEvent);

                        // Restart the timer for the next character
                        ES_Timer_InitTimer(SCROLL_TIMER, 1200);  // Adjust scroll interval as need
                    } else {
                        DB_printf("Error: ScrollString is NULL\n");
                    }
                }
            }
            else if(ThisEvent.EventType == ES_SCROLL)
            {
                ES_Timer_StopTimer(SCROLL_TIMER);
                DB_printf("ES_SCROLL\n");
                ScrollString = ThisEvent.EventMessage;
                ScrollLen = strlen(ThisEvent.EventMessage);
                ScrollIndex = 0;
                ES_Timer_InitTimer(SCROLL_TIMER, 1200);
                DB_printf("ES_SCROLL OVER\n");
            }
            else if(ThisEvent.EventType == ES_NEW_WORD)
            {
                ES_Timer_StopTimer(SCROLL_TIMER);
                ScrollIndex = 0;
                
                uint8_t WordLength = strlen(ThisEvent.EventMessage);
                DM_ClearDisplayBuffer(ThisEvent.EventParam);
                for (uint8_t i = 0; i < WordLength; i++){
                    if (i > 0){
                        DM_ScrollDisplayBuffer(4, ThisEvent.EventParam);
                    }
                    DM_AddChar2DisplayBuffer((unsigned char)ThisEvent.EventMessage[i], ThisEvent.EventParam);
                }
                
                // Center word on display (assuming 4 matrices wide)
                DM_ScrollDisplayBuffer((32 - (WordLength*4))/2, ThisEvent.EventParam);
                
                myEvent.EventType = ES_ROWUPDATE;
                CurrentState = UPDATING;
                PostLEDService(myEvent);
            }
        }
        break;
        case UPDATING:
        {
            if (ThisEvent.EventType == ES_TIMEOUT || ThisEvent.EventType == ES_NEW_WORD || ThisEvent.EventType == ES_SCROLL )
            {
                // add event to the defferal queue
                if (ES_DeferEvent(DeferralQueue, ThisEvent)){
                }
            }
            else if (ThisEvent.EventType == ES_ROWUPDATE )
            {
                // continue updating display
                if(DM_TakeDisplayUpdateStep() == false)
                {
                    PostLEDService(ThisEvent);
                }
                else { // updating is complete, recall defferred events
                    CurrentState = IDLE;
                    if (true == ES_RecallEvents(MyPriority, DeferralQueue)){
                    }
                }
            }
        }
        default:
        {}
        break;
    }

    return ReturnEvent;
}

/*------------------------------ End of file ------------------------------*/

