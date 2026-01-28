#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <SdFat.h>

#define AUDIO_PIN PA3       // пін для PWM
#define SAMPLE_RATE 8000  // частота семплів, Гц

#define SD_SCK PB13
#define SD_MISO PB14
#define SD_MOSI PB15

// Terminal Window
#define WIN_X1 41
#define WIN_Y1 16
#define WIN_X2 126
#define WIN_Y2 62

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ПІНИ
#define OLED_DC     PA1
#define OLED_CS     PA2
#define OLED_RESET  PA0

// вертикальні обмеження тексту
#define LINE_HEIGHT 8
#define MAX_LINES 5

// горизонтальні обмеження
#define TEXT_X 42
#define TEXT_WIDTH (SCREEN_WIDTH - TEXT_X)

String inputBuffer = "";
int startY = 16; // пропуск жовтого (на олед дісплеї)
// масив для скролінгу та лічильник рядків
String displayBuffer[MAX_LINES];
int displayCount = 0;
int startbar = 1;
// створюємо об'єкт дисплея з пінів
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

// Створюємо окремий об'єкт SPI для другого порту
SPIClass mySPI2(2); // порт SPI2
SdFat sd2(&mySPI2); 

#define SD_CS PB12   // Chip Select

int scripter = 0;

char nextScriptPath[64];


#define MAX_FILES 20       // Макс. кількість файлів у одній папці
#define MAX_NAME_LEN 24    // Макс. довжина файлу для відображення

struct FileEntry {

  char name[MAX_NAME_LEN];
  bool isDir;
};

FileEntry menuItems[MAX_FILES];
int fileCount = 0;
int selectedIdx = 0;
char currentPath[64] = "/"; // Активна директорія



// приклад псевдонімів для пінів
int pinFromAlias(const char* p) {
    if (strcmp(p, "P1") == 0) return PB0;
    if (strcmp(p, "P2") == 0) return PB1;
    // ... додати інші піни
    return -1;  // якщо не знайдено
}
String uartData = ""; 

int sms = 0;// messages from uart3

void setup() {  
  // примусово налаштовуємо швидкість SPI для STM32
  SPI.setClockDivider(SPI_CLOCK_DIV8); // Знижуємо швидкість для стабільності
  SPI.begin();
  Serial.begin(9600);
  Serial3.begin(9600);  
  // Ініціалізація з перевіркою (0x3C тут не потрібен для SPI, але метод викликаємо так)
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    for(;;); // Якщо дисплей не знайдено, програма зациклиться тут
  }
  StartScreen();

// microsd
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); // CS неактивний

  // 3. Ініціалізуємо SPI2
  mySPI2.begin();
  Serial.println("Try to init SD on SPI2...");

  // ініціалізація (1  мгц)
   if (!sd2.begin(SD_CS, SD_SCK_MHZ(18))) {
    // код помилки
    Serial.print("SD Er Code: ");
    Serial.println(sd2.card()->errorCode(), HEX);
    Serial.print("SD Er Data: ");
    Serial.println(sd2.card()->errorData(), HEX);
  } else {
    Serial.println("SD OK");
  }//// microsd  


  mainmenu();
  display.display();
  // Рамка інтерфейс 
}

void mainmenu(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(1,1,127,63,WHITE);
  display.drawRect(1,1,40,63,WHITE);
  display.setCursor(3,5);
  display.print("VF ._.");
  display.drawLine(1,15,127,15,WHITE);
  display.setCursor(3,16);
  display.println("V_005");

  
}

// Допоміжна малює інтерфейс файлового менеджера
void drawMenu() {
    display.clearDisplay();

    display.setCursor(0, 0);
    display.print("Path:"); display.print(currentPath);
    display.drawLine(0, 16, 128, 16, WHITE);

    // Вираховуємо, з якого елемента починати малювати (прокрутка)
    int startIdx = 0;
    if (selectedIdx >= 4) {
        startIdx = selectedIdx - 3; // Тримаємо вибраний пункт в полі зору
    }

    for (int i = 0; i < 4; i++) {
        int itemIdx = startIdx + i;
        if (itemIdx >= fileCount) break; // Вихід, якщо файли закінчилися

        display.setCursor(0, 20 + (i * 10));
        
        // Малюємо стрілочку, якщо цей індекс збігається з обраним
        display.print((itemIdx == selectedIdx) ? "->" : "  ");
        
        if (menuItems[itemIdx].isDir) display.print("[D] ");
        display.print(menuItems[itemIdx].name);
    }
    display.display();
}

// Допоміжна зчитує SD
void scanDirectory(const char* path) {
    File root = sd2.open(path);

    if (!root || !root.isDirectory()) return;
    fileCount = 0;
    File entry;
    while (entry.openNext(&root, O_RDONLY)) {
        if (fileCount >= MAX_FILES) { entry.close(); break; }
        entry.getName(menuItems[fileCount].name, MAX_NAME_LEN);
        if (menuItems[fileCount].name[0] == '.') { entry.close(); continue; }
        menuItems[fileCount].isDir = entry.isDirectory();
        entry.close();
        fileCount++;
    }
    root.close();
    selectedIdx = 0;
}

bool confirmAction(const char* msg) { // меню підтвердження для файлів

  display.clearDisplay();
  display.drawRect(10, 10, 108, 44, WHITE); // Рамка вікна
  display.setCursor(15, 20);
  display.print(msg);
  display.setCursor(15, 40);
  display.print("A-Yes  C-No");
  display.display();

  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'A') return true;
      if (c == 'C') return false;
    }
  }
}

const char* p_c = "(c)2026";

void createNewFile() {
  inputBuffer = "";

  char newFileName[64];
  char finalName[40];

  // режим вводу назви
  display.clearDisplay();

  display.setCursor(0, 20);
  display.print("Waiting file name");
  display.display();

  bool nameReceived = false;
  // очікування воду імені
  while (!nameReceived) {
    if (Serial.available()) {
      String receivedChar = Serial.readString();
      // команда (Enter)


      if (receivedChar == "com_r" || receivedChar == "com_n") {
        if (inputBuffer.length() > 0) {
          nameReceived = true;
        }
      } 
      // якщо не команда виходу ввід у текст
      else if (receivedChar != "#") { 
        inputBuffer += receivedChar;
        
        // лог
        Serial.println(inputBuffer); 
        display.clearDisplay();
        display.setCursor(0, 10);
        display.print("Name:");
        display.setCursor(0, 30);
        display.print(inputBuffer);
        display.display();
      }
      else if (receivedChar == "#") {
        return; 
      }
    }
    delay(10);
  }

  // Отримуємо назву з inputBuffer
  // inputBuffer — глобальний масив введення
  if (inputBuffer.length() == 0) {
    strcpy(finalName, "noname"); 
  } else {
    // Використовуємо .c_str() для конвертації String у char*
    strncpy(finalName, inputBuffer.c_str(), 32);
    finalName[32] = '\0'; // Обмежуємо довжину
  }

  // Вибір розширення на екрані
  display.clearDisplay();
  display.setCursor(0, 20);
  display.print("Format for: ");
  display.setCursor(0, 30);
  display.print(finalName);
  display.setCursor(0, 50);
  display.print("1:.txt  2:.vfs"); // Використовуємо цифри для вибору
  display.display();

  char extension[5] = "";
  bool extensionSelected = false;

  // Чекаємо вибору формату
  while (!extensionSelected) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '1') {
        strcpy(extension, ".txt");
        extensionSelected = true;
      } else if (c == '2') {
        strcpy(extension, ".vfs");
        extensionSelected = true;
      } else if (c == 'C') { // Відміна створення
        return; 
      }
    }
  }

  // Формується повний шлях: папка + ім'я + розширення
  strcpy(newFileName, currentPath);
  if (newFileName[strlen(newFileName) - 1] != '/') strcat(newFileName, "/");
  strcat(newFileName, finalName);
  strcat(newFileName, extension);

  // Створення файлу через SdFat

  File f = sd2.open(newFileName, O_CREAT | O_WRITE);
  if (f) {
    f.println("#Created by VF_OS"); //Початковий заголовок
    f.close();
    
    display.clearDisplay();
    display.setCursor(0, 30);
    display.print("Success!");
    display.display();
    delay(800);
  } else {
    display.clearDisplay();
    display.setCursor(0, 30);
    display.print("SD Error!");
    display.display();
    delay(1000);
  }
  
  inputBuffer = ""; // Очищуємо буфер вже після створення файлу
  fileManager();
}


void PlayAudioPWM(const char* path) {

  // Відкриваємо файл
  File f = sd2.open(path);
  if (!f) return;

  // Налаштовуємо пін. Для STM32 краще використовувати режим PWM
  pinMode(AUDIO_PIN, PWM); 
  
  // Прискорюємо ШИМ (Таймер 2 для PA3), щоб прибрати свист
  Timer2.setPrescaleFactor(1); 
  Timer2.setOverflow(1023); // 10-бітний ШИМ

  const int BUF_SIZE = 512;
  uint8_t buffer[BUF_SIZE]; 

  // Розрахунок часу
  uint32_t interval = 1000000 / SAMPLE_RATE;
  uint32_t nextTick = micros();

  while (f.available()) {
    int bytesRead = f.read(buffer, BUF_SIZE);
    if (bytesRead <= 0) break;

    for (int i = 0; i < bytesRead; i++) {
      // Точний таймінг
      while (micros() < nextTick); 
      nextTick += interval;

      // Виводимо звук. 
      // Оскільки ми встановили Overflow(1023), множимо 8-бітний байт (0-255) на 4
      pwmWrite(AUDIO_PIN, buffer[i] * 4);
    }
    
    // Перевірка зупинки
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'C' || c == '#') break;
    }
    
    yield(); // Щоб SD-карта та система не "зависали"
  }

  // Вимикаємо звук
  pwmWrite(AUDIO_PIN, 0); 
  f.close();
}


// VF_OS-style зчитувач данних з виводом у термінал дісплея
void ReadFilePrint(const char* path) {
  mainmenu();
  File f = sd2.open(path);
  if (!f) {
    addDisplayLine("No file"); 
    return;
  }

  unsigned long lastPrint = millis();
  char buffer[32]; // Масив для символів (розмір має бути SCREEN_WIDTH / 6 + 1)
  uint8_t pos = 0;
  const uint8_t maxLen = SCREEN_WIDTH / 6;

  addDisplayLine("read..");

  while (f.available()) {
    char c = f.read();

    // Якщо кінець рядка, або буфер повний
    if (c == '\n' || pos >= maxLen - 1) {
      if (pos > 0) {
        buffer[pos] = '\0'; // Закриваємо рядок
        
        // Чекаємо 500мс (замість окремої функції wait500)
        while (millis() - lastPrint < 1000) {
          // система жива
        }
        
        addDisplayLine(buffer);
        lastPrint = millis();
        pos = 0;
      }
      if (c == '\n') continue; // Переходимо до наступного символу
    }
    
    // Ігноруємо символ повернення каретки \r та записуємо символ у буфер
    if (c != '\r') { 
      buffer[pos++] = c;
    }
  }

  // Друкуємо залишок тексту, якщо він є
  if (pos > 0) {
    buffer[pos] = '\0';
    while (millis() - lastPrint < 500);
    addDisplayLine(buffer);
  }

  f.close();
  addDisplayLine("Ok"); 
}


const char* p_p = "Project";

void com_manager() {
  
  addDisplayLine(inputBuffer.c_str());

  // тимчасовий масив для розділення строки
  char buf[48];
  inputBuffer.toCharArray(buf, 48);

  // розбір строки
  char* cmd = strtok(buf, " ");
  char* arg = strtok(NULL, " ");

  // clear
  if (strcmp(cmd, "C0") == 0) {
    clearDisplayBuffer();
    inputBuffer = "";
    return;
  }

  // VFScript
  else if (strcmp(cmd, "C1") == 0) {
    clearDisplayBuffer();
    addDisplayLine("VFScript");
    scripter = 1;
    inputBuffer = "";
    return;
  }
  // File Manager
  else if (strcmp(cmd, "C3") == 0) {
    fileManager();
    inputBuffer = "";
    return;
  }
  // open patch/file.vfs
  else if (strcmp(cmd, "C2") == 0) {
    if (arg != NULL) {
      runVFS(arg);
    } else {
      addDisplayLine("No path");
    }
    inputBuffer = "";
    return;
  }

  // Секретна команда на основі дати =--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  else if (strcmp(cmd, "15022009") == 0) {
      display.invertDisplay(true); // Ефектний візуальний відгук
    delay(100);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, 128, 64, WHITE);
  
    display.setCursor(10, 15);
    display.print("VF_OS Build 01:");
    display.setCursor(10, 30);
    display.print("Veil Fog Project");
  
    display.setCursor(10, 45);

    display.print("Auth: Kalachov Ruslan"); 
   
    display.display();
    delay(5000); // Відображати 5 секунд
    display.clearDisplay();
    mainmenu();
    inputBuffer = "";
    return;
  }

  // help
  else if (strcmp(cmd, "0A") == 0) {
    help_manager();
    inputBuffer = "";
    return;
  }
    // sms(uart3)
  else if (strcmp(cmd, "CA1") == 0) {
    if(sms == 0) {
    sms =1;
    addDisplayLine("sms on");
    } else if(sms == 1){
      sms == 0;
      addDisplayLine("sms off");
    }
    inputBuffer = "";
    return;
  }

  // error comand
  else {
    addDisplayLine("ERR");
    inputBuffer = "";
    return;
  }
}





const char* p_v = "Veil";

// меню допомоги
void help_manager(){
  ReadFilePrint("VF/com/hp1.txt");
  return;
}


void addDisplayLine(String text) {
  int maxChars = 14; // для textSize 1
  int start = 0;

  // цикл по тексту для автопереносу
  while (start < text.length()) {
    // беремо шматок тексту, який влізе в один рядок
    String line = text.substring(start, min(start + maxChars, text.length()));

    // скролінг
    if (displayCount == MAX_LINES) {
      for (int i = 1; i < MAX_LINES; i++) {
        displayBuffer[i-1] = displayBuffer[i];
      }
      displayBuffer[MAX_LINES-1] = line;
    } else {
      displayBuffer[displayCount++] = line;
    }

    start += maxChars; // беремо наступну частину рядка
  }

  // оновлюємо екран
  display.fillRect(TEXT_X, startY, SCREEN_WIDTH-TEXT_X-1, SCREEN_HEIGHT-startY-1 , BLACK);

  //display.drawRect(0, 0, 40, SCREEN_HEIGHT, SSD1306_WHITE);
  for (int i = 0; i < displayCount; i++) {
    display.setCursor(TEXT_X+1, 16 + i * LINE_HEIGHT); // координати скролінгу (відмальовка)
    display.println(displayBuffer[i]);
  }

  display.display();
}

// Розносимо частини назви по коду (можна покласти в різні місця файлу)
const char* p_os = "VF_OS";

const char* p_f = " Fog ";

void StartScreen() {
  display.clearDisplay();

  for(int i = 0; i < 250; i++) {
    int x = random(0, 128);
    int y = random(0, 64);
    if (y > random(0, 64)) {
      display.drawPixel(x, y, SSD1306_WHITE);
    }
  }

  display.drawLine(15, 10, 113, 10, SSD1306_WHITE); // Верх
  display.drawLine(15, 54, 113, 54, SSD1306_WHITE); // Низ
  display.drawLine(5, 20, 5, 44, SSD1306_WHITE);    // Лево
  display.drawLine(123, 20, 123, 44, SSD1306_WHITE); // Право
  

  display.drawLine(5, 20, 15, 10, SSD1306_WHITE);
  display.drawLine(113, 10, 123, 20, SSD1306_WHITE);
  display.drawLine(5, 44, 15, 54, SSD1306_WHITE);
  display.drawLine(113, 54, 123, 44, SSD1306_WHITE);

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(38, 16); 
  display.print(p_os);


  display.drawLine(40, 33, 88, 33, SSD1306_WHITE);

  //(Veil Fog Project)
  display.setTextSize(1);
  display.setCursor(18, 39);
  display.print(p_v); 
  display.print(p_f); 
  display.print(p_p);

  
  display.setCursor(15, 56);
  display.print(p_c);
  display.print(" ");
  
  display.print("IP"); 
  display.print("Rus");
  display.print("lan"); 
  
  display.drawPixel(0, 0, SSD1306_WHITE);
  display.drawPixel(127, 0, SSD1306_WHITE);
  display.drawPixel(0, 63, SSD1306_WHITE);
  display.drawPixel(127, 63, SSD1306_WHITE);

  display.display();
  delay(3500);
  display.clearDisplay();
}

// Очищення всього буфера дисплея ( С0 = clear )
void clearDisplayBuffer() {
  for (int i = 0; i < MAX_LINES; i++) displayBuffer[i] = "";
  displayCount = 0;

  display.fillRect(TEXT_X, startY, SCREEN_WIDTH-TEXT_X-1, SCREEN_HEIGHT-startY-1 , BLACK);
  display.display();
}
//---------------------------------------------------------
void loop() {
  display.fillRect(42, 5, 126-42-1, 14-5-1 , BLACK);
  display.setCursor(42,5);
  display.print(">"+inputBuffer);
  display.display();
  while (Serial.available() > 0) {
    

    String receivedChar = Serial.readString();

    // якщо Enter
    if (receivedChar == "com_r" || receivedChar == "com_n") {

      // ВИКОНУЄМО ТІЛЬКИ ЯКЩО Є КОМАНДА
      if (inputBuffer.length() > 0 && scripter == 0) {
        com_manager();
      }
      else if (inputBuffer.length() > 0 && scripter == 1) {
        vfs_manager();
      }
    }
    else {
      inputBuffer += receivedChar;
      Serial.println(inputBuffer);
      
    }
    
  
  }
  if (Serial3.available() && sms == 1) {
    char c = Serial3.read(); 
    //буфер uart3
    static String uartBuf = "";
    if (c == '\n') {
      addDisplayLine(uartBuf.c_str());
      uartBuf = "";
    } else if (c != '\r') {
      uartBuf += c;
    }
  }
  
}

// -----------------------------------------------------
void vfs_manager() {
  // вивід буфера
  addDisplayLine(inputBuffer.c_str());

  // команда на вихід (*0#)
  if (inputBuffer == "*0#") {
    addDisplayLine("Exit...");
    display.clearDisplay();
    mainmenu();
    inputBuffer = ""; // Очищуємо буфер
    scripter = 0;// 0 == system / 1 == VFScritpt
    return; 
  }

  // виконання якщо рядок не порожній
  if (inputBuffer.length() > 0) {
    // тимчасовий буфер char для executeCommand
    char buf[48]; 
    inputBuffer.toCharArray(buf, 48);
    
    executeCommand(buf); // передача команди
    
    inputBuffer = ""; 
  }
}

// 10 змінних (40 байт)
int v[10]; 
int getV(char* arg) {
  if (!arg) return 0;
  // чи починається аргумент з 'v' (наприклад, "v0")
  if (arg[0] == 'v' && arg[1] >= '0' && arg[1] <= '9') {
    return v[arg[1] - '0']; 
  }
  return atoi(arg); // якщо не змінна повертаємо як число
}

bool skipNext = false; 


int executeCommand(char* line) {
  if (skipNext) {
    skipNext = false; 
    return 0;        
  }

  String cmdLine = String(line);
  cmdLine.trim(); 

  if (cmdLine.startsWith("goto ")) {
    String targetPath = cmdLine.substring(5);
    targetPath.trim(); 
    strncpy(nextScriptPath, targetPath.c_str(), 63);
    nextScriptPath[63] = '\0'; 
    return 3; 
  }


  if (line[0] <= 32 || line[0] == '#') return 0; 

  char* cmd = strtok(line, " ");
  char* a1 = strtok(NULL, " ");
  char* a2 = strtok(NULL, " ");
  char* a3 = strtok(NULL, " ");
  char* a4 = strtok(NULL, " ");
  char* a5 = strtok(NULL, " ");

  if (!cmd) return 0;

  // --- ЛОГИКА IF ---
  if (strcmp(cmd, "ifV") == 0) {
    if (v[atoi(a1)] != getV(a2)) skipNext = true;
    return 0;
  }
  
  if (strcmp(cmd, "ifP") == 0) {
    if (digitalRead(atoi(a1)) != getV(a2)) skipNext = true;
    return 0;
  }
  // --- РОЗШИРЕНИЙ IF ---
  if (strcmp(cmd, "if") == 0) {
    // if <|>|= <v/num> <v/num>
    int left  = getV(a2);
    int right = getV(a3);
    char op = a1[0];

    bool cond = false;

    if (op == '<') cond = (left < right);
    else if (op == '>') cond = (left > right);
    else if (op == '=') cond = (left == right);

    if (!cond) skipNext = true;
    return 0;
  }

  // --- СИСТЕМНІ ---

  if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "*0#") == 0) return 1;
  if (strcmp(cmd, "loop") == 0) return 2;
  
  if (strcmp(cmd, "cls") == 0) {
    display.clearDisplay();
  }

  // --- МАТЕМАТИКА ---
  else if (strcmp(cmd, "calc") == 0) {
    // calc <dest_idx> <src_v_idx> <op> <val_or_v>
    // Приклад: calc 0 0 + 1 (v0++) або calc 0 1 * v2 (v0 = v1 * v2)
    int dest = atoi(a1);
    int s1 = v[atoi(a2)];
    char op = a3[0];
    int s2 = getV(a4);

    if (op == '+') v[dest] = s1 + s2;
    else if (op == '-') v[dest] = s1 - s2;
    else if (op == '*') v[dest] = s1 * s2;
    else if (op == '/') if(s2 != 0) v[dest] = s1 / s2;
  }

  // --- АНАЛОГОВІ СИГНАЛИ (Тільки для дозволених пінів: 16, 17, 3, 4) ---
  else if (strcmp(cmd, "anaR") == 0) {
    // Читаємо АЦП у змінну: anaR <v_idx> <pin>
    int p = atoi(a2);
    if (p == 16 || p == 17 || p == 3 || p == 4) {
      v[atoi(a1)] = analogRead(p);
    } else {
      v[atoi(a1)] = 0; // Якщо пін заборонений, повертаємо 0
    }
  }
  else if (strcmp(cmd, "anaW") == 0) {
    // ШІМ вихід (яскравість): anaW <pin> <0-255>
    int p = atoi(a1);
    if (p == 16 || p == 17 || p == 3 || p == 4) {
      analogWrite(p, getV(a2));
    }
  }

  // --- ГРАФІКА (З підтримкою змінних v0-v9) ---
  else if (strcmp(cmd, "pix") == 0) {
    display.drawPixel(getV(a1), getV(a2), getV(a3));
  }
  else if (strcmp(cmd, "rect") == 0) {
    display.drawRect(getV(a1), getV(a2), getV(a3), getV(a4), getV(a5));
  }
  else if (strcmp(cmd, "line") == 0) {
    display.drawLine(getV(a1), getV(a2), getV(a3), getV(a4), getV(a5));
  }
  else if (strcmp(cmd, "txt") == 0) {
    display.setCursor(getV(a1), getV(a2));
    // якщо аргумент змінна v0-v9 — друкуємо значення, інакше текст
    if (a3 && a3[0] == 'v' && a3[1] >= '0' && a3[1] <= '9') {
      display.print(getV(a3));
    } else {
      display.print(a3);
    }
  }
  // писати змінну
  else if (strcmp(cmd, "txtv") == 0) {
    display.setCursor(getV(a1), getV(a2));
    display.print(v[atoi(a3)]);
  } 



  // --- ЗАЛІЗО ---
  // --- ЗАЛІЗО (Дозволені тільки 16(PB0), 17(PB1), 3(PA3), 4(PA4)) ---
  else if (strcmp(cmd, "pinM") == 0) {
    int p = atoi(a1);
    // Дозволяємо лише конкретні номери пінів
    if (p == 16 || p == 17 || p == 3 || p == 4) {
      pinMode(p, (a2[0] == 'o') ? OUTPUT : INPUT);
    }
  } 
  else if (strcmp(cmd, "digital") == 0) {
    int p = atoi(a1);
    // Дозволяємо лише конкретні номери пінів
    if (p == 16 || p == 17 || p == 3 || p == 4) {
      digitalWrite(p, getV(a2));
    }
  }
  else if (strcmp(cmd, "del") == 0) {
    delay(getV(a1));
  } 
  else if (strcmp(cmd, "u3w") == 0) {
    // якщо аргумент змінна v0-v9 — друкуємо значення, інакше текст
    if (a1 && a1[0] == 'v' && a1[1] >= '0' && a1[1] <= '9') {
      Serial3.println(getV(a1));
    } else if (a1) {
      Serial3.println(a1);
    }
  }

  else if (strcmp(cmd, "u3t") == 0) {
    display.setCursor(getV(a1), getV(a2));
    display.setTextColor(1);
    display.setTextSize(1);
    if (uartData.length() > 0) display.print(uartData.c_str());
    else display.print("..."); 
  }
  else if (strcmp(cmd, "u3c") == 0) {
    uartData = ""; 
  }
  else if (strcmp(cmd, "u3b") == 0) {
    long baud = atol(a1);
    if (baud > 0) {
      Serial3.end();      
      Serial3.begin(baud);
    }
  }

  else if (strcmp(cmd, "keyR") == 0) {
    if (inputBuffer.length() > 0) {
      char c = inputBuffer[0];
      inputBuffer = ""; 
      if (c > 31) v[atoi(a1)] = (int)c; 
      else v[atoi(a1)] = 0; 
    } else {
      v[atoi(a1)] = 0;
    }
  }

  else if (strcmp(cmd, "beep") == 0) {
    // beep <pin> <hz/v> <ms/v>
    int p = atoi(a1);
    
    // Перевірка, чи дозволено використовувати цей пін (PB0, PB1, PA3, PA4)
    if (p == 16 || p == 17 || p == 3 || p == 4) {
      int f = getV(a2);
      int d = getV(a3);
      
      // Захист від ділення на нуль
      if (f > 0) {
        long delayMicros = 1000000 / f / 2; 
        long cycles = (long)f * d / 1000;
        
        pinMode(p, OUTPUT);
        for (long i = 0; i < cycles; i++) {
          digitalWrite(p, HIGH);
          delayMicroseconds(delayMicros);
          digitalWrite(p, LOW);
          delayMicroseconds(delayMicros);
        }
      }
    }
  }

  return 0;
}

void runVFS(const char* path) {

  inputBuffer = "";
  File f = sd2.open(path);
  if (!f) {
    addDisplayLine("Err: No file"); 
    return;
  }

  char buf[64]; 
  uint8_t pos = 0;
  bool isRunning = true; 
  char nextPath[64] = ""; // буфер для наступного файлу
  bool jump = false;      // прапор переходу

  addDisplayLine("VFS Start");

  while (isRunning) {
    // --- блок оживления Serial ---
    while (Serial.available() > 0) {
      char c = Serial.read();
      if (c > 31) inputBuffer += c; 
    }
    
    // --- фонове читання ---
    if (Serial3.available() > 0) {
      while (Serial3.available() > 0) {
        char c = Serial3.read();
        if (c > 31) { 
          uartData += c;
          Serial.print(c); 
        }
      }
    }

    if (!f.available()) {
      isRunning = false; 
      break;
    }

    char c = f.read();
    

    if (c == '\n' || pos >= sizeof(buf) - 1) {
      buf[pos] = '\0';
      
      if (pos > 0) {
        // executeCommand має повертати: 2-LOOP, 1-EXIT, 3-GOTO
        int result = executeCommand(buf); 
        display.display(); 
        
        if (result == 2) { // LOOP
          f.close();        
          f = sd2.open(path); 
          if (!f) isRunning = false; 
          pos = 0;
          continue; 
        } 
        else if (result == 1) { // EXIT
          isRunning = false; 
        }
        else if (result == 3) { // GOTO (Chain Load)
          // Отримуємо шлях з глобальної змінної, яку заповнив executeCommand
          strncpy(nextPath, nextScriptPath, 63); 
          jump = true;
          isRunning = false;
        }
      }
      pos = 0;
    } 
    else if (c > 31) { 
      buf[pos++] = c;
    }
  } 
  
  if (f) f.close(); // Закриваємо поточний файл перед виходом або переходом
  
  if (jump) {
    runVFS(nextPath); // Запускаємо наступний файл (пам'ять вже вільна)
    return; // Виходимо, щоб не викликати mainmenu завчасно
  }

  delay(1000);
  mainmenu(); 
  display.display(); 
}

// File manager головний цикл (керує процесом)
void fileManager() {
    strcpy(currentPath, "/");
    scanDirectory(currentPath);
    drawMenu();

    bool inManager = true;
    while (inManager) {
        if (Serial.available()) {
            char cmd = Serial.read();

            if (cmd == 'C'){
              inManager = false;
              mainmenu(); // Вихід
            }
            else if (cmd == '2') { // Вгору
                if (selectedIdx > 0) selectedIdx--;
                drawMenu();
            } 
            else if (cmd == '8') { // Вниз
                if (selectedIdx < fileCount - 1) selectedIdx++;
                drawMenu();
            } 
            else if (cmd == 'B') { // Корінь
                strcpy(currentPath, "/");
                scanDirectory(currentPath);
                drawMenu();
            }
            else if (cmd == 'A') { // Дія
                FileEntry *sel = &menuItems[selectedIdx];
                char fullPath[64];
                strcpy(fullPath, currentPath);
                if (fullPath[strlen(fullPath)-1] != '/') strcat(fullPath, "/");
                strcat(fullPath, sel->name);

                if (sel->isDir) {
                    strcpy(currentPath, fullPath);
                    scanDirectory(currentPath);
                    drawMenu();
                } else {
                    int len = strlen(sel->name);
                    if (len > 4 && strcasecmp(sel->name + len - 4, ".txt") == 0) {
                        ReadFilePrint(fullPath);
                        drawMenu(); // Повертаємо вигляд після читання
                    } else if (len > 4 && strcasecmp(sel->name + len - 4, ".vfs") == 0) {
                        runVFS(fullPath);
                        drawMenu();
                    } else if (len > 4 && strcasecmp(sel->name + len - 4, ".raw") == 0) {
                        PlayAudioPWM(fullPath);
                        drawMenu();
                    }
                }
            // видалення файлу 
            }
            else if (cmd == '#') { 
              FileEntry *sel = &menuItems[selectedIdx];
              char fullPath[64];
              strcpy(fullPath, currentPath);
              if (fullPath[strlen(fullPath)-1] != '/') strcat(fullPath, "/");
              strcat(fullPath, sel->name);

                if (confirmAction("Delete?")) {
                  if (sel->isDir) {
                    sd2.rmdir(fullPath); // Удалить папку (должна быть пустой)
                  } else {
                  sd2.remove(fullPath); // Удалить файл
                  }
                  scanDirectory(currentPath); // Обновляем список
                }
             drawMenu();
           }
           

       // створити файл
          else if (cmd == '*') {
               if (confirmAction("Create file?")) {
               createNewFile();
               scanDirectory(currentPath);
             }
             drawMenu();
            }
            
        } 
        delay(10); 
    } while (inManager)

    mainmenu();
} // fileManager
