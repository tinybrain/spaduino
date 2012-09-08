//
//  LTViewController.h
//  Slimer
//
//  Created by Simon Aridis-Lang on 28/08/12.
//  Copyright (c) 2012 Simon Aridis-Lang. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface LTViewController : UIViewController <NSStreamDelegate>
{
    NSInputStream *_inputStream;
    NSOutputStream *_outputStream;
}

@end

/*

typedef enum : unsigned char
{
    kHi,
    kAck,
    kUnknown,
    kError,
    kNotReady,
    kInvalidArg,
    kStates,
    kSync,
    kTime,
    kTemperature,
    kRelays,
    kMode,
    kAux,
    kSetPoint,
    kSchedules,
    kScheduleTimers
} SpaCommand;

typedef enum : unsigned char
{
    kOk,
    kUnknownError,
    kTemperatureSensor,
    kWaterLevelSensor,
    kHighLimit
} SpaError;

typedef enum : unsigned char
{
    mError,
    mInit,
    mOff,
    mAutoHeat,
    mRapidHeat,
    mSoak
} SpaMode;

typedef enum : unsigned char
{
    pError,
    pOff,
    pOn,
    pHeat
} SpaPumpMode;

typedef enum : unsigned char
{
    rSafety,
    rPump,
    rHeat,
    rAux
} Relay;
 
 */


