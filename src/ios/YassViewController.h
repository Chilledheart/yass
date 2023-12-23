// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef YASS_IOS_VIEW_CONTROLLER
#define YASS_IOS_VIEW_CONTROLLER

#import <UIKit/UIKit.h>

@interface YassViewController : UIViewController
- (void)OnStart;
- (void)OnStop;
- (void)Started;
- (void)StartFailed;
- (void)Stopped;
@property (weak, nonatomic) IBOutlet UIButton *startButton;
@property (weak, nonatomic) IBOutlet UIButton *stopButton;
@property (weak, nonatomic) IBOutlet UITextField *serverHost;
@property (weak, nonatomic) IBOutlet UITextField *serverPort;
@property (weak, nonatomic) IBOutlet UITextField *username;
@property (weak, nonatomic) IBOutlet UITextField *password;
@property (weak, nonatomic) IBOutlet UIPickerView *cipherMethod;
@property (weak, nonatomic) IBOutlet UITextField *timeout;
- (NSString*)getCipher;

@property (weak, nonatomic) IBOutlet UILabel *currentIP;
@property (weak, nonatomic) IBOutlet UILabel *status;


@end

#endif // YASS_IOS_VIEW_CONTROLLER
