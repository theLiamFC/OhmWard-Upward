
/****************************************************************************

  Header file for LED Service
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

#ifndef LEDService_H
#define LEDService_H

#include <stdint.h>
#include <stdbool.h>

#include "ES_Events.h"
#include "ES_Port.h"  

// Public Function Prototypes
bool InitLEDService(uint8_t Priority);
bool PostLEDService(ES_Event_t ThisEvent);
ES_Event_t RunLEDService(ES_Event_t ThisEvent);

typedef enum{
	IDLE,
  UPDATING
} LED_State_t;

#endif /* ServTemplate_H */

