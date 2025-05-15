#include <Arduino.h>
#include <PinChangeInterrupt.h> 

// RGB LED 핀 설정 (PWM 출력 가능)
const int redPin = 11;
const int greenPin = 10;
const int bluePin = 9;

// 단색 LED 핀 설정 (PWM 출력 가능)
const int yellowPin = 6;
const int green2Pin = 5;
const int blue2Pin = 3;

// RC 수신기에서 들어오는 PWM 신호를 받는 핀
const int onOffPin = 4;       // CH6 : 전체 ON/OFF 스위치
const int huePin = 7;         // CH3 : HSV 색상 조절
const int brightnessPin = 8;  // CH2 : 단색 LED 밝기 조절

// 각 채널의 펄스 측정용 변수 (마이크로초 단위)
volatile unsigned long ch6_start = 0, ch6_pulse = 1500; // CH6: ON/OFF
volatile unsigned long ch3_start = 0, ch3_pulse = 1500; // CH3: 색상 조절
volatile unsigned long ch2_start = 0, ch2_pulse = 1500; // CH2: 밝기 조절

// 현재 색상을 추적하기 위한 열거형
enum LedColor { NONE, BLUE, GREEN, RED };
LedColor currentColor = NONE;

// HSV → RGB 변환 함수
// h: 색상(0~360도), s: 채도(0~1), v: 밝기(0~1)
// 결과는 r/g/b 값(0~255)로 전달됨
void hsvToRgb(float h, float s, float v, int& r, int& g, int& b) {
  float c = v * s;  // 색상의 강도
  float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));  // 보조 색상 계산
  float m = v - c;  // 밝기 보정
  float r1, g1, b1;

  // 색상 범위에 따라 RGB 값 선택
  if (h < 60)      { r1 = c; g1 = x; b1 = 0; }
  else if (h < 120){ r1 = x; g1 = c; b1 = 0; }
  else if (h < 180){ r1 = 0; g1 = c; b1 = x; }
  else if (h < 240){ r1 = 0; g1 = x; b1 = c; }
  else if (h < 300){ r1 = x; g1 = 0; b1 = c; }
  else             { r1 = c; g1 = 0; b1 = x; }

  // 0~255 범위로 변환
  r = (r1 + m) * 255;
  g = (g1 + m) * 255;
  b = (b1 + m) * 255;
}

// === 인터럽트 핸들러 ===

// CH6: ON/OFF 펄스 측정
void ch6ISR() {
  if (digitalRead(onOffPin) == HIGH)
    ch6_start = micros();  // HIGH 시작 시간 기록
  else
    ch6_pulse = micros() - ch6_start;  // HIGH 길이 측정
}

// CH3: 색상 조절용 펄스 측정
void ch3ISR() {
  if (digitalRead(huePin) == HIGH)
    ch3_start = micros();
  else
    ch3_pulse = micros() - ch3_start;
}

// CH2: 단색 LED 밝기용 펄스 측정
void ch2ISR() {
  if (digitalRead(brightnessPin) == HIGH)
    ch2_start = micros();
  else
    ch2_pulse = micros() - ch2_start;
}

void setup() {
  Serial.begin(9600);  // 디버깅용 시리얼 출력 시작

  // 입력 핀 설정
  pinMode(onOffPin, INPUT);
  pinMode(huePin, INPUT);
  pinMode(brightnessPin, INPUT);

  // 출력 핀 설정
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(green2Pin, OUTPUT);
  pinMode(blue2Pin, OUTPUT);

  // 핀 체인지 인터럽트 등록 (핀 상태 변화 감지)
  attachPinChangeInterrupt(digitalPinToPCINT(onOffPin), ch6ISR, CHANGE);
  attachPinChangeInterrupt(digitalPinToPCINT(huePin), ch3ISR, CHANGE);
  attachPinChangeInterrupt(digitalPinToPCINT(brightnessPin), ch2ISR, CHANGE);
}

void loop() {
  // 색상(Hue) 계산: 1000~2000us → 0~270도 맵핑
  float hue = map(ch3_pulse, 1000, 2000, 0, 270);
  hue = constrain(hue, 0, 270);  // 안전한 범위로 제한

  int r = 0, g = 0, b = 0;
  hsvToRgb(hue, 1.0, 1.0, r, g, b);  // 채도/밝기 100% 고정

  // 단색 LED 밝기 계산: 1200~1800us → 0~255
  int brightness = map(ch2_pulse, 1200, 1800, 0, 255);
  brightness = constrain(brightness, 0, 255);

  // CH6 신호가 ON 범위일 경우 (1800us 이상)
  if (ch6_pulse >= 1800) {
    // RGB LED에 색상 출력
    analogWrite(redPin, r);
    analogWrite(greenPin, g);
    analogWrite(bluePin, b);

    // 단색 LED에도 동일 밝기로 출력
    analogWrite(yellowPin, brightness);
    analogWrite(green2Pin, brightness);
    analogWrite(blue2Pin, brightness);

    // 현재 RGB 색상에 따라 상태 저장 (디버깅용)
    if (r > g && r > b) currentColor = RED;
    else if (g > r && g > b) currentColor = GREEN;
    else if (b > r && b > g) currentColor = BLUE;
    else currentColor = NONE;
  }
  // OFF 상태일 경우 모든 LED OFF
  else {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 0);
    analogWrite(yellowPin, 0);
    analogWrite(green2Pin, 0);
    analogWrite(blue2Pin, 0);
    currentColor = NONE;
  }

  // 시리얼 모니터 디버깅 출력
  Serial.print("CH2 (solid bright): ");
  Serial.print(ch2_pulse);
  Serial.print("  CH3 (HSV hue): ");
  Serial.print(ch3_pulse);
  Serial.print("  CH6 (on/off): ");
  Serial.print(ch6_pulse);
  Serial.print("  Hue: ");
  Serial.print(hue);
  Serial.print("  Brightness: ");
  Serial.print(brightness);
  Serial.print("\n");

  delay(20);  // 너무 빠른 루프 반복 방지
}
