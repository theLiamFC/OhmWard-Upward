/****************************************************************************

  Header file for template service
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

#ifndef GameService_H
#define GameService_H

// Event Definitions
#include "ES_Configure.h" /* gets us event definitions */
#include "ES_Types.h"     /* gets bool type for returns */

// Public Function Prototypes

// Public variables
extern uint8_t ShiftRegisterVals[];

typedef enum{
    Waiting2Coins,
    Waiting1Coins,
    Waiting,
    GameOn,
    GameOver
} GameState_t;

// Service Functions
bool InitGameService(uint8_t Priority);
bool PostGameService(ES_Event_t ThisEvent);
ES_Event_t RunGameService(ES_Event_t ThisEvent);

// Private Functions
void GetNewPlanet(void);
void UpdateScore(int16_t DeltaScore);
void UpdateDisplay(uint8_t WhichDisplay, char *Msg);

#endif /* ServTemplate_H */

