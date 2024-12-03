/****************************************************************************

  Header file for buzz service
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

#ifndef BuzzService_H
#define BuzzService_H

#include "ES_Types.h"

// Public Function Prototypes

bool InitBuzzService(uint8_t Priority);
bool PostBuzzService(ES_Event_t ThisEvent);
ES_Event_t RunBuzzService(ES_Event_t ThisEvent);
void SoundBuzzer(uint32_t freq, uint32_t duration);

#endif /* ServTemplate_H */

