//
//  ViewController.h
//  蓝牙4.0Demo
//
//  Created by lby on 17/3/21.
//  Copyright © 2017年 lby. All rights reserved.
//

#import <UIKit/UIKit.h>

#define ScreenWidth [[UIScreen mainScreen]bounds].size.width

#define ScreenHeight [[UIScreen mainScreen]bounds].size.height

@interface ViewController : UIViewController<UITableViewDelegate,UITableViewDataSource>

@property (nonatomic,strong) UITableView * tableview;

@end

