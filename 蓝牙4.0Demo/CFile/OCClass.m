//
//  OCClass.m
//  SYBluetoothDemo
//
//  Created by Li JinYou on 2018/7/24.
//  Copyright © 2018年 laizheyuan. All rights reserved.
//

#import "OCClass.h"
#import <Foundation/Foundation.h>

@interface OCClass : NSObject

- (void)sayHello;

@end

@implementation OCClass

- (void)sayHello{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"PushMessage" object:nil];
}

- (void)blueTooth:(int)value{
    
    NSDictionary *dict = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithInt:value],@"blueStatus", nil];
    
    [[NSNotificationCenter defaultCenter] postNotificationName:@"ReceiveBluetooth" object:dict];
}

@end

void objcSayHello(){
    OCClass *obj = [[OCClass alloc]init];
    [obj sayHello];
}

void objcReceiveBluetooth(int status)
{
    OCClass *obj = [[OCClass alloc]init];
    [obj blueTooth:status];
}






