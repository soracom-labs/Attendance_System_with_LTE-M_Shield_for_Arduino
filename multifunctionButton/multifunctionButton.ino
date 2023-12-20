/*
 * multifunctionButton.ino
 * A button that automatically switches functions based on the current time
 *
 * Copyright SORACOM
 * This software is released under the MIT License, and libraries used by these sketches
 * are subject to their respective licenses.
 * See also: https://github.com/soracom-labs/Attendance_System_with_LTE-M_Shield_for_Arduino/README.md
*/

#define CONSOLE Serial
#define ENDPOINT "uni.soracom.io"
#define SKETCH_NAME "[Multifunction Button]"
#define VERSION "1.0"

// ================================================================================
/* !!利用者が設定するパラメータ */
// 記録内容
// <注意> スペースを利用して文字列の長さを揃える
#define Function1 "Punch-In "
#define Function2 "Punch-Out"

// 記録内容を切り替える時刻
#define TargetHour 12    // 時
#define TargetMinute 0   // 分

// ================================================================================
/* for LTE-M Shield for Arduino */
#define RX 10
#define TX 11
#define BAUDRATE 9600
#define BG96_RESET 15

#define TINY_GSM_MODEM_BG96
#include <TinyGsmClient.h>

#include <SoftwareSerial.h>
SoftwareSerial LTE_M_shieldUART(RX, TX);
TinyGsm modem(LTE_M_shieldUART);
TinyGsmClient ctx(modem);

// ================================================================================
/* SSD1306 128x64ピクセルのOLEDディスプレイ */
#include <U8x8lib.h>
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);
#define U8X8_ENABLE_180_DEGREE_ROTATION 1

// ================================================================================
/* ボタン */
#define buttonPin 6

// ================================================================================
void setup() {
  CONSOLE.begin(115200);

  CONSOLE.println();
  CONSOLE.print(F("Welcome to ")); CONSOLE.println(SKETCH_NAME); 
  CONSOLE.print(F("* Version: ")); CONSOLE.println(VERSION);

  /* 通信関連 */
  CONSOLE.println();
  CONSOLE.print(F("Resetting Module"));
  pinMode(BG96_RESET,OUTPUT);
  digitalWrite(BG96_RESET,LOW);
  delay(300);
  digitalWrite(BG96_RESET,HIGH);
  delay(300);
  digitalWrite(BG96_RESET,LOW);
  CONSOLE.println(F(" -> done."));

  LTE_M_shieldUART.begin(BAUDRATE);

  CONSOLE.print(F("Restarting Modem"));
  modem.restart();
  CONSOLE.println(F(" -> done."));

  CONSOLE.print(F("Modem Info: "));
  String modemInfo = modem.getModemInfo();
  CONSOLE.println(modemInfo);

  CONSOLE.print(F("Waiting for Network"));
  while (!modem.waitForNetwork()) CONSOLE.print(".");
  CONSOLE.println(F(" -> done."));

  CONSOLE.print(F("Establishing Connection (soracom.io)"));
  modem.gprsConnect("soracom.io", "sora", "sora");
  CONSOLE.println(F(" -> done."));

  CONSOLE.print(F("Checking Network Connection"));
  while (!modem.isNetworkConnected()) CONSOLE.print(".");
  CONSOLE.println(F(" -> done."));

  CONSOLE.print(F("My IP address: "));
  IPAddress ipaddr = modem.localIP();
  CONSOLE.println(ipaddr);

  CONSOLE.println(" ");
  CONSOLE.println("Ready for Telecommunication");

  /* OLEDディスプレイ関連 */
  u8x8.begin();                                         // 初期化
  u8x8.setFlipMode(U8X8_ENABLE_180_DEGREE_ROTATION);    // 映像の反転
  u8x8.setFont(u8x8_font_8x13B_1x2_f);                  // フォントの指定
  u8x8.drawString(0, 0, "Function:");                   // 文字の描画
  u8x8.drawString(0, 4, "----------------");

  /* ボタン関連 */
  pinMode(buttonPin, INPUT_PULLUP);                     // 初期設定

  CONSOLE.println("Start!");
  CONSOLE.println(" ");
}

// ================================================================================
/* 記録内容の切り替え時刻と現在時刻を比較する関数 */
// 返り値：1 or 2 (記録内容としてFunction1を採用すべき時は1を、Function2を採用すべき時は2を返す)
int compareTime(int targetHour, int targetMinute, int currentHour, int currentMinute) {
  if (currentHour < targetHour || (currentHour == targetHour && currentMinute < targetMinute)) {
    return 1;
  } else {
    return 2;
  }
}

// ================================================================================
/* SORACOM Harvest Dataへデータを送信するための関数 */
void sendData(int status) {
  char payload[120];
  sprintf_P(payload, PSTR("{\"report\":%d}"), status);
  CONSOLE.println(payload);

  // エンドポイントへの接続
  if (!ctx.connect(ENDPOINT, 80)) {
    CONSOLE.println(F("failed."));
    delay(3000);
    return;
  }

  // リクエストの送信
  char hdr_buf[40];
  ctx.println(F("POST / HTTP/1.1"));
  sprintf_P(hdr_buf, PSTR("Host: %s"), ENDPOINT);
  ctx.println(hdr_buf);
  ctx.println(F("Content-Type: application/json"));
  sprintf_P(hdr_buf, PSTR("Content-Length: %d"), strlen(payload));
  ctx.println(hdr_buf);
  ctx.println();

  ctx.println(payload);

  //　レスポンスの受信
  while (ctx.connected()) {
    String line = ctx.readStringUntil('\n');
    // CONSOLE.println(line);
    if (line == "\r") {
      CONSOLE.println(F("Response header received."));
      break;
    }
  }
}

// ================================================================================
void loop() {
  // 現在時刻の取得
  String dateTime = modem.getGSMDateTime(DATE_TIME);

  // 取得結果の表示
  // CONSOLE.println("Current Date and Time: " + dateTime);

  int hour = dateTime.substring(0, 2).toInt();
  int minute = dateTime.substring(3, 5).toInt();
  int second = dateTime.substring(6, 8).toInt();

  u8x8.drawString(0, 6, dateTime.substring(0, 2).c_str());
  u8x8.drawString(2, 6, ":"); 
  u8x8.drawString(3, 6, dateTime.substring(3, 5).c_str());
  u8x8.drawString(5, 6, ":"); 
  u8x8.drawString(6, 6, dateTime.substring(6, 8).c_str());
  u8x8.drawString(8, 6, "  ");

  // 関数compareTime()の返り値を変数statusに格納
  int status = compareTime(TargetHour, TargetMinute, hour, minute);

  if (status == 1) {
    u8x8.drawString(0, 2, Function1);
  } else {
    u8x8.drawString(0, 2, Function2);
  }

  // ボタンの状態の読み込み
  if (digitalRead(buttonPin)) {
    // ボタンが押された場合
    u8x8.drawString(0, 6, "REPORTING ");
    CONSOLE.println(" ");

    if (status == 1) {
      CONSOLE.println(Function1);
    } else {
      CONSOLE.println(Function2);
    }
    
    sendData(status);

    u8x8.drawString(0, 6, "REPORTED  ");
    delay(1000);
  } 

  delay(100);
}
