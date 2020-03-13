//
//  ViewController.m
//  Deadweight
//
//  Created by Tanay Findley on 3/13/20.
//  Copyright © 2020 Tanay Findley. All rights reserved.
//

#import "ViewController.h"
#include "exploit.h"
#include "patchfinder64.h"
#include "KernelUtils.h"

@interface ViewController ()


@property (strong, nonatomic) IBOutlet UILabel *offsLabel;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
}

void runOnMainQueueWithoutDeadlocking(void (^block)(void))
{
    if ([NSThread isMainThread])
    {
        block();
    }
    else
    {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}


void lol()
{
    runOnMainQueueWithoutDeadlocking(^{
        
        [[ViewController self] offsLabel].text = @"Hi!";
        get_tfp0();
        init_kernel(kreadOwO, find_kernel_base_sockpuppet(), NULL);
        [[ViewController self] offsLabel].text = [NSString stringWithFormat:@"0x%016llx", find_trustcache()];
        printf("TrustCache: 0x%016llx", find_trustcache());
        term_kernel();
        
    });
}

- (IBAction)runButton:(id)sender {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul), ^{
        lol();
    });
}


@end
