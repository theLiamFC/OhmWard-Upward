/****************************************************************************

  Header file for shift service
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

#ifndef ShiftService_H
#define ShiftService_H

// Event Definitions
#include "ES_Configure.h" /* gets us event definitions */
#include "ES_Types.h"     /* gets bool type for returns */

// Public Function Prototypes

typedef enum{
    Waiting
    UpdatingShift
} ShiftState_t;

// Service Functions
bool InitShiftService(uint8_t Priority);
bool PostShiftService(ES_Event_t ThisEvent);
ES_Event_t RunShiftService(ES_Event_t ThisEvent);

#endif /* ServTemplate_H */

