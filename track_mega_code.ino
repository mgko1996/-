#include <Keypad.h>
#include <Servo.h>
#include <deprecated.h>
#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <require_cpp11.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>

#define SDA_PIN 10
#define RST_PIN 9

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
    {'1', '2', '3'}, // {  S1,  S2,  S3}
    {'4', '5', '6'}, // {  S5,  S6,  S7}
    {'7', '8', '9'}, // {  S9, S10, S11}
    {'*', '0', '#'}  // { S13, S14, S15}
};

byte rowPins[ROWS] = {29, 28, 27, 26}; // {R4, R3, R2, R1}
byte colPins[COLS] = {23, 24, 25};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS); // 키패드 오브젝트 생성

LiquidCrystal_I2C lcd(0x27, 20, 4);

Servo myservo;
MFRC522 rfid(SDA_PIN, RST_PIN);

bool longPress = false; // 키패드 이벤트용 및 계속점멸 longPress 플래그
bool doorOpen = true;
char temp[4] = {
    0,
}; // 비밀번호 설정시 임시 저장변수
//char password[3] = "123";  // 초기 비밀번호 및 사용자 비밀번호 저장 변수
char password[3];
char inputCode[3] = "000"; // 검증을 위한 사용자 입력 비밀번호 저장 배열
uint8_t codeIndex = 0;     // 비밀번호 문자열 배열 index

int piezo = 2;
int tones[] = {523, 587, 659, 698, 784, 880, 988, 1046};

bool kmg;

unsigned long off_time = 0;

void setup()
{
    Serial.begin(9600);
    keypad.addEventListener(keypadEvent); // 키패드 이밴트 관리자 설정
    keypad.setHoldTime(1000);
    SPI.begin();
    rfid.PCD_Init();
    lcd.init();
    lcd.backlight();
    lcd.print("Press # for 1s");
    pinMode(5, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);
    myservo.attach(A5); //서보모터 핀 설정 A5
    myservo.write(0);
    kmg = false;
    lcd.setCursor(0, 1);
    lcd.print("door open  ");
    pinMode(3, OUTPUT);
    pinMode(piezo, OUTPUT);
    pinMode(12, INPUT);
    pinMode(13, INPUT);
}

void loop()
{

    char key = keypad.getKey();
    if (longPress)
    { // password 설정
        if (key)
        {
            if (key != '*' && key != '#' && codeIndex < 3)
            { // 숫자이면, 3자리 이상 무시
                Serial.print("pass set key: ");
                Serial.println(key);
                temp[codeIndex] = key;
                lcd.setCursor(13 + codeIndex, 0);
                lcd.print(key);
                codeIndex++;
            }

            else if (key == '#')
            { // password 저장
                for (int i = 0; i < 3; i++)
                    password[i] = temp[i];
                longPress = false;
                key = '\0'; // 입력된 '#' 삭제 - 오류방지
                codeIndex = 0;
                doorOpen = false; // 자동으로 잠기는 문 표시
                Serial.println("set new password");
                lcd.setCursor(0, 1);
                lcd.print("door closed");
                digitalWrite(5, HIGH);
                digitalWrite(6, HIGH);
                digitalWrite(7, HIGH);
                off_time = millis() + 3000;
                myservo.write(0);
                kmg = false;
                digitalWrite(3, HIGH);
                for (int i = 8; i >= 0; i--)
                {
                    tone(piezo, tones[i]);
                    delay(50);
                }
                noTone(piezo);
                delay(500);
            }

            else if (key == '*')
            { // 입력값 초기화
                lcd.setCursor(13, 0);
                lcd.print("      ");
                for (int i = 0; i < 3; i++)
                    temp[i] = '0';
                key = '\0'; // 입력된 '#' 삭제 - 오류방지
                codeIndex = 0;
                Serial.println("CLEAR");
            }
        }
    }
    else
    {
        if (key)
        {
            if (key != '*' && key != '#' && doorOpen == true)
            { // 숫자만 입력
                Serial.print("key: ");
                Serial.println(key);
                lcd.setCursor(0, 0);
                lcd.print("                         ");
                lcd.print("Room Number: "); // 키 홀드 시간 설정 - 2000 = 2초
                inputCode[codeIndex] = key;
                lcd.setCursor(13 + codeIndex, 0);
                lcd.print(key);
                codeIndex++;
                if (codeIndex == 3)
                {
                    if (strncmp(password, inputCode, 3) == 0)
                    {
                        Serial.println("door closed");
                        lcd.setCursor(0, 1);
                        lcd.print("door closed");
                        digitalWrite(5, LOW);
                        digitalWrite(6, LOW);
                        digitalWrite(7, LOW);
                        myservo.write(0);
                        kmg = false;
                        doorOpen = false;
                        codeIndex = 0;
                        digitalWrite(3, HIGH);
                        for (int i = 8; i >= 0; i--)
                        {
                            tone(piezo, tones[i]);
                            delay(50);
                        }
                        noTone(piezo);
                        delay(500);
                    }
                    else
                    {
                        Serial.println("wrong number");
                        codeIndex = 0;
                    }
                    for (int i = 0; i < 3; i++)
                        inputCode[i] = '0';
                }
            }
            else if (key == '*')
            { // 입력값 초기화
                lcd.setCursor(13, 0);
                lcd.print("      ");
                Serial.println("CLEAR");
                for (int i = 0; i < 3; i++)
                    inputCode[i] = '0';
                key = '\0'; // 입력된 '#' 삭제 - 오류방지
                codeIndex = 0;
            }
        }
    }

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
    {
        String content = ""; // 문자열 자료형 content 선언

        for (byte i = 0; i < rfid.uid.size; i++)
        {
            // 문자열을 string에 추가
            content.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
            content.concat(String(rfid.uid.uidByte[i], HEX));
        }
        String result = content.substring(1);
        Serial.println(result);
        if (doorOpen && kmg)
        {
            myservo.write(0);
            kmg = false;
            delay(300);
        }
        else if (doorOpen && !kmg)
        {
            myservo.write(180);
            kmg = true;
            delay(300);
        }
        else
        {
            if (password[0] == '3' && password[1] == '0' && password[2] == '1' && result == "ba 6d 3d 29" && kmg)
            {
                Serial.println("door open      ");
                lcd.setCursor(0, 1);
                lcd.print("door open         ");
                digitalWrite(5, LOW);
                digitalWrite(6, LOW);
                digitalWrite(7, LOW);
                myservo.write(0);
                kmg = false;
                doorOpen = true;
                codeIndex = 0;
                digitalWrite(3, LOW);
                for (int i = 8; i >= 0; i--)
                {
                    tone(piezo, tones[i]);
                    delay(50);
                }
                noTone(piezo);
                delay(500);
                lcd.setCursor(13, 0);
                lcd.print("      ");
            }
            else if (password[0] == '3' && password[1] == '0' && password[2] == '1' && result == "ba 6d 3d 29" && !kmg)
            {
                Serial.println("door open      ");
                lcd.setCursor(0, 1);
                lcd.print("door open         ");
                digitalWrite(5, LOW);
                digitalWrite(6, LOW);
                digitalWrite(7, LOW);
                myservo.write(180);
                kmg = true;
                doorOpen = true;
                codeIndex = 0;
                digitalWrite(3, LOW);
                for (int i = 0; i < 8; i++)
                {
                    tone(piezo, tones[i]);
                    delay(50);
                }
                noTone(piezo);
                delay(500);
                lcd.setCursor(13, 0);
                lcd.print("      ");
            }
            if (result == "c7 71 22 19" && kmg)
            {
                Serial.println("door open");
                lcd.setCursor(0, 1);
                lcd.print("door open         ");
                digitalWrite(5, LOW);
                digitalWrite(6, LOW);
                digitalWrite(7, LOW);
                myservo.write(0);
                kmg = false;
                doorOpen = true;
                codeIndex = 0;
                digitalWrite(3, LOW);
                for (int i = 8; i >= 0; i--)
                {
                    tone(piezo, tones[i]);
                    delay(50);
                }
                noTone(piezo);
                delay(500);
                lcd.setCursor(13, 0);
                lcd.print("      ");
            }
            else if (result == "c7 71 22 19" && !kmg)
            {
                Serial.println("door open");
                lcd.setCursor(0, 1);
                lcd.print("door open         ");
                digitalWrite(5, LOW);
                digitalWrite(6, LOW);
                digitalWrite(7, LOW);
                myservo.write(180);
                kmg = true;
                doorOpen = true;
                codeIndex = 0;
                digitalWrite(3, LOW);
                for (int i = 0; i < 8; i++)
                {
                    tone(piezo, tones[i]);
                    delay(50);
                }
                noTone(piezo);
                delay(500);
                lcd.setCursor(13, 0);
                lcd.print("      ");
            }
        }
    }
    if ((digitalRead(13) == HIGH) && doorOpen && kmg)
    {
        myservo.write(0);
        kmg = false;
        delay(300);
    }
    else if ((digitalRead(13) == HIGH) && doorOpen && !kmg)
    {
        myservo.write(180);
        kmg = true;
        delay(300);
    }

    if (millis() > off_time)
    {
        digitalWrite(5, LOW);
        digitalWrite(6, LOW);
        digitalWrite(7, LOW);
        off_time = 0;
    }
}

void keypadEvent(KeypadEvent key)
{ 
    if (doorOpen == true)
    { // 문 열린 상태에서 키패드 이벤트 진입
        switch (keypad.getState())
        {
        case HOLD:
            if (key == '#')
            {
                longPress = true; // 비밀번호 변경코드 진입
                key = '\0';       // 입력된 '#' 삭제 - 오류방지
                codeIndex = 0;
                Serial.println("set pass in");
                lcd.setCursor(0, 0);
                lcd.print("Room Number: "); // 키 홀드 시간 설정 - 2000 = 2초
                lcd.setCursor(13, 0);
                lcd.print("   ");
                break;
            }
            else
            {
                lcd.setCursor(0, 0);
                lcd.print("Room Number: ");
                lcd.setCursor(13, 0);
                lcd.print(password[0]);
                lcd.setCursor(14, 0);
                lcd.print(password[1]);
                lcd.setCursor(15, 0);
                lcd.print(password[2]);
                if (doorOpen)
                {
                    lcd.setCursor(0, 1);
                    lcd.print("door open    ");
                }
                else
                {
                    lcd.setCursor(0, 1);
                    lcd.print("door closed   ");
                }
            }
            // switch (key)
            // {
            // case '#':
            //     longPress = true; // 비밀번호 변경코드 진입
            //     key = '\0';       // 입력된 '#' 삭제 - 오류방지
            //     codeIndex = 0;
            //     Serial.println("set pass in");
            //     lcd.setCursor(0, 0);
            //     lcd.print("Room Number: "); // 키 홀드 시간 설정 - 2000 = 2초
            //     lcd.setCursor(13, 0);
            //     lcd.print("   ");
            //     break;
            // }
            // break; // HOLD 종료
        }
    }
}