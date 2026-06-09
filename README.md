智慧图书馆资产管理系统 – STM32 + RFID + 物联网

![GitHub last commit](https://img.shields.io/badge/last%20commit-2026--06--09-brightgreen)
![Platform](https://img.shields.io/badge/platform-STM32F103C8T6-blue)
![License](https://img.shields.io/badge/license-MIT-green)

 📌 项目简介

本项目实现了一套基于 STM32F103C8T6 + ESP8266-01S + PN532 (RFID) + DHT11 的智慧图书资产管理系统。系统能够读取图书电子标签（ISO/IEC 14443A），采集环境温湿度，并通过 MQTT 协议 将数据上报到 OneNET 物联网平台，最终在 Web 端实现图书资产的全生命周期台账管理（含折旧、盘点、报废申请等财务功能）。vscode+platform拓展使用Arduino框架

🔧 技术栈与参数

| 模块             | 型号/技术                                          | 关键参数/配置                                      |
|-----------------|---------------------------------------------------|--------------------------------------------------|
| 主控 MCU        | STM32F103C8T6                                    | ARM Cortex-M3，72MHz，64KB Flash                |
| 无线通信        | ESP8266-01S                                      | AT 固件 + MQTT 透传，TCP Keep-Alive = 30s      |
| RFID 读写       | PN532                                            | I2C 接口（SCL-PB6, SDA-PB7），13.56MHz，MIFARE 协议 |
| 环境传感器      | DHT11                                            | 单总线（OneWire），湿度±5%，温度±2℃              |
| 云平台          | OneNET / 阿里云 IoT                              | MQTT 3.1.1，TLS 可选（本例为明文 1883）          |
| 数据格式        | Alink JSON / OneJSON                            | 动态字符串拼接，处理 `\"` / `\\` 转义            |
| 开发工具链      | Keil MDK 5，STM32CubeMX，Arduino IDE（辅助调试） | 串口调试助手波特率 115200                        |

 🧠 核心技术难点与解决方案

 1. 串口不定长数据帧丢包 → **DMA + 空闲中断**
问题：ESP8266 返回的 AT 响应长度不固定，普通接收循环易造成数据丢失。  
解决：使能空闲中断 + DMA 循环模式，实现 CPU 零干预的不定长帧接收，实测 200 字节以内无丢包。

 2. JSON 载荷嵌套转义错误 → **动态拼接 + 边界校验**
问题：阿里云/OneNET 物模型要求 JSON 内嵌特殊字符（如 `\"value\"`），直接 `sprintf` 易导致结构错位。  
解决：手工构造字符串，逐段追加并对双引号、逗号进行显式转义（`\\\"` / `\\,`），同时预留 buffer 边界检查，防止溢出。

 3. 弱网长连接断开 → **心跳保活 + 自动重连**
问题：Wi-Fi 信号波动或云平台空闲踢线导致 MQTT 断连。  
解决：配置 `AT+MQTTKEEPALIVE=30`，同时在 `loop()` 中维护定时器（每 3 秒发布一次数据），若连续 2 次 PUB 无 ACK 则触发 `AT+MQTTCLEAN=1` + 重连流程。

📡 系统架构图

[ DHT11 ] → [ STM32F103 ] ← [ PN532 (RFID) ]
│
(USART2)
↓
[ ESP8266-01S ]
│
MQTT
↓
[ OneNET 平台 ]
│
HTTP API
↓
[ 前端 Web 资产管理系统 ]
（图书折旧 / 热力图 / 报废审批）
