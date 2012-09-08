/*
 * IRUniversal: record and play back IR signals with using differents types of sensors.
 * I hope that this might be useful to handicapped persons.
 *
 * Save the codes in the EEPROM from deletion by turning off the device
 * It can be used with many differents sensor. 
 * An IR detector/demodulator must be connected to the input RECV_PIN.
 * An IR LED must be connected to the output PWM pin 3. MANDATORY!!!
 * A visible LED can be connected to STATUS_PIN to provide status.
 *
 * The logic is:
 * If button1 and button2 are pressed simultaneously the user will send 4 code
 * with the original remote control to be registered in IRUniversal
 * If the button 1 is pressed, send the IR code 1.
 * If the button 2 is pressed, send the IR code 2.
 * To use any different sensor overwrite sensorProcess method.
 *
 * Version 1.1 August, 2012
 * This code is in the public domain. Antonio Garcia Figueras
 * http://proyectosantonio.blogspot.com.es/
 *
 * Credits: IRremote library. Ken Shirriff. http://www.arcfn.com/2009/08/multi-protocol-infrared-remote-library.html 
 *     and http://www.arcfn.com/2009/09/arduino-universal-remote-record-and.html  
 *     Wiichuck http://todbot.com/blog/2008/02/18/wiichuck-wii-nunchuck-adapter-available/
 */

#include <IRremote.h> //See http://www.arcfn.com/2009/08/multi-protocol-infrared-remote-library.html
#include <EEPROM.h>
#include <EEPROMAnything.h>// http://arduino.cc/playground/Code/EEPROMWriteAnything
#include "pitches.h"

//**************Tonos del buzzer

 // notes in the melody:
int melody[] = {NOTE_C4, NOTE_G3,NOTE_G3, NOTE_A3, NOTE_G3,0, NOTE_B3, NOTE_C4};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 8, 8, 4,4,4,4,4 };
  
int BUZZER_PIN = 9;
//************* Fin tonos
  
boolean isOnPause = true; //Sirve para saber si está en pausa

int RECV_PIN = 12; //IR detector/demodulator
int STATUS_PIN = 13; //Visible LED

const int CODES_NUMBER = 4; //Number of infrared codes to store
const int BUTTONS_NUMBER = 2; //Number of buttons. 
int inputs[BUTTONS_NUMBER]={11, 10}; //Buttons ports

IRrecv irrecv(RECV_PIN);
IRsend irsend;

decode_results results;

typedef struct
{
  int codeType; // The type of code
  unsigned long codeValue; // The code value if not raw
  unsigned int rawCodes[RAWBUF]; // The durations if raw
  int codeLen; // The length of the code
  int toggle; // The RC5/6 toggle state
} record_type;

record_type record[CODES_NUMBER]; 

// Interval between the beginning of a recording and the beginning of the next recording
// Out record size is 162 bytes. So we will not wast eeprom space.
const int INTERVAL_EEPROM = 162;

//*****************************************************************************************
// ONLY with IR distance sensor (begin)                                                   *
// Checking an object in sensor range to send only one code                               *
//*****************************************************************************************
// El orden en el que hay que grabar los comandos debe ser: pausa, play, fastforward, backforward

const int DISTANCE_THRESHOLD = 200; //Higher values will denote object proximity 
int sensorPin = A0; //IR distance sensor                                                 
int sensorValue = 0;  // to store sensor value               
int firstAverage = 0;
int secondAverage = 0;
int lectura1, lectura2, lectura3;

//Checking an object with the IR distance sensor
void sensorProcess(){ 
  firstAverage = media3lecturas();
  Serial.print("*******firstAverage: ");
  Serial.println(firstAverage);

  if (firstAverage==999){
    return; //Ha habido algún error y terminamos
  }
  if (firstAverage > DISTANCE_THRESHOLD){
    //We have detected anything
    musiquitaBreve();
//    delay(1000); //wait 1 second
    secondAverage  = media3lecturas();
    while (abs(firstAverage-secondAverage) < 100){
      //Mientras no se mueva mucho seguimos leyendo
      secondAverage  = media3lecturas();
    }
    Serial.print("**2222*secondAverage: ");
    Serial.println(secondAverage);
    tone(BUZZER_PIN, NOTE_DS8);
    noTone(BUZZER_PIN);
    if (secondAverage==999){
      return; //Ha habido algún error y terminamos
    }
    if (secondAverage <= DISTANCE_THRESHOLD){
     //We haven't detected nothing in the second readings 
        if (!isOnPause){
          Serial.println("Envio comando PAUSE");
          sendCode(0); //Sending first stored code (pause)
          isOnPause=true; //to next time
          musiquita();
        }else{
          Serial.println("Envio comando PLAY");
          sendCode(1); //Sending second stored code (play)
          isOnPause=false; //to next time
          musiquita();
        }     
    }else if (secondAverage > firstAverage){
      //Highter
      Serial.println("Envio comando BACKFORWARD");
      for(int i=0; i<5; i++){ 
        // sending the command 5 times to increase velocity
        sendCode(3); //Sending fourth stored code (BackForward) 
      }      
      sendCode(3); //Sending fourth stored code (BackForward)            
      isOnPause=false; //to next time      
      musiquita();
    }else{
      //Closer
      Serial.println("Envio comando FASTFORWARD");
      for(int i=0; i<6; i++){ 
        // sending the command 6 times to increase velocity
        sendCode(2); //Sending third stored code (FastForward)
      }
      isOnPause=false; //to next time    
      musiquita();     
    } 
  }
}


//Devuelve 999 si ha habido algun error y no debe considerarse válida la media
int media3lecturas(){
  do
  {
    lectura1=analogRead(sensorPin);
    delay(50); //delay between readings to avoid errors because of erratic reading
    lectura2=analogRead(sensorPin);
    delay(50); //delay between readings to avoid errors because of erratic reading
    lectura3=analogRead(sensorPin);
    delay(50);
//    if (lectura2 > (lectura1+75) || lectura2 < (lectura1-75) ||
//        lectura3 > (lectura1+75) || lectura3 < (lectura1-75)){
//          return 999; 
//        }
  //Alguna de las lecturas tiene demasiada diferencia y no lo consideramos válido
  }while (lectura2 > (lectura1+75) || lectura2 < (lectura1-75) ||
          lectura3 > (lectura1+75) || lectura3 < (lectura1-75));
  return (lectura1 + lectura2 + lectura3)/3; //solo salimos si las tres lecturas son similares
}


//*****************************************************************************************
// ONLY with IR distance sensor (end)                                                     *
//*****************************************************************************************

//-----------------------------------------------------------------------------------------------

//*****************************************************************************************
// ONLY with WII Nunchuck (begin)                                                         *
// Using Nunchuck joystick. It's necesary nunchuck_funcs.h                                *
// See: http://todbot.com/blog/2008/02/18/wiichuck-wii-nunchuck-adapter-available/        *
//*****************************************************************************************

//#include <Wire.h>
//#include "nunchuck_funcs.h"
//int joy_x_axis;
//int joy_y_axis;
//int loop_cnt=0;
//
//void sensorProcess(){
//    nunchuck_get_data();
//    joy_x_axis = nunchuck_buf[0];
//    joy_y_axis = nunchuck_buf[1];  
//    
//    //            nunchuck_print_data();
//    
//    if (joy_x_axis >= 42 && joy_x_axis <= 192 && joy_y_axis >= 196){
//    // Serial.println("!!!!! North !!!!!");
//       sendCode(0);
//    }       
//    if (joy_x_axis >= 46 && joy_x_axis <= 188 && joy_y_axis <= 60){
//    // Serial.println("!!!!! South !!!!!");
//       sendCode(1);
//    } 
//    if (joy_x_axis <= 42 && joy_y_axis <= 196 && joy_y_axis >= 60){
//    // Serial.println("!!!!! West !!!!!");
//       sendCode(2);
//    }  
//    if (joy_x_axis >= 192 && joy_y_axis <= 196 && joy_y_axis >= 60){
//    // Serial.println("!!!!! East !!!!!");
//       sendCode(3);
//    }        
//}
//*****************************************************************************************
// ONLY with WII Nunchuck (end)                                                           *
//*****************************************************************************************



void setup()
{
  Serial.begin(9600);
  pinMode(STATUS_PIN, OUTPUT); //Visible LED
  for(int i=0; i < BUTTONS_NUMBER; i++){
    pinMode(inputs[i], INPUT); //Register button
  }
  readEEPROMCodes(); //Read previously EEPROM stored codes
  
  
//  //************ ONLY with WII Nunchuck
////    nunchuck_setpowerpins();
////    nunchuck_init(); // send the initilization handshake
//  //**************
}


//----------------------------------------------------------------------------------------
//*****************************************************************************************
// Common code for any sensor. Isn't necesary to change nothing                           *
// from here to the end of the code                                                       *
//*****************************************************************************************

void loop() {
  buttonsProcess();
  sensorProcess(); 
}




//Reading from EEPROM and storing in record
void  readEEPROMCodes(){
  for (int i=0; i < CODES_NUMBER; i++){
    EEPROM_readAnything(i*INTERVAL_EEPROM, record[i]);
    Serial.print("Reading. Type=");
    Serial.print(record[i].codeType);
    Serial.print(" Value=");
    Serial.print(record[i].codeValue);
    Serial.print(" Index=");
    Serial.println(i, DEC);    
  }
}


// Stores the code for later playback
// Most of this code is just logging
void storeCode(decode_results *results, int index) {
  record[index].codeType = results->decode_type;
  int count = results->rawlen;
  if (record[index].codeType == UNKNOWN) {
    Serial.println("Received unknown code, saving as raw");
    record[index].codeLen = results->rawlen - 1;
    // To store raw codes:
    // Drop first value (gap)
    // Convert from ticks to microseconds
    // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
    for (int i = 1; i <= record[index].codeLen; i++) {
      if (i % 2) {
        // Mark
        record[index].rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK - MARK_EXCESS;
        Serial.print(" m");
      } 
      else {
        // Space
        record[index].rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK + MARK_EXCESS;
        Serial.print(" s");
      }
      Serial.print(record[index].rawCodes[i - 1], DEC);
    }
    Serial.println("");
  }
  else {
    if (record[index].codeType == NEC) {
      Serial.print("Received NEC: ");
      if (results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
        Serial.println("repeat; ignoring.");
        return;
      }
    } 
    else if (record[index].codeType == SONY) {
      Serial.print("Received SONY: ");
    } 
    else if (record[index].codeType == RC5) {
      Serial.print("Received RC5: ");
    } 
    else if (record[index].codeType == RC6) {
      Serial.print("Received RC6: ");
    } 
    else {
      Serial.print("Unexpected codeType ");
      Serial.print(record[index].codeType, DEC);
      Serial.println("");
    }
    Serial.println(results->value, HEX);
    record[index].codeValue = results->value;
    record[index].codeLen = results->bits;
  }
  int bytesTransfered;
  bytesTransfered = EEPROM_writeAnything(INTERVAL_EEPROM*index, record[index]); //Storing in EEPROM
  
  Serial.print("Stored. Type=");
  Serial.print(record[index].codeType);
  Serial.print(" Value=");
  Serial.print(record[index].codeValue);
  Serial.print(" index=");
  Serial.println(index, DEC);
  Serial.print("bytesTransfered=");
  Serial.println(bytesTransfered);
}

// Send infrared code
void sendCode(int index) {
  Serial.println("Sending command");
  digitalWrite(STATUS_PIN, HIGH); //Visible LED on
  if (record[index].codeType == NEC) {
      irsend.sendNEC(record[index].codeValue, record[index].codeLen);
      Serial.print("Sent NEC ");
      Serial.println(record[index].codeValue, HEX);
  } 
  else if (record[index].codeType == SONY) {
    irsend.sendSony(record[index].codeValue, record[index].codeLen);
    Serial.print("Sent Sony ");
    Serial.println(record[index].codeValue, HEX);
  } 
  else if (record[index].codeType == RC5 || record[index].codeType == RC6) {
    // Put the toggle bit into the code to send
    record[index].codeValue = record[index].codeValue & ~(1 << (record[index].codeLen - 1));
    record[index].codeValue = record[index].codeValue | (record[index].toggle << (record[index].codeLen - 1));
    if (record[index].codeType == RC5) {
      Serial.print("Sent RC5 ");
      Serial.println(record[index].codeValue, HEX);
      irsend.sendRC5(record[index].codeValue, record[index].codeLen);
    } 
    else {
      irsend.sendRC6(record[index].codeValue, record[index].codeLen);
      Serial.print("Sent RC6 ");
      Serial.println(record[index].codeValue, HEX);
    }
  } 
  else if (record[index].codeType == UNKNOWN /* i.e. raw */) {
    // Assume 38 KHz
    irsend.sendRaw(record[index].rawCodes, record[index].codeLen, 38);
    Serial.println("Sent raw");
  }
  delay(300); // Wait a bit between retransmissions  
  digitalWrite(STATUS_PIN, LOW); //Visible LED off
}


//Receives the codes to be stored in the record array and in EEPROM
void receiveCodes(){
  irrecv.enableIRIn(); // Start the receiver
  blink(); //LED warning start codes recording
  int recordingIndex=0;
  irrecv.resume(); // resume receiver
  //we will record all codes
  while (recordingIndex < (CODES_NUMBER)){
    Serial.print("Waiting for code number ");
    Serial.println(recordingIndex, DEC);
    if (irrecv.decode(&results)) {
      Serial.println("Storing code");
      digitalWrite(STATUS_PIN, HIGH);
      storeCode(&results, recordingIndex++);
      irrecv.resume(); // resume receiver
      digitalWrite(STATUS_PIN, LOW);
      delay(1000); //Pause before store next code
    }   
  } 
  blink(); //LED warning finish codes recording
}


//Visible LED blinking
void blink(){
 for(int x=0; x<4; x++){
   digitalWrite(STATUS_PIN, HIGH);
   delay(300);
   digitalWrite(STATUS_PIN, LOW);
   delay(300);
 }
}


void buttonsProcess(){
  //If buttons 1 and 2 are simultaneasly pressed we will record IR codes
  codesReceiveProcess(); 
  
  //If any button is pressed send code
  for(int i=0; i<BUTTONS_NUMBER; i++){
   if(digitalRead(inputs[i]) == HIGH){
     //if button 1 is pressed we will send code 1,
     //if button2 we will send code 2 and so on.
     Serial.print("Sending code:");
     Serial.println(i, DEC);
     sendCode(i);
   } 
  }
}


//If button1 and button2 are pressed we will start codes recording
void codesReceiveProcess(){
  if (digitalRead(inputs[0]) == HIGH && digitalRead(inputs[1]) == HIGH){  
    receiveCodes();
  }
}

// Emite la musica por el buzzer
void musiquita(){
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < 8; thisNote++) {

    // to calculate the note duration, take one second 
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000/noteDurations[thisNote];
    tone(BUZZER_PIN, melody[thisNote],noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(BUZZER_PIN);
//    digitalWrite(BUZZER_PIN, LOW);
  } 
}

  // Emite la musica breve por el buzzer
void musiquitaBreve(){
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < 1; thisNote++) {

    // to calculate the note duration, take one second 
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000/noteDurations[thisNote];
    tone(BUZZER_PIN, melody[thisNote],noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(BUZZER_PIN);
//    digitalWrite(BUZZER_PIN, LOW);
  }
}
