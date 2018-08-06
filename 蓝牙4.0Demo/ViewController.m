//
//  ViewController.m
//  蓝牙4.0Demo
//
//  Created by lby on 17/3/21.
//  Copyright © 2017年 lby. All rights reserved.
//

#import "ViewController.h"
#import <CoreBluetooth/CoreBluetooth.h>
#include "protocol.h"

@interface ViewController ()<CBCentralManagerDelegate,CBPeripheralDelegate>

/// 中央管理者 -->管理设备的扫描 --连接
@property (nonatomic, strong) CBCentralManager *centralManager;
// 存储的设备
@property (nonatomic, strong) NSMutableArray *peripherals;
@property (nonatomic, strong) NSMutableArray *dataArray;
// 扫描到的设备
@property (nonatomic, strong) CBPeripheral *cbPeripheral;
// 文本
@property (weak, nonatomic) IBOutlet UITextView *peripheralText;
// 蓝牙状态
@property (nonatomic, assign) CBManagerState peripheralState;

@property (nonatomic, strong) NSTimer *timer;

@property (nonatomic, strong) CBCharacteristic *characteristic;

@property (nonatomic, strong) UITextView *text;
@end

// 蓝牙设备名
static NSString * const kBlePeripheralName = @"9A62";
// 通知服务
static NSString * const kNotifyServerUUID = @"9A62";
// 通知特征值
static NSString * const kNotifyCharacteristicUUID = @"4B40";
// 写描述UUID
static NSString * const kDescriptorUUID = @"2902";

@implementation ViewController

- (void)viewDidLoad {
    
    [super viewDidLoad];
    
    self.characteristic = nil;
    [self centralManager];
    self.peripheralText.hidden = YES;
    
    [self createSubviews];
    [self blueToothSetting];
}

//获取当前时间戳
- (NSString *)currentTimeStr{
    NSDate* date = [NSDate dateWithTimeIntervalSinceNow:0];
    NSTimeInterval time=[date timeIntervalSince1970];
    NSString *timeString = [NSString stringWithFormat:@"%.0f", time];
    return timeString;
}

// 蓝牙设置
- (void)blueToothSetting
{
    // 随机数响应
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(PushMessage) name:@"PushMessage" object:nil];
    // 蓝牙反馈回调
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(blueCallback:) name:@"ReceiveBluetooth" object:nil];
    
    // 解析初始化
    ProtocolInit();
    // 修改当前的协议状态
    protocolChangeStatus(STA_WAIT);
    
    // 创建定时器每10毫秒解析一次
    self.timer = [NSTimer timerWithTimeInterval:0.01 target:self selector:@selector(timerAction) userInfo:nil repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:self.timer forMode:NSDefaultRunLoopMode];
}

// 定时器
- (void)timerAction
{
    ProtocolParser();
}

// 蓝牙开门
- (void)blueOpen
{
    char msg[MSG_SIZE] = {};
    protocolChangeStatus(STA_WORK);
    ProtocolConferKey();
    
    //蓝牙开门指令，uid可以改为设备标识
    sprintf(msg,"{\"typ\":\"req\",\"cmd\":\"1101\",\"tck\":%d,\"cnt\":{\"uid\":\"mydevice\",\"tckt\":1,\"mtp\":1}}",[[self currentTimeStr] intValue]);
    ProtocolPack(0x00,0x01,msg,NULL,0);
    
//    NSMutableString *string = [NSMutableString string];
//    for(int i=0;i<protoLength;i++) {
//        [string appendFormat:@"%c",protoPack[i]];
//    }
//    NSLog(@"%@",string);
//    [self showMessage:string];
    
    [self sendMessage];
}

// 蓝牙反馈回调
- (void)blueCallback:(NSNotification *)notif
{
    //蓝牙反馈状态，0为非重发，1为重发，默认为0
    int blueStatus = [notif.object[@"blueStatus"] intValue];
    NSLog(@"收到 == %d",blueStatus);
    
    [self sendMessage];
    
    if (blueStatus == 0) {
        UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@"提示" message:@"蓝牙开门成功" delegate:self cancelButtonTitle:@"确认" otherButtonTitles:nil, nil];
        [alertView show];
    }
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (self.cbPeripheral) {
            [self.centralManager cancelPeripheralConnection:self.cbPeripheral];
        }
    });
}

// 状态更新时调用
- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
    switch (central.state) {
        case CBManagerStateUnknown:{
            NSLog(@"为知状态");
            self.peripheralState = central.state;
        }
            break;
        case CBManagerStateResetting:
        {
            NSLog(@"重置状态");
            self.peripheralState = central.state;
        }
            break;
        case CBManagerStateUnsupported:
        {
            NSLog(@"不支持的状态");
            self.peripheralState = central.state;
        }
            break;
        case CBManagerStateUnauthorized:
        {
            NSLog(@"未授权的状态");
            self.peripheralState = central.state;
        }
            break;
        case CBManagerStatePoweredOff:
        {
            NSLog(@"关闭状态");
            self.peripheralState = central.state;
        }
            break;
        case CBManagerStatePoweredOn:
        {
            NSLog(@"开启状态－可用状态");
            self.peripheralState = central.state;
            NSLog(@"%ld",(long)self.peripheralState);
        }
            break;
        default:
            break;
    }
}

//// 蓝牙反馈
//- (void)ReceiveBluetooth
//{
////    NSMutableString *string = [NSMutableString string];
////    for(int i=0;i<protoLength;i++) {
////        [string appendFormat:@"%c",protoPack[i]];
////    }
////    NSLog(@"%@",string);
////    [self showMessage:string];
//
//    if (self.characteristic) {
//        [self showMessage:@"蓝牙反馈"];
//
//        int num = protoLength / 20;
//        int length = protoLength % 20;
//        for (int i = 0;i < protoLength/20 + 1; i++) {
//
//            if (i == num  && protoLength % 20 > 0) {
//                Byte test[length];
//
//                for (int j=0; j<length; j++) {
//                    test[j] = protoPack[i * 20 + j];
//                }
//                NSData *data = [NSData dataWithBytes:&test length:length];
//                [self.cbPeripheral writeValue:data forCharacteristic:self.characteristic type:CBCharacteristicWriteWithoutResponse];
//            }
//            else{
//                Byte mydata[20] = {};
//                for (int j=0; j<20; j++) {
//                    mydata[j] = protoPack[i * 20 + j];
//                }
//                NSData *data = [NSData dataWithBytes:&mydata length:20];
//                [self.cbPeripheral writeValue:data forCharacteristic:self.characteristic type:CBCharacteristicWriteWithoutResponse];
//            }
//        }
//    }
//}

// 随机数响应回调
- (void)PushMessage
{
//    [self showMessage:@"已发送 :"];
//    NSMutableString *string = [NSMutableString string];
//    for(int i=0;i<protoLength;i++) {
//        [string appendFormat:@"%02X ",protoPack[i]];
//    }
//    [self showMessage:string];
    
    [self sendMessage];
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.01 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self blueOpen];
    });
}

// 分段发送
- (void)sendMessage{
    if (self.characteristic) {
        
        //        NSMutableString *keyStr = [NSMutableString string];
        //        for(int i=0;i<16;i++) {
        //            [keyStr appendFormat:@"%02X  ",protoKey[i]];
        //        }
        //        [self showMessage:[NSString stringWithFormat:@"发送数据, key == %@",keyStr]];
        
        int num = protoLength / 20;
        int length = protoLength % 20;
        
        
        for (int i = 0;i < num + 1; i++) {
            
            if (i == num && protoLength % 20 > 0) {
                Byte mydata[length];
                for (int j=0; j<length; j++) {
                    mydata[j] = protoPack[i * 20 + j];
                }
                NSData *data = [NSData dataWithBytes:&mydata length:length];
                [self.cbPeripheral writeValue:data forCharacteristic:self.characteristic type:CBCharacteristicWriteWithoutResponse];
            }
            else{
                Byte mydata[20] = {};
                for (int j=0; j<20; j++) {
                    mydata[j] = protoPack[i * 20 + j];
                }
                NSData *data = [NSData dataWithBytes:&mydata length:20];
                [self.cbPeripheral writeValue:data forCharacteristic:self.characteristic type:CBCharacteristicWriteWithoutResponse];
            }
        }
    }
}

- (void)createSubviews{
    
    self.dataArray = [NSMutableArray array];
    
    self.tableview = [[UITableView alloc]initWithFrame:CGRectMake(0, -30, ScreenWidth, ScreenHeight - 200) style:UITableViewStyleGrouped];
    self.tableview.delegate = self;
    self.tableview.dataSource = self;
    self.tableview.backgroundColor = [UIColor lightGrayColor];
    [self.view addSubview:self.tableview];
    
    UITextView *text = [[UITextView alloc] initWithFrame:CGRectMake(0, ScreenHeight - 200, ScreenWidth, 200)];
    [self.view addSubview:text];
    text.scrollEnabled = YES;
    text.editable = NO;
    self.text = text;
    
    UIBarButtonItem *myButton = [[UIBarButtonItem alloc] initWithTitle:@"扫描" style:UIBarButtonItemStylePlain target:self action:@selector(scanForPeripherals)];
    self.navigationItem.rightBarButtonItem = myButton;
    
    UIBarButtonItem *Button = [[UIBarButtonItem alloc] initWithTitle:@"开门" style:UIBarButtonItemStylePlain target:self action:@selector(openDoor)];
    self.navigationItem.leftBarButtonItem = Button;
}

// 扫描设备
- (void)scanForPeripherals
{
    [self.peripherals removeAllObjects];
    [self.dataArray removeAllObjects];
    if (self.cbPeripheral) {
        [self.centralManager cancelPeripheralConnection:self.cbPeripheral];
    }
    self.text.text = @"";

    [self.centralManager stopScan];
    if (self.peripheralState ==  CBManagerStatePoweredOn)
    {
        [self.centralManager scanForPeripheralsWithServices:nil options:nil];
    }
}

/**
 扫描到设备
 
 @param central 中心管理者
 @param peripheral 扫描到的设备
 @param advertisementData 广告信息
 @param RSSI 信号强度
 */
- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI
{
    if (advertisementData[@"kCBAdvDataLocalName"]) {
        if (![self.peripherals containsObject:peripheral])
        {
            NSString *blueName = [[NSUserDefaults standardUserDefaults] objectForKey:@"blueName"];
            if ([blueName isEqualToString:advertisementData[@"kCBAdvDataLocalName"]]) {
                self.cbPeripheral = peripheral;
            }
            
            if ([[NSString stringWithFormat:@"%@",advertisementData[@"kCBAdvDataServiceUUIDs"][0]] isEqualToString:kBlePeripheralName])
            {
                [self.peripherals addObject:peripheral];
                [self.dataArray addObject:advertisementData];
            }
        }
    }
    [self.tableview reloadData];
}

/**
 连接失败
 
 @param central 中心管理者
 @param peripheral 连接失败的设备
 @param error 错误信息
 */

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    if ([peripheral.name isEqualToString:kBlePeripheralName])
    {
        [self.centralManager connectPeripheral:peripheral options:nil];
    }
}

/**
 连接断开
 
 @param central 中心管理者
 @param peripheral 连接断开的设备
 @param error 错误信息
 */

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    [self showMessage:@"断开连接"];
    if ([peripheral.name isEqualToString:kBlePeripheralName])
    {
        [self.centralManager connectPeripheral:peripheral options:nil];
    }
}

/**
 连接成功
 
 @param central 中心管理者
 @param peripheral 连接成功的设备
 */
- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
    [self showMessage:[NSString stringWithFormat:@"连接设备:%@成功",peripheral.name]];
    peripheral.delegate = self;
    [peripheral discoverServices:nil];
}

/**
 扫描到服务
 
 @param peripheral 服务对应的设备
 @param error 扫描错误信息
 */
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error
{
    // 遍历所有的服务
    for (CBService *service in peripheral.services)
    {
        // 获取对应的服务
        if ([service.UUID.UUIDString isEqualToString:kNotifyServerUUID])
        {
            // 根据服务去扫描特征
            [self showMessage:[NSString stringWithFormat:@"发现的服务 == %@",service.UUID.UUIDString]];
            [peripheral discoverCharacteristics:@[[CBUUID UUIDWithString:kNotifyCharacteristicUUID]] forService:service];
        }
    }
}

/**
 扫描到对应的特征
 
 @param peripheral 设备
 @param service 特征对应的服务
 @param error 错误信息
 */
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error
{
    // 遍历所有的特征
    for (CBCharacteristic *characteristic in service.characteristics)
    {
        if ([characteristic.UUID.UUIDString isEqualToString:kNotifyCharacteristicUUID])
        {
            self.characteristic = characteristic;
            if (self.characteristic.properties & CBCharacteristicPropertyNotify) {
                [peripheral setNotifyValue:YES forCharacteristic:self.characteristic];
            }
            [peripheral discoverDescriptorsForCharacteristic:self.characteristic];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    if (error)
        {
            NSLog(@"Error changing notification state: %@", error.localizedDescription);
        }
        
        if (characteristic.isNotifying)
        {
           // NSLog(@"Notification began on %@", characteristic);
            [peripheral readValueForCharacteristic:characteristic];
        }
        else
        {
            [self.centralManager cancelPeripheralConnection:self.cbPeripheral];
        }
    }

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverDescriptorsForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    /*
    for (CBDescriptor *d in characteristic.descriptors) {

        Byte bytes[]={0x01,0x00};
        NSLog(@"详情 uuid == %@",d.UUID.UUIDString);
        [peripheral readValueForDescriptor:d];
        
        if ([d.UUID.UUIDString isEqualToString:kDescriptorUUID]) {
            [peripheral writeValue:[[NSData alloc] initWithBytes:bytes length:16] forCharacteristic:characteristic type:CBCharacteristicWriteWithoutResponse];
        }
    }
     */
}

/**
 根据特征读到数据
 
 @param peripheral 读取到数据对应的设备
 @param characteristic 特征
 @param error 错误信息
 */
- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(nonnull CBCharacteristic *)characteristic error:(nullable NSError *)error
{
    Byte *testByte = (Byte *)[characteristic.value bytes];
    for (int i=0; i<characteristic.value.length; i++) {
        char test = testByte[i] & 0XFF;
        ProtocolRecevier(&test);
    }
    
    NSString * str = [[NSString alloc] initWithData:characteristic.value encoding:NSUTF8StringEncoding];
    [self showMessage:@"收到的数据"];
    [self showMessage:str];
}

#pragma mark - tableview
-(NSInteger)numberOfSectionsInTableView:(UITableView *)tableView{
    return 1;
}
-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section{
    return self.peripherals.count;
}

-(CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath{
    return 50;
}

-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString * identify = @"cell";
    UITableViewCell * cell = [tableView dequeueReusableCellWithIdentifier:identify];
    
    if (cell == nil) {
        cell = [[UITableViewCell alloc]initWithStyle:UITableViewCellStyleDefault reuseIdentifier:identify];
        cell.selectionStyle = UITableViewCellSelectionStyleDefault;
    }
    NSDictionary *advertisementData = self.dataArray[indexPath.row];
    cell.textLabel.text = advertisementData[@"kCBAdvDataLocalName"];
    
    return cell;
}

-(void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    CBPeripheral *peripheral = self.peripherals[indexPath.row];
    self.cbPeripheral = peripheral;
    [self.centralManager connectPeripheral:peripheral options:nil];
    
    NSDictionary *advertisementData = self.dataArray[indexPath.row];
    [[NSUserDefaults standardUserDefaults] setObject:advertisementData[@"kCBAdvDataLocalName"] forKey:@"blueName"];
}

- (void)openDoor
{
    if (!self.cbPeripheral) {
        UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@"提示" message:@"请点击开门的蓝牙" delegate:self cancelButtonTitle:@"确认" otherButtonTitles:nil, nil];
        [alertView show];
        return;
    }
    [self.centralManager connectPeripheral:self.cbPeripheral options:nil];
}

// 日志输出
- (void)showMessage:(NSString *)message
{
    self.text.text = [self.text.text stringByAppendingFormat:@"%@\n",message];
}

- (NSMutableArray *)peripherals
{
    if (!_peripherals) {
        _peripherals = [NSMutableArray array];
    }
    return _peripherals;
}

- (CBCentralManager *)centralManager
{
    if (!_centralManager)
    {
        _centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    }
    return _centralManager;
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}



@end
