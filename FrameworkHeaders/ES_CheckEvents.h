/****************************************************************************
 Module
     EF_CheckEvents.h
 Description
     header file for use with the data structures to define the event checking
     functions and the function to loop through the array calling the checkers
 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 08/05/13 15:19 jec      modifications to suit new portable type definitions
 01/15/12 12:00 jec      new header for local types
 10/16/11 17:17 jec      started coding
*****************************************************************************/

#ifndef ES_CheckEvents_H
#define ES_CheckEvents_H

#include "ES_Types.h"

typedef bool CheckFunc (void);

typedef CheckFunc (*pCheckFunc);

bool ES_CheckUserEvents(void);
bool CheckIRSensor(void);

#endif  // ES_CheckEvents_H
