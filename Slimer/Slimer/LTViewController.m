//
//  LTViewController.m
//  Slimer
//
//  Created by Simon Aridis-Lang on 28/08/12.
//  Copyright (c) 2012 Simon Aridis-Lang. All rights reserved.
//

#import "LTViewController.h"
#import "LTAppDelegate.h"

@interface LTViewController ()

@end

@implementation LTViewController

- (void)viewDidLoad
{
    LTAppDelegate *appDelegate = (LTAppDelegate*)[[UIApplication sharedApplication] delegate];
    appDelegate.viewController = self;
    
    [super viewDidLoad];
    
    tu = 40.f;
    tl = 15.f;
    m  = 20.f;
    bh = 280.f;

    modeButtons = @[ _offButton, _offButton, _offButton, _autoButton, _rapidButton, _soakButton ];
    [modeButtons retain];
    
    timerFormmatter = [[NSDateFormatter alloc] init];
    [timerFormmatter setDateFormat:@"H:mm.ss"];
    
    statusStrings = @[ @"ok", @"unknown", @"temperature", @"water level", @"high limit" ];
    [statusStrings retain];
    
    [[UIApplication sharedApplication] setStatusBarHidden:YES
                                            withAnimation:UIStatusBarAnimationFade];
    
    self.autoTimerLabel.text = nil;
    self.rapidTimerLabel.text = nil;
    self.filterTimerLabel.text = nil;
    self.soakTimerLabel.text = nil;
    
    //[self startClient];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    return UIInterfaceOrientationIsLandscape(interfaceOrientation);
}

- (void)startClient
{
    NSLog(@"Starting Client");
    
    CFReadStreamRef readStream;
    CFWriteStreamRef writeStream;
    
    CFStreamCreatePairWithSocketToHost(NULL, (CFStringRef)@"bubbles.local", 8782, &readStream, &writeStream);
    
    inputStream = (NSInputStream*)readStream;
    outputStream = (NSOutputStream*)writeStream;

    [inputStream setDelegate:self];
    [outputStream setDelegate:self];

    [inputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [outputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    
    [inputStream open];
    [outputStream open];
}

- (void)stopClient
{
    [inputStream close];
    [outputStream close];
}

- (void)stream:(NSStream *)st
   handleEvent:(NSStreamEvent)streamEvent
{
    switch(streamEvent)
    {
        case NSStreamEventOpenCompleted:
        {
            NSLog(@"%@ opened", [st class]);
            break;
        }
            
		case NSStreamEventHasBytesAvailable:
        {
            if (st == inputStream)
            {
                uint8_t buffer[1024];
                int len;
                
                while ([inputStream hasBytesAvailable])
                {
                    len = [inputStream read:buffer maxLength:sizeof(buffer)];
                    
                    if (!len) continue;
                    
                    SpaMessage *message = [SpaMessage parseFromData:[NSData dataWithBytes:buffer
                                                                                   length:len]];
                    
                    if (message)
                        [self handleMessage:message];
                    else
                        NSLog(@"Invalid message (%d bytes)", len);
                }
            }
			break;
        }
            
		case NSStreamEventErrorOccurred:
        {
            NSLog(@"%@ error", [st class]);
            
            /*
            connectTimer = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                        target:self
                                                          selector:@selector(startClient)
                                                          userInfo:nil
                                                           repeats:NO
                            ];
             */
			break;
        }
            
		case NSStreamEventEndEncountered:
        {
            NSLog(@"%@ closed", [st class]);
			break;
        }
            
		default:
        {
			NSLog(@"Unknown event");
        }
    }
}

- (void)sendCommand:(NSString*)cmd
{
	NSData *data = [[NSData alloc] initWithData:[cmd dataUsingEncoding:NSASCIIStringEncoding]]; 
	[outputStream write:[data bytes] maxLength:[data length]];
    
    NSLog(@"sent %@", cmd);
}

- (void)handleMessage:(SpaMessage*)message
{
    Class klass = NSClassFromString(message.className);
    
    NSObject *sm = [klass parseFromData:message.message];
    
    if (![sm isKindOfClass:[PBGeneratedMessage class]])
        return;
    
    PBGeneratedMessage *pbm = (PBGeneratedMessage*)sm;
    
    if ([Relays class] == klass)
        return [self handleRelays:(Relays*)pbm];
    
    if ([Temperature class] == klass)
        return [self handleTemperature:(Temperature*)pbm];
    
    if ([States class] == klass)
        return [self handleStates:(States*)pbm];
    
    if ([ScheduleTimers class] == klass)
        return [self handleTimers:(ScheduleTimers*)pbm];
}

// ---------------------------------------------------------------------------------------
// States

- (void)handleStates:(States*)s
{
    NSLog(@"states %d %d %d %d", s.mode, s.pump, s.aux, s.status);
    
    States *o = self.states;
        
    if (o && o.mode >= 2 && o.mode <= 5)
        [[modeButtons objectAtIndex:o.mode] setSelected:NO];
    
    self.states = s;
    
    int md = s ? s.mode : -1;
    
    md = (md && s.mode >= 2 && s.mode <= 5) ? md : -1;
    
    if (md > 0)
    {
        UIButton *b = [modeButtons objectAtIndex:md];
        if (b)
            [b setSelected:YES];
    }
    
    self.auxButton.selected = (s.aux == 1);
        
    self.status.hidden = (s.status == 0);
    self.status.text = [statusStrings objectAtIndex:s.status];
}

- (IBAction)mode:(UIButton *)sender
{
    [self sendCommand:[NSString stringWithFormat:@"md %d\n", sender.tag]];

    for(UIButton *b in modeButtons)
        [b setSelected:NO];

    [sender setSelected:YES];
}

- (IBAction)aux:(UIButton *)sender
{
    [self sendCommand:@"a\n"];
    [_auxButton setSelected:!_auxButton.selected];
}

// ---------------------------------------------------------------------------------------
// Relays

- (void)handleRelays:(Relays*)r
{
    self.relays = r;
    
    _safetyRelay.highlighted = r.safety;
    _pumpRelay.highlighted = r.pump;
    _heatRelay.highlighted = r.heat;
    _auxRelay.highlighted = r.aux;
    
    NSLog(@"relays %d, %d, %d, %d", r.safety, r.pump, r.heat, r.aux);
}

// ---------------------------------------------------------------------------------------
// Temperature Methods

- (CGFloat)tempToHeight:(CGFloat)t
{
    return bh * (t - tl) / (tu - tl);
}

- (CGFloat)heightToTemp:(CGFloat)h
{
    return tl + h * (tu - tl) / bh;
}

- (void)updateTemperature:(CGFloat)t
                  withBar:(UIView*)b
                 tempView:(UIView*)v
                    label:(UILabel*)l
{
    CGFloat h = [self tempToHeight:t];
    CGFloat y = bh - h + m;

    CGRect bf = b.frame;
    CGRect vf = v.frame;
    
    b.frame = CGRectMake(bf.origin.x, y, bf.size.width, h);
    v.frame = CGRectMake(vf.origin.x, y - 20.f, vf.size.width, vf.size.height);
    
    l.text = [NSString stringWithFormat:@"%.02f", t];
}

- (void)handleTemperature:(Temperature*)t
{
    self.temperature = t;
    
    [self updateTemperature:t.temperature
                    withBar:_temperatureBar
                   tempView:_temperatureView
                      label:_temperatureLabel
     ];
    
    if (editingSetPoint)
        return;
    
    [self updateTemperature:t.setPoint
                    withBar:_setPointBar
                   tempView:_setPointView
                      label:_setPointLabel
     ];
    
    NSLog(@"temperature %f setpoint %f derivative %f triggerstate %d",
          t.temperature, t.setPoint, t.derivative, t.triggerState);
}

- (IBAction)onSetPointPanGesture:(UIPanGestureRecognizer*)gesture
{
    switch(gesture.state)
    {
        case UIGestureRecognizerStateBegan:
            editingSetPoint = YES;
            break;
            
        case UIGestureRecognizerStateChanged:
        {
            CGRect spr = _setPointView.frame;
            CGPoint p = [gesture translationInView:_setPointView];
            
            CGFloat y = spr.origin.y + 20.f + p.y;
            CGFloat h = bh + m - y;
            
            tmpSetPoint = clip(tl, tu, [self heightToTemp:h]);
            
            [self updateTemperature:tmpSetPoint
                            withBar:_setPointBar
                           tempView:_setPointView
                              label:_setPointLabel
             ];
            
            [gesture setTranslation:CGPointMake(0.f, 0.f) inView:_setPointView];
            break;
        }
            
        default:
        {
            editingSetPoint = NO;
            [self sendCommand:[NSString stringWithFormat:@"sp %.02f\n", tmpSetPoint]];
            break;
        }
    }
}

// ---------------------------------------------------------------------------------------
// States

- (void)handleTimers:(ScheduleTimers*)t
{
    NSString* (^formatTimer)(int) = ^ NSString* (int i)
    {
        int hr = i / 3600; i %= 3600;
        int mn = i / 60; i %= 60;
        
        return [NSString stringWithFormat:@"%02d:%02d.%02d", hr, mn, i];
    };
    
    int mr = t.manualDuration > 0 ? t.manualDuration - t.dutyElapsed : 0;
    
    _rapidTimerLabel.text = (_rapidButton.selected)
    ? formatTimer(mr) : @"";

    _filterTimerLabel.text = (_filterButton.selected)
    ? formatTimer(mr) : @"";

    _soakTimerLabel.text = (_soakButton.selected)
    ? formatTimer(mr) : @"";        
}

@end
