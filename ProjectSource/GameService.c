/****************************************************************************
 Module
   GameService.c

 Revision
   1.0.1

 Description
   This is a source file to handle the majority of game logic for the Ohmward
   & Upward 218A arcade game.
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "GameService.h"
#include "LEDService.h"
#include "PIC32_AD_Lib.h"
#include "PIC32_SPI_HAL.h"
#include "ShiftService.h"
#include "BuzzService.h"
#include "PWM_PIC32.h"

#include "dbprintf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*----------------------------- Module Defines ----------------------------*/
#define ONE_SEC 1000
#define HALF_SEC (ONE_SEC / 2)
#define TWO_SEC (ONE_SEC * 2)
#define FIVE_SEC (ONE_SEC * 5)

#define ENTER_POST     ((MyPriority<<3)|0)
#define ENTER_RUN      ((MyPriority<<3)|1)
#define ENTER_TIMEOUT  ((MyPriority<<3)|2)

#define PLANET_LED 0
#define BLACKHOLE_LED 4
#define TIMING_LED 7
#define COIN_LED 5
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
void GetNewPlanet(void);
void UpdateScore(int16_t DeltaScore);
void UpdateDisplay(uint8_t WhichDisplay, char *Msg);
void GetDifficulty(uint8_t Level);

/*---------------------------- Module Variables ---------------------------*/
// State Machine Variables
static uint8_t MyPriority;
static GameState_t CurrentState;
static GameState_t NextState;

// Game Variables
uint8_t Countdown;
int8_t TargetPlanet;
int16_t Score;
bool BlackHole;
uint8_t ShiftRegisterVals[13];

// Timing Variables
bool AsteroidTimeout;
bool CoinTimeout;
static uint32_t GameInterval = 1000*10; // timer interval
static uint32_t UserInputTimeout = 1000*20; // timer interval
static uint32_t AsteroidDelay = 1000*1;
static uint32_t CoinDelay = 300;
static uint32_t GameOverTime = 1000*7; // time after game ends before restart
static uint32_t LevelDisplayTime = 1500;

// Difficulty Variables
static uint32_t PlanetSwitchTime = 1000*5; // timer interval
static uint16_t PlanetSwitchTime_MAX = 1000*5;
static uint16_t PlanetSwitchTime_MIN = 1000*1;
static uint8_t BlackHoleProb = 50; // % chance blackhole turns on
static uint32_t BlackHoleSampleTime = 1000*1; // timer interval
static uint8_t DifficultyLevel;

// Deferral Queue Variables
//static ES_Event_t DeferralQueue[3 + 1];

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitGameService

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
bool InitGameService(uint8_t Priority)
{
    // Init Hall Effect Inputs
    TRISBbits.TRISB5 = 1;   //B3 as input
    TRISBbits.TRISB8 = 1;   //B6 as input
    TRISAbits.TRISA2 = 1;   //A2 as input
    TRISAbits.TRISA3 = 1;   //A3 as input
    TRISBbits.TRISB9 = 1;   //B9 as input
    TRISBbits.TRISB10 = 1;   //B10 as input
    
    // Init IR sensor Input
    TRISBbits.TRISB2 = 1;   //B2 as input
    ANSELBbits.ANSB2 = 0;   //B2 as digital
    
    // Init Potentiometer Input
    TRISBbits.TRISB3 = 1;   //B3 as input
    ANSELBbits.ANSB3 = 1;   //B3 as analog
    ADC_ConfigAutoScan(BIT5HI);
    
    // Init Difficulty
    uint32_t PotNum[1];
    ADC_MultiRead(PotNum);
    int16_t Val = 100 * (1023-PotNum[0]) / 1023;
    if (Val <= 0){
        Val = 1;
    } else if (Val >= 100){
        Val = 100;
    }
    GetDifficulty(Val);
    
    MyPriority = Priority;

    // Init Game Variables
    Score = 0;
    BlackHole = false;
    AsteroidTimeout = false;
    CoinTimeout = false;
    Countdown = 6;
    TargetPlanet = -1;

    DB_printf("InitGameService\n");
    DB_printf("Press 'c' to insert a coin\n");
    DB_printf("Press 'p' for a planet hit\n");
    DB_printf("Press 'a' for a asteroid hit\n");
    DB_printf("Press 'b' for a blackhole hit\n");

    // Init State Machine
    NextState = Waiting2Coins;
    
    // Post the initial transition event
    ES_Event_t ThisEvent;
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
     PostTemplateService

 Parameters
     EF_Event_t ThisEvent ,the event to post to the queue

 Returns
     bool false if the Enqueue operation failed, true otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     J. Edward Carryer, 10/23/11, 19:25
****************************************************************************/
bool PostGameService(ES_Event_t ThisEvent)
{
  return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunTemplateService

 Parameters
   ES_Event_t : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   add your description here
 Notes

 Author
   J. Edward Carryer, 01/15/12, 15:23
****************************************************************************/
ES_Event_t RunGameService(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
  
  CurrentState = NextState;
  
  switch (CurrentState)
  {
    case Waiting2Coins:
    {
        DB_printf("Waiting2Coins\n");

        // Reset game Variables
        Score = 0;
        BlackHole = false;
        AsteroidTimeout = false;
        CoinTimeout = false;
        Countdown = 6;
        TargetPlanet = -1;

        if (ThisEvent.EventType == ES_INIT){
            UpdateDisplay(2, "2 CNS");
            UpdateDisplay(1, "INSERT");
        }
        else if (ThisEvent.EventType == ES_TIMEOUT){
            if (ThisEvent.EventParam == 11){ // countdown timer
                UpdateDisplay(2, "2 CNS");
                UpdateDisplay(1, "INSERT");
                
                // Reset LEDs to initial state
                uint8_t newVals[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                memcpy(ShiftRegisterVals, newVals, sizeof(ShiftRegisterVals));

                // Post LED update event
                static ES_Event_t ShiftEvent;
                ShiftEvent.EventType = ES_UPDATE_SHIFT;
                ShiftEvent.EventParam = MyPriority;
                PostShiftService(ShiftEvent);
            }
        }
        // If first coin was inserted proceed to next state
        else if (ThisEvent.EventType == ES_COIN_INSERT){
            //ScrollDisplay(2, "HELLO EMMA AND LIAM");
            UpdateDisplay(2, "1 CN");
            UpdateDisplay(1, "INSERT");
            
            ES_Event_t BuzzEvent;
            BuzzEvent.EventType = ES_BUZZ;
            BuzzEvent.EventParam = 4;
            PostBuzzService(BuzzEvent);
            
            // Update coin LED
            ShiftRegisterVals[COIN_LED] = 1;
            static ES_Event_t ShiftEvent;
            ShiftEvent.EventType = ES_UPDATE_SHIFT;
            ShiftEvent.EventParam = MyPriority;
            PostShiftService(ShiftEvent);
            
            // Coin delay
            CoinTimeout = true;
            ES_Timer_InitTimer(COIN_DELAY_TIMER, CoinDelay);
            
            // User Input timer
            ES_Timer_InitTimer(USER_INPUT_TIMER, UserInputTimeout);
            
            NextState = Waiting1Coins;
        } 
        else if (ThisEvent.EventType == ES_NEW_POT){
            char DifficultyStr[4];
            DifficultyLevel = ThisEvent.EventParam;
            sprintf(DifficultyStr, "%d", DifficultyLevel);
            UpdateDisplay(2, DifficultyStr);
            UpdateDisplay(1, "LEVEL");
            GetDifficulty(DifficultyLevel);

            ES_Timer_InitTimer(COUNTDOWN_TIMER, LevelDisplayTime);
            ES_Timer_InitTimer(USER_INPUT_TIMER, UserInputTimeout); //Restart user input timer
        }
    }
    break;
    case Waiting1Coins:
    {
        // If second coin was inserted proceed to next state
        if (ThisEvent.EventType == ES_COIN_INSERT && CoinTimeout == false){
            DB_printf("Starting the Game!!!\n");

            UpdateDisplay(2, "PLAY!");
            UpdateScore(0);
            
            ES_Event_t BuzzEvent;
            BuzzEvent.EventType = ES_BUZZ;
            BuzzEvent.EventParam = 5;
            PostBuzzService(BuzzEvent);
            
            // Reset LEDs to initial state
            uint8_t newVals[13] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};
            memcpy(ShiftRegisterVals, newVals, sizeof(ShiftRegisterVals));

            // Post LED update event
            static ES_Event_t ShiftEvent;
            ShiftEvent.EventType = ES_UPDATE_SHIFT;
            ShiftEvent.EventParam = MyPriority;
            PostShiftService(ShiftEvent);
            
            // update planet for start
            GetNewPlanet();

            // get difficulty
            DB_printf("Difficulty Level: %d\n",DifficultyLevel);

            // Start all game timers
            ES_Timer_InitTimer(PLANET_TIMER, PlanetSwitchTime);
            ES_Timer_InitTimer(BLACKHOLE_TIMER, BlackHoleSampleTime);
            ES_Timer_InitTimer(COUNTDOWN_TIMER, GameInterval);
            ES_Timer_InitTimer(USER_INPUT_TIMER, UserInputTimeout);

            NextState = GameOn;
        } else if (ThisEvent.EventType == ES_TIMEOUT){
            if(ThisEvent.EventParam == 7){ //COIN_DELAY_TIMER
                DB_printf("COIN_DELAY_TIMER\n");
                CoinTimeout = false; // turn off coin delay
            } else if (ThisEvent.EventParam == 11){ // COUNTDOWN TIMER
                UpdateDisplay(2, "1 CN");
                UpdateDisplay(1, "INSERT");
            } else if(ThisEvent.EventParam == 10){
                UpdateDisplay(2, "SLOW!");
                UpdateDisplay(1, "TOO");
                
                ES_Timer_InitTimer(COUNTDOWN_TIMER, LevelDisplayTime*1.5);
                
                NextState = Waiting2Coins;
            }
        } else if (ThisEvent.EventType == ES_NEW_POT){
            char DifficultyStr[4];
            DifficultyLevel = ThisEvent.EventParam;
            sprintf(DifficultyStr, "%d", DifficultyLevel);
            UpdateDisplay(2, DifficultyStr);
            UpdateDisplay(1, "LEVEL");
            GetDifficulty(DifficultyLevel);
            
            ES_Timer_InitTimer(COUNTDOWN_TIMER, LevelDisplayTime);
            ES_Timer_InitTimer(USER_INPUT_TIMER, UserInputTimeout); //Restart user input timer
        }
    }
    break;
    case GameOn:
    {
        // Serial print out
        DB_printf("---------------------------------\n");
        DB_printf("GameOn for %d more seconds\n",Countdown*10);
        DB_printf("Score: %d\n",Score);
        DB_printf("Target Planet: %d\n",TargetPlanet+1);
        DB_printf("BlackHole: %s\n",BlackHole ? "ON" : "OFF");
        DB_printf("---------------------------------\n");
        
        // Timeout cases
        if (ThisEvent.EventType == ES_TIMEOUT){
            if(ThisEvent.EventParam == 14){ //PLANET_TIMER
                GetNewPlanet();
                ES_Timer_InitTimer(PLANET_TIMER, PlanetSwitchTime); //Start planet timer
            }
            else if(ThisEvent.EventParam == 13){ //BLACKHOLE_TIMER
                if (BlackHole == false){ 
                    if ((rand() % 100) < BlackHoleProb){ // turn on Black Hole
                        BlackHole = true; //toggle
                        
                        // Update black hole LED
                        ShiftRegisterVals[BLACKHOLE_LED] = 1;
                        ES_Event_t myEvent;
                        myEvent.EventType = ES_UPDATE_SHIFT;
                        myEvent.EventParam = MyPriority;
                        PostShiftService(myEvent);
                    }
                } else { // Blackhole == true
                    if ((rand() % 100) < (100 - BlackHoleProb)){ // turn off Black Hole
                        BlackHole = false; //toggle
                        
                        // update black hole LED
                        ShiftRegisterVals[BLACKHOLE_LED] = 0;
                        ES_Event_t myEvent;
                        myEvent.EventType = ES_UPDATE_SHIFT;
                        myEvent.EventParam = MyPriority;
                        PostShiftService(myEvent);
                    }
                }
                ES_Timer_InitTimer(BLACKHOLE_TIMER, BlackHoleSampleTime); // Restart black hole timer
            }
            else if(ThisEvent.EventParam == 11){ //COUNTDOWN_TIMER
                if(Countdown > 1){ // 6 -> 0
                    ES_Timer_InitTimer(COUNTDOWN_TIMER, GameInterval); //Start 10s timer
                    
                    //Update time LEDs
                    ShiftRegisterVals[TIMING_LED+Countdown-1] = 0;
                    ES_Event_t myEvent;
                    myEvent.EventType = ES_UPDATE_SHIFT;
                    myEvent.EventParam = MyPriority;
                    PostShiftService(myEvent);
                    
                    Countdown--;
                }
                else{ // Game over
                    ES_Event_t BuzzEvent;
                    BuzzEvent.EventType = ES_BUZZ;
                    BuzzEvent.EventParam = 6;
                    PostBuzzService(BuzzEvent);
                    
                    NextState = GameOver;
                    Countdown = 6;
                    ES_Timer_InitTimer(COUNTDOWN_TIMER, GameOverTime/6);
                    ES_Timer_StopTimer(PLANET_TIMER);
                    ES_Timer_StopTimer(BLACKHOLE_TIMER);
                }
            }
            else if(ThisEvent.EventParam == 10){ //USER_INPUT_TIMER
                UpdateDisplay(2, "SLOW!");
                UpdateDisplay(1, "TOO");
                
                ES_Event_t BuzzEvent;
                BuzzEvent.EventType = ES_BUZZ;
                BuzzEvent.EventParam = 6;
                PostBuzzService(BuzzEvent);
                
                // set LEDs to game over state
                uint8_t newVals[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                memcpy(ShiftRegisterVals, newVals, sizeof(ShiftRegisterVals));
                ES_Event_t ShiftEvent;
                ShiftEvent.EventType = ES_UPDATE_SHIFT;
                PostShiftService(ShiftEvent);
                
                NextState = Waiting;
                DB_printf("-> Going to Waiting\n");
                
                ES_Timer_InitTimer(DELAY_TIMER, 3000);
                ES_Timer_StopTimer(PLANET_TIMER);
                ES_Timer_StopTimer(BLACKHOLE_TIMER);
            }
            else if(ThisEvent.EventParam == 12){ // DELAY_TIMER
                AsteroidTimeout = false;
            }
        }
        else if (ThisEvent.EventType == ES_PLANET_HIT){
            DB_printf("ES_PLANET_HIT\n");

            if (ThisEvent.EventParam == TargetPlanet){
                GetNewPlanet();
                UpdateScore(10);

                ES_Event_t BuzzEvent;
                BuzzEvent.EventType = ES_BUZZ;
                BuzzEvent.EventParam = 1;
                PostBuzzService(BuzzEvent);
                
                ES_Timer_InitTimer(PLANET_TIMER, PlanetSwitchTime); //Restart planet timer
                ES_Timer_InitTimer(USER_INPUT_TIMER, UserInputTimeout); //Restart user input timer
            }
        }
        
        else if(ThisEvent.EventType == ES_ASTEROID_HIT){
            if(AsteroidTimeout == false){
                DB_printf("ES_ASTEROID_HIT\n");
                
                UpdateScore(-3);

                ES_Event_t BuzzEvent;
                BuzzEvent.EventType = ES_BUZZ;
                BuzzEvent.EventParam = 2;
                PostBuzzService(BuzzEvent);
                
                AsteroidTimeout = true;
                ES_Timer_InitTimer(DELAY_TIMER, AsteroidDelay); // prevent multiple asteroid hits
                ES_Timer_InitTimer(USER_INPUT_TIMER, UserInputTimeout); //Restart user input timer
            }
        }
        
        else if(ThisEvent.EventType == ES_BLACKHOLE_HIT){
            DB_printf("ES_BLACKHOLE_HIT\n");

            if(BlackHole == true){
                UpdateScore(-5);

                ES_Event_t BuzzEvent;
                BuzzEvent.EventType = ES_BUZZ;
                BuzzEvent.EventParam = 3;
                PostBuzzService(BuzzEvent);
                
                BlackHole = false; // turn off black hole

                //Update black hole LEDs
                ShiftRegisterVals[BLACKHOLE_LED] = 0;
                ES_Event_t myEvent;
                myEvent.EventType = ES_UPDATE_SHIFT;
                myEvent.EventParam = MyPriority;
                PostShiftService(myEvent);
                
                ES_Timer_InitTimer(BLACKHOLE_TIMER, BlackHoleSampleTime);
            }
            ES_Timer_InitTimer(USER_INPUT_TIMER, UserInputTimeout);
        }
    }
    break;
    case Waiting:
    {
        if (ThisEvent.EventType == ES_TIMEOUT){
            if (ThisEvent.EventParam == 12){
                NextState = Waiting2Coins;
                DB_printf("-> Leaving Waiting\n");
                
                UpdateDisplay(2, "2 CNS");
                UpdateDisplay(1, "INSERT");
            }
        }
    }
    break;
    case GameOver:
    {                
        DB_printf("GAME_OVER!!! Your score was: %d\n", Score);

        if (ThisEvent.EventType == ES_TIMEOUT){
            if(ThisEvent.EventParam == 11){
                if (Countdown > 0){
                    if ((Countdown % 2) == 1){
                        UpdateDisplay(2, "SCORE!");
                        
                        uint8_t newVals[13] = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
                        memcpy(ShiftRegisterVals, newVals, sizeof(ShiftRegisterVals));
                        
                        ES_Event_t ShiftEvent;
                        ShiftEvent.EventType = ES_UPDATE_SHIFT;
                        ShiftEvent.EventParam = MyPriority;
                        PostShiftService(ShiftEvent);
                        
                        Countdown--;
                        ES_Timer_InitTimer(COUNTDOWN_TIMER, GameOverTime/6);
                    } else {
                        UpdateDisplay(2, "FINAL!");
                        
                        uint8_t newVals[13] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
                        memcpy(ShiftRegisterVals, newVals, sizeof(ShiftRegisterVals));
                        
                        ES_Event_t ShiftEvent;
                        ShiftEvent.EventType = ES_UPDATE_SHIFT;
                        ShiftEvent.EventParam = MyPriority;
                        PostShiftService(ShiftEvent);
                        
                        Countdown--;
                        ES_Timer_InitTimer(COUNTDOWN_TIMER, GameOverTime/6);
                    }
                } else {
                    DB_printf("Restarting the game...\n");
                    UpdateDisplay(2, "2 CNS");
                    UpdateDisplay(1, "INSERT");
                    
                    uint8_t newVals[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    memcpy(ShiftRegisterVals, newVals, sizeof(ShiftRegisterVals));
                        
                    ES_Event_t ShiftEvent;
                    ShiftEvent.EventType = ES_UPDATE_SHIFT;
                    ShiftEvent.EventParam = MyPriority;
                    PostShiftService(ShiftEvent);
                        
                    NextState = Waiting2Coins;
                    Score = 0;
                    
                    ES_Timer_InitTimer(COUNTDOWN_TIMER, 1000);
                }
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

/***************************************************************************
 private functions
 ***************************************************************************/

/****************************************************************************
Function
    GetNewPlanet
Parameters
    void
Returns
    void
Description
    get new random planet
    update LEDs
****************************************************************************/
void GetNewPlanet(void){
    int8_t NewPlanet;
    if (TargetPlanet == -1){ // starting from null
        NewPlanet = rand() % 4;
    } else {
        int8_t RandNum = rand() % 3;
        if (RandNum >= TargetPlanet){
            NewPlanet = RandNum + 1;
        } else {
            NewPlanet = RandNum;
        }

        ShiftRegisterVals[TargetPlanet] = 0;
    }
    
    ShiftRegisterVals[NewPlanet] = 1;
    ES_Event_t myEvent;
    myEvent.EventType = ES_UPDATE_SHIFT;
    myEvent.EventParam = MyPriority;
    PostShiftService(myEvent);

    TargetPlanet = NewPlanet;
}

/****************************************************************************
Function
    UpdateScore
Parameters
    int16_t DeltaScore: addition or subtraction from current score
Returns
    void
Description
    update score
    update display
****************************************************************************/
void UpdateScore(int16_t DeltaScore){
    static char ScoreString[4];

    DB_printf("Updating Score...\n");
    Score = Score + DeltaScore;
    if (Score < 0){
        Score = Score;
    }
    
    sprintf(ScoreString, "%d", Score);
    
    UpdateDisplay(1, ScoreString);
}

/****************************************************************************
Function
    UpdateDisplay
Parameters
    uint8_t WhichDisplay: which display to update
    char *Msg: message to display
Returns
    void
Description
    update score
    update display
****************************************************************************/
void UpdateDisplay(uint8_t WhichDisplay, char *Msg){
    ES_Event_t myEvent;
    myEvent.EventType = ES_NEW_WORD;
    myEvent.EventParam = WhichDisplay; // display 1
    myEvent.EventMessage = Msg;
    PostLEDService(myEvent);
}

/****************************************************************************
Function
    GetDifficulty
Parameters
    void
Returns
    void
Description
    determine difficulty of 1,2,3 from potentiometer reading
    set up module variables accordingly
****************************************************************************/
void GetDifficulty(uint8_t Level){
    PlanetSwitchTime = PlanetSwitchTime_MIN + ((PlanetSwitchTime_MAX - PlanetSwitchTime_MIN)*(1 - Level/100));
    BlackHoleProb = Level;
    
    DB_printf("Switch Time: %d\n", PlanetSwitchTime);
    DB_printf("BH: %d\n", BlackHoleProb);
}


/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/

