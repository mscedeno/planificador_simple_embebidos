//Librerias para controlar pantalla LCD
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Incluímos la librería para poder controlar el servo
#include <Servo.h> 

#include "planificador.c"
 
// Declaramos la variable para controlar el servo
Servo servoMotor;

LiquidCrystal_I2C lcd(0x27,16,2); 

//define como se van rellenando cada barra
byte gauge_empty[8] =  {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000};    
byte gauge_fill_1[8] = {B10000, B10000, B10000, B10000, B10000, B10000, B10000, B10000};    
byte gauge_fill_2[8] = {B11000, B11000, B11000, B11000, B11000, B11000, B11000, B11000};    
byte gauge_fill_3[8] = {B11100, B11100, B11100, B11100, B11100, B11100, B11100, B11100};    
byte gauge_fill_4[8] = {B11110, B11110, B11110, B11110, B11110, B11110, B11110, B11110};    
byte gauge_fill_5[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};    
byte gauge_left[8] =   {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000};   
byte gauge_right[8] =  {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000};    

byte gauge_mask_left[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};  
byte gauge_mask_right[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};

byte char_blink[8] =   {B11111, B10001, B10001, B10001, B10001, B10001, B10001, B11111};    // left part of gauge - empty

byte gauge_left_dynamic[8];   // left part of gauge dynamic - will be set in the loop function
byte gauge_right_dynamic[8];  // right part of gauge dynamic - will be set in the loop function

int servo_gauge = 0;       // value for the CPU gauge
char buffer[10];         // helper buffer to store C-style strings (generated with sprintf function)
int move_offset = 0;     // used to shift bits for the custom characters

const int gauge_size_chars = 16;       // width of the gauge in number of characters
char gauge_string[gauge_size_chars+1]; // string that will include all the gauge character to be printed
   

#define PERIODO_POT 50
#define PERIODO_SERVO 50
#define PERIODO_LCD 50
#define PERIODO_inLCD 500
#define TICK 50

bool estadoLCD;
tTime now;
tTime LastScheduleTime;

int pot; //lectura del potenciometro
int servoPos; //posicion del servomotor
int valor;

//4 tareas requeridas
struct Task *tareaPot; //encargada de leer el potenciometro
struct Task *tareaServo; //cambiar posicion servo debido al potenciometro
struct Task *tareaLCD; //actualizar barra de carga y caracter 
struct Task *tareaInvLCD; //causa parpadeo de LCD

void ReadPot(void *args)
{
  pot = analogRead(A0); //lee la variacion del motor
  //Serial.println(pot);
}

void MoverServo(void *args)
{
  servoPos = map(pot,0,1023,0,180); //convierte la posicion del potenciometro a la posicion del servomotor
  servoMotor.write(servoPos);//cambia la posicion del servo
}

void InvertirLCD(void *args)
{
  if(estadoLCD){
    lcd.setCursor(0,1);              // move the cursor to the next line
    lcd.write(byte(7)); //caracter vacio
  }
  estadoLCD = !estadoLCD;
}

void updateLCD(void *args)
{
  servo_gauge = map(servoPos,0,180,0,100);
  if (estadoLCD) {
    float units_per_pixel = (gauge_size_chars*5.0)/100.0;        //  every character is 5px wide, we want to count from 0-100
  int value_in_pixels = round(servo_gauge * units_per_pixel);    // cpu_gauge value converted to pixel width

  int tip_position = 0;      // 0= not set, 1=tip in first char, 2=tip in middle, 3=tip in last char

  if (value_in_pixels < 5) {tip_position = 1;}                            // tip is inside the first character
  else if (value_in_pixels > gauge_size_chars*5.0-5) {tip_position = 3;}  // tip is inside the last character
  else {tip_position = 2;}                                                // tip is somewhere in the middle

  move_offset = 4 - ((value_in_pixels-1) % 5);      // value for offseting the pixels for the smooth filling

  for (int i=0; i<8; i++) {   // dynamically create left part of the gauge
     if (tip_position == 1) {gauge_left_dynamic[i] = (gauge_fill_5[i] << move_offset) | gauge_left[i];}  // tip on the first character
     else {gauge_left_dynamic[i] = gauge_fill_5[i];}                                                     // tip not on the first character
 
     gauge_left_dynamic[i] = gauge_left_dynamic[i] & gauge_mask_left[i];                                 // apply mask for rounded corners
  }

  for (int i=0; i<8; i++) {   // dynamically create right part of the gauge
     if (tip_position == 3) {gauge_right_dynamic[i] = (gauge_fill_5[i] << move_offset) | gauge_right[i];}  // tip on the last character
     else {gauge_right_dynamic[i] = gauge_right[i];}                                                       // tip not on the last character

     gauge_right_dynamic[i] = gauge_right_dynamic[i] & gauge_mask_right[i];                                // apply mask for rounded corners
  }  

  lcd.createChar(5, gauge_left_dynamic);     // create custom character for the left part of the gauge
  lcd.createChar(6, gauge_right_dynamic);    // create custom character for the right part of the gauge

  for (int i=0; i<gauge_size_chars; i++) {  // set all the characters for the gauge
      if (i==0) {gauge_string[i] = byte(5);}                        // first character = custom left piece
      else if (i==gauge_size_chars-1) {gauge_string[i] = byte(6);}  // last character = custom right piece
      else {                                                        // character in the middle, could be empty, tip or fill
         if (value_in_pixels <= i*5) {gauge_string[i] = byte(7);}   // empty character
         else if (value_in_pixels > i*5 && value_in_pixels < (i+1)*5) {gauge_string[i] = byte(5-move_offset);} // tip
         else {gauge_string[i] = byte(255);}                        // filled character
      }
  }
  lcd.setCursor(0,0);              // move the cursor to the next line
  lcd.print(gauge_string);         // display the gauge
  lcd.setCursor(0,1);              // move the cursor to the next line
  lcd.write(byte(0));
  }
}

void setup() {
  //display.begin(SSD1306_SWITCHCAPVCC, 0x27);
  //lcd.begin(16,2);

  Serial.begin(9600);
  // Iniciamos el servo para que empiece a trabajar con el pin 9
  servoMotor.attach(9);
  // Inicializamos al ángulo 0 el servomotor
  servoMotor.write(0);

  estadoLCD = false;

  lcd.init();                       // initialize the 16x2 lcd module
  lcd.createChar(7, gauge_empty);   // middle empty gauge
  lcd.createChar(1, gauge_fill_1);  // filled gauge - 1 column
  lcd.createChar(2, gauge_fill_2);  // filled gauge - 2 columns
  lcd.createChar(3, gauge_fill_3);  // filled gauge - 3 columns
  lcd.createChar(4, gauge_fill_4);  // filled gauge - 4 columns 
  lcd.createChar(0, char_blink); 
  lcd.backlight();  

  tTime now = TimeNow() + 1;

  //Crea e inicializa las tareas periodicas, excepto la tarea de lectura del sensor de distancia
  tareaPot=SchedulerAddTask(now, 0, PERIODO_POT, ReadPot);
  TaskEnable(tareaPot);

  tareaServo = SchedulerAddTask(now, 0, PERIODO_SERVO, MoverServo);
  TaskEnable(tareaServo);

  tareaInvLCD = SchedulerAddTask(now, 0, PERIODO_inLCD, InvertirLCD);
  TaskEnable(tareaInvLCD);

  tareaLCD = SchedulerAddTask(now, 0, PERIODO_LCD, updateLCD);
  TaskEnable(tareaLCD);

  LastScheduleTime = now;
}

void loop() {


  //Revisa si ha transcurrido un TICK
  if (TimePassed(LastScheduleTime) > TICK) {
    //Ejecuta el planificador
    SchedulerRun();
    LastScheduleTime = TimeNow();
  }
}
