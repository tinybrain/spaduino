//
//  LTAppDelegate.h
//  Slimer
//
//  Created by Simon Aridis-Lang on 28/08/12.
//  Copyright (c) 2012 Simon Aridis-Lang. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "LTViewController.h"

@interface LTAppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property (nonatomic, retain) LTViewController *viewController;

@end
