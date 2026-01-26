#include <Keypad.h>

// ===== Keypad =====
const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {8, 9, 10, 11};
byte colPins[COLS] = {4, 5, 6, 7};

Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(9600);

  // Зменшуємо агресивність keypad
  keypad.setDebounceTime(25);
  keypad.setHoldTime(500);
}

void loop() {
  char customKey = customKeypad.getKey();

  if (customKey){
    if(customKey == 'D'){
      // "Enter"
      Serial.print("com_r"); 
    } 
    else {
      // кнопки
      Serial.print(customKey);
    }
  }
}

