//
//  LTViewController.h
//  Slimer
//
//  Created by Simon Aridis-Lang on 28/08/12.
//  Copyright (c) 2012 Simon Aridis-Lang. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "Spa.pb.h"

#define min(_x, _y) (_x < _y ? _x : _y)
#define max(_x, _y) (_x > _y ? _x : _y)

#define clip(_min, _max, _value) ( min(_max, max(_min, _value)) )

@interface LTViewController : UIViewController <NSStreamDelegate>
{
    NSInputStream     *inputStream;
    NSOutputStream    *outputStream;
    
    UIButton *nilButton;
    
    CGFloat tu;
    CGFloat tl;
    CGFloat m;
    CGFloat bh;
    
    Temperature *temperature;
    Relays *relays;
    States *states;
    ScheduleTimers *timers;
    
    Boolean editingSetPoint;
    CGFloat tmpSetPoint;
    
    NSArray *modeButtons;
    NSTimer *connectTimer;
    
    NSDateFormatter *timerFormmatter;
    NSArray *statusStrings;
}

@property (nonatomic, retain) IBOutlet UILabel  *safetyRelay;
@property (nonatomic, retain) IBOutlet UILabel  *pumpRelay;
@property (nonatomic, retain) IBOutlet UILabel  *heatRelay;
@property (nonatomic, retain) IBOutlet UILabel  *auxRelay;
@property (nonatomic, retain) IBOutlet UILabel  *status;

@property (nonatomic, retain) IBOutlet UIView   *barBackground;

@property (nonatomic, retain) IBOutlet UILabel  *temperatureLabel;
@property (nonatomic, retain) IBOutlet UIView   *temperatureView;
@property (nonatomic, retain) IBOutlet UIView   *temperatureBar;

@property (nonatomic, retain) IBOutlet UILabel  *setPointLabel;
@property (nonatomic, retain) IBOutlet UIView   *setPointView;
@property (nonatomic, retain) IBOutlet UIView   *setPointBar;

@property (nonatomic, retain) IBOutlet UIButton *offButton;
@property (nonatomic, retain) IBOutlet UIButton *autoButton;
@property (nonatomic, retain) IBOutlet UIButton *rapidButton;
@property (nonatomic, retain) IBOutlet UIButton *filterButton;
@property (nonatomic, retain) IBOutlet UIButton *soakButton;
@property (nonatomic, retain) IBOutlet UIButton *auxButton;

@property (nonatomic, retain) IBOutlet UILabel  *autoTimerLabel;
@property (nonatomic, retain) IBOutlet UILabel  *rapidTimerLabel;
@property (nonatomic, retain) IBOutlet UILabel  *filterTimerLabel;
@property (nonatomic, retain) IBOutlet UILabel  *soakTimerLabel;

@property (nonatomic, retain) Temperature *temperature;
@property (nonatomic, retain) Relays *relays;
@property (nonatomic, retain) States *states;

- (void)startClient;
- (void)stopClient;

@end
