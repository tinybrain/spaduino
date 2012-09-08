//
//  LTViewController.m
//  Slimer
//
//  Created by Simon Aridis-Lang on 28/08/12.
//  Copyright (c) 2012 Simon Aridis-Lang. All rights reserved.
//

#import "LTViewController.h"
#import "Spa.pb.h"

@interface LTViewController ()

@end

@implementation LTViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

        
    [[UIApplication sharedApplication] setStatusBarHidden:YES
                                            withAnimation:UIStatusBarAnimationFade];
    
    [self initNetworkCommunication];
    
    //[self sendCommand:@"h"];
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

- (void)initNetworkCommunication
{
    CFReadStreamRef readStream;
    CFWriteStreamRef writeStream;
    
    CFStreamCreatePairWithSocketToHost(NULL, (CFStringRef)@"llama-macbook-pro.local", 8782, &readStream, &writeStream);
    
    _inputStream = (NSInputStream *)readStream;
    _outputStream = (NSOutputStream *)writeStream;
    
    [_inputStream setDelegate:self];
    [_outputStream setDelegate:self];
    
    [_inputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [_outputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    
    [_inputStream open];
    [_outputStream open];
}

- (void)sendCommand:(NSString*)cmd
{
    cmd = [NSString stringWithFormat:@"%@\n", cmd];

	NSData *data = [[NSData alloc] initWithData:[cmd dataUsingEncoding:NSASCIIStringEncoding]];

	[_outputStream write:[data bytes] maxLength:[data length]];
}

- (void)stream:(NSStream *)theStream
   handleEvent:(NSStreamEvent)streamEvent
{
    switch(streamEvent)
    {
        case NSStreamEventOpenCompleted:
            NSLog(@"Stream opened");
            break;
            
		case NSStreamEventHasBytesAvailable:
            if (theStream == _inputStream)
            {
                uint8_t buffer[1024];
                int len;
                
                while ([_inputStream hasBytesAvailable])
                {
                    len = [_inputStream read:buffer maxLength:sizeof(buffer)];
                    
                    if (!len) continue;

                    NSString *message = [[NSString alloc] initWithBytes:buffer length:len encoding:NSASCIIStringEncoding];
                    
                    if (!message) continue;
                    
                    NSLog(@"server said: %@", message);
                    
                    [self handleMessage:message];
                }
            }
			break;
            
		case NSStreamEventErrorOccurred:
			NSLog(@"Can not connect to the host!");
			break;
            
		case NSStreamEventEndEncountered:
			break;
            
		default:
			NSLog(@"Unknown event");
    }
}

- (void)handleMessage:(NSString*)message
{
    /*
    NSDictionary *cmdLookup = @{
    @"tmp"  : @(kTemperature),
    @"st"   : @(kStates),
    @"rly"  : @(kRelays)
    };

    NSScanner *scanner = [NSScanner scannerWithString:message];
    
    NSString *cmdString;
    
    [scanner scanUpToString:@" " intoString:&cmdString];
    
    if (!cmdString)
        return;
    
    NSNumber *cmdIndex = (NSNumber*)[cmdLookup objectForKey:cmdString];

    if (!cmdIndex)
        return;
    
    SpaCommand cmd = (SpaCommand)[cmdIndex intValue];
        
    NSLog(@"Got cmd %d\n", cmd);
    
    */
}


@end
