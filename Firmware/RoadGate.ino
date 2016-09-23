#include <Debounce.h>
#include <SD.h>
#define LED_PIN		13//????????
//SD card/////////////////////////////////////////////////////////////////////////
const int chipSelect = 4;
File dataFile;
//RoadGateStateMachine////////////////////////////////////////////////////////////
#define PinKeyUp 3
#define PinKeyDown 5
#define PinInterlockUp 6
#define PinInterlockDown 7
#define PinMotorPowerOn 9
#define PinMotorDirectoin 8
#define PinOptoSafe A0
#define PinLearnKey A1

enum GateState {
  gateReady, gateDelay, gateUp, gateDown}; //Stop: ready to go up and down; Delay: wait for a moment, stop after that 
  
GateState inputState = gateReady;
unsigned int motorPowerOn = 0; 
unsigned int motorDirection = 0; 
Debounce dKeyUp = Debounce(20, PinKeyUp); // bounse filter in milliseconds 
Debounce dKeyDown = Debounce(20, PinKeyDown); 
Debounce dInterlockUp = Debounce(20, PinInterlockUp); 
Debounce dInterlockDown = Debounce(20, PinInterlockDown); 
Debounce dOptoSafe = Debounce(20, PinOptoSafe);
Debounce dLearnKey = Debounce(20, PinLearnKey);

const long GateDelayTime = 1000;     // delay (in milliseconds) before start motor, just after stop
unsigned long GateDelayPreviousMillis = 0; // store last time State machine was updated
//RECIEVER/////////////////////////////////////////////////////////////////////////
#define HCS_RECIEVER_PIN  2    // пин к которому подключен приемник для брелков
class HCS301 {
public:
unsigned BattaryLow :  
  1;  // На брелке села батарейка
unsigned Repeat :  
  1; // повторная посылка
unsigned BtnNoSound :  
  1;
unsigned BtnOpen :  
  1; 
unsigned BtnClose :  
  1; 
unsigned BtnRing :  
  1;
  unsigned long SerialNum;
  unsigned long Encript;
  void print();
};
volatile boolean	HCS_Listening = true;		
byte				HCS_preamble_count = 0;
uint32_t			HCS_last_change = 0;
//uint32_t			HCS_start_preamble = 0;
uint8_t				HCS_bit_counter;				// счетчик считанных бит данных
uint8_t				HCS_bit_array[66];				// массив считанных бит данных
#define				HCS_TE		400				// типичная длительность имульса Te
#define				HCS_Te2_3	600				// HCS_TE * 3 / 2
HCS301 hcs301;


///////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600); 
  //---------------------------------------
  pinMode(PinKeyUp, INPUT);
  pinMode(PinKeyDown, INPUT);
  pinMode(PinInterlockUp, INPUT);
  pinMode(PinInterlockDown, INPUT);
  pinMode(PinMotorPowerOn, OUTPUT);
  digitalWrite(PinMotorPowerOn, 0);
  pinMode(PinMotorDirectoin, OUTPUT);
  pinMode(PinOptoSafe, INPUT);

  //---------------------------------------
  pinMode(HCS_RECIEVER_PIN, INPUT); // Брелки
  attachInterrupt(0, HCS_interrupt, CHANGE);
  //---------------------------------------
  
  //--calibrate-to-up
  if(false){
  Serial.println("Starting to calibrate");
  dOptoSafe.update();
  while(dOptoSafe.read()==0){
    dOptoSafe.update();
  }
  Serial.println("OptoSafe is OK");
  
    dInterlockUp.update();
    dInterlockDown.update();
    if(!(dInterlockUp.read()||dInterlockDown.read())){
      digitalWrite(PinMotorDirectoin, 1);
      digitalWrite(PinMotorPowerOn, 1);//motorPowerOn 
      while(!(dInterlockUp.read()||dInterlockDown.read())){
        dInterlockUp.update();
        dInterlockDown.update();}
      }
      digitalWrite(PinMotorPowerOn, 0);//motorPowerOn 
  Serial.println("Calibrate finished");
  
  }
  //--calibrate-to-up
  
  
  pinMode(10, OUTPUT);// make sure that the default chip select pin is set to// output, even if you don't use it:
  // Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
   
  
  }
 else{
  Serial.println("SD card OK");//initialized
  dataFile = SD.open("ingress.txt",FILE_WRITE);
  //temp //need to read fil i dont know whiy
  if (dataFile) {
    /*
    while (dataFile.available()) {
      dataFile.read(id, 10);
      // Serial.println(id);
    }
    //  dataFile.close();
    */
  }  
  else {
    Serial.println("SETUP error opening ingress.txt");
  }
 } 
}


void readLine(char *buffer)
{
  bool EOL = false;
  char c;
  int idx=0;
  while (! EOL)
  {
    dataFile.read(&c, 1);//c = readCharSD();  / reads 1 char from SD
    if (c == '\n' || idx==19)  // prevent buffer overflow too..
    {
      buffer[idx] = 0;
      idx = 0;
      EOL = true;
    }
    else
    {
      buffer[idx] = c;
      idx++;
    }
  }
}

void SDLearnKey(long unsigned int  LearnKey){
  Serial.println("Try to learn key "+String(LearnKey));
  dataFile.println(String(LearnKey));
  dataFile.close();
  dataFile = SD.open("ingress.txt");
}

int IDisIngress(char* idtoFind)
{
  char id[20];
  int result;
  result = 0;
  /*while (!Serial) {//????????????
   ; // wait for serial port to connect. Needed for Leonardo only
   }*/
  if (dataFile) {
    dataFile.seek(0);
    while (dataFile.available()) {
      //dataFile.read(id, 10);
      //   Serial.println(String(id));
      readLine(id);
      if (strncmp(id, idtoFind, 7) == 0 ) {
        result = 1;
      }
    }
  }  
  else {// if the file isn't open, pop up an error:
    Serial.println("error opening ingress.txt");
  } 
  //  Serial.print("EOF");

  return result;
}



void loop() {
  unsigned long CurTime = millis();
  
  if(inputState == gateDelay){
    if (CurTime - GateDelayPreviousMillis >= GateDelayTime) { //delay and stop
      inputState = gateReady;
    }
    return;
  };

  //--проверяем наличие команды брелка----
  int result=0;
  HCS301 msg;  
  bool HCSget=false;
  if(HCS_Listening == false){
    HCSget=true;
    memcpy(&msg,&hcs301,sizeof(HCS301));
    HCS_Listening = true;// включаем слушанье брелков снова
    Serial.println(String("KeyFb#")+String(msg.SerialNum));
    Serial.println(String("BtnOpen#")+String(msg.BtnOpen));
    Serial.println(String("BtnClose#")+String(msg.BtnClose));
    //--поиск в файле доступа------------
    char idtoFind[8];
    String(msg.SerialNum).toCharArray(idtoFind,8);
    result = IDisIngress(idtoFind);
    if (result == 1) {     
      Serial.println(idtoFind);
    }
    //-----------------------------------
  }//end if(HCS_Listening == false)
  //---------------------------------------
  //--проверяем наличие команды брелка----
  dKeyUp.update ();
  dKeyDown.update ();
  dInterlockUp.update ();
  dInterlockDown.update ();
  dInterlockDown.update ();
  dLearnKey.update ();

  if(HCSget && dLearnKey.read() && (result==0)){
    SDLearnKey(msg.SerialNum);
  }
  
  if (dKeyUp.read() || (result==1 && msg.BtnOpen)) {
    switch (inputState) {
      case (gateReady):
      {
        inputState = gateUp;
      }
      break;
      case (gateDown):
      {
        inputState = gateDelay;
      }
      break;
    }
  }
  if (dKeyDown.read()|| (result==1 && msg.BtnClose)) {
    switch (inputState) {
      case (gateReady):
      {
        inputState = gateDown;
      }
      break;
      case (gateUp):
      {
        inputState = gateDelay;
      }
      break;
    }
  }
  
  if (dInterlockUp.read()){
    switch (inputState) {
      case (gateUp):   
      {
        inputState = gateReady;
      } 
      break;
      case (gateDown): 
      {
        inputState = gateDown;
      } 
      break;
    }
  }
  
  if (dInterlockDown.read()) {
    switch (inputState) {
      case (gateDown): 
      {
        inputState = gateReady;
      } 
      break;
      case (gateUp):   
      {
        inputState = gateUp;
      } 
      break;
    }
  }

  dOptoSafe.update();
  int OptoSafe=dOptoSafe.read();
  
  switch (inputState){
    case (gateReady): 
    {
      motorPowerOn = 0; 
      motorDirection = 0;
    } 
    break;
    case (gateDelay):
    {
      motorPowerOn = 0; 
      motorDirection = 0; 
      GateDelayPreviousMillis = CurTime;
    } 
    break;
    case (gateUp):   
    {
      motorPowerOn = 1; 
      motorDirection = 0;
    } 
    break;
    case (gateDown): 
    {
      motorPowerOn = 1; 
      motorDirection = 1;
//      if(OptoSafe==1){
//      motorPowerOn = 1; 
//      motorDirection = 1;}
//      else{
//      motorPowerOn = 0; 
//      motorDirection = 1;}
    } 
    break;
  }
  //motorPowerOn = ! motorPowerOn; motorDirection = ! motorDirection;
  digitalWrite(PinMotorPowerOn, motorPowerOn);//motorPowerOn 
  digitalWrite(PinMotorDirectoin, motorDirection);
  //  Serial.println(inputState); 
  //  delay(500);
}

// Функции класса HCS301 для чтения брелков
void HCS301::print(){
  String btn;

  if (BtnRing == 1) btn += "Ring";
  if (BtnClose == 1) btn += "Close";
  if (BtnOpen == 1) btn += "Open";
  if (BtnNoSound == 1) btn += "NoSound";

  String it2;
  it2 += "Encript ";
  it2 += Encript;
  it2 += " Serial ";
  it2 += SerialNum;
  it2 += " ";
  it2 += btn;
  it2 += " BattaryLow=";
  it2 += BattaryLow;
  it2 += " Rep=";
  it2 += Repeat;

  Serial.println(it2);

}

void HCS_interrupt(){

  if(HCS_Listening == false){
    return;
  }

  uint32_t cur_timestamp = micros();
  uint8_t  cur_status = digitalRead(HCS_RECIEVER_PIN);
  uint32_t pulse_duration = cur_timestamp - HCS_last_change;
  HCS_last_change			= cur_timestamp;

  // ловим преамбулу
  if(HCS_preamble_count < 12){
    if(cur_status == HIGH){
      if( ((pulse_duration > 150) && (pulse_duration < 500)) || HCS_preamble_count == 0){
        // начало импульса преамбулы
        //if(HCS_preamble_count == 0){
        //	HCS_start_preamble = cur_timestamp; // Отметим время старта преамбулы
        //}
      } 
      else {
        // поймали какую то фигню, неправильная пауза между импульсами
        HCS_preamble_count = 0; // сбрасываем счетчик пойманных импульсов преамбулы
        goto exit; 

      }
    } 
    else {
      // конец импульса преамбулы
      if((pulse_duration > 300) && (pulse_duration < 600)){
        // поймали импульс преамбулы
        HCS_preamble_count ++;
        if(HCS_preamble_count == 12){
          // словили преамбулу
          //HCS_Te = (cur_timestamp - HCS_start_preamble) / 23;  // вычисляем длительность базового импульса Te
          //HCS_Te2_3 = HCS_Te * 3 / 2;
          HCS_bit_counter = 0;
          goto exit; 
        }
      } 
      else {
        // поймали какую то фигню
        HCS_preamble_count = 0; // сбрасываем счетчик пойманных импульсов преамбулы
        goto exit; 
      }
    }
  }
  // ловим данные
  if(HCS_preamble_count == 12){
    if(cur_status == HIGH){
      if(((pulse_duration > 250) && (pulse_duration < 900)) || HCS_bit_counter == 0){
        // начало импульса данных
      } 
      else {
        // неправильная пауза между импульсами
        HCS_preamble_count = 0;
        goto exit; 
      }
    } 
    else {
      // конец импульса данных
      if((pulse_duration > 250) && (pulse_duration < 900)){
        HCS_bit_array[65 - HCS_bit_counter] = (pulse_duration > HCS_Te2_3) ? 0 : 1; // импульс больше, чем половина от Те * 3 поймали 0, иначе 1
        HCS_bit_counter++;	
        if(HCS_bit_counter == 66){
          // поймали все биты данных
          HCS_Listening = false;	// отключем прослушку приемника, отправляем пойманные данные на обработку
          HCS_preamble_count = 0; // сбрасываем счетчик пойманных импульсов преамбулы

          hcs301.Repeat = HCS_bit_array[0];
          hcs301.BattaryLow = HCS_bit_array[1];
          hcs301.BtnNoSound = HCS_bit_array[2];
          hcs301.BtnOpen = HCS_bit_array[3];
          hcs301.BtnClose = HCS_bit_array[4];
          hcs301.BtnRing = HCS_bit_array[5];

          hcs301.SerialNum = 0;
          for(int i = 6; i < 34;i++){
            hcs301.SerialNum = (hcs301.SerialNum << 1) + HCS_bit_array[i];
          };

          uint32_t Encript = 0;
          for(int i = 34; i < 66;i++){
            Encript = (Encript << 1) + HCS_bit_array[i];
          };
          hcs301.Encript = Encript;
        }
      } 
      else {
        // поймали хрень какую то, отключаемся
        HCS_preamble_count = 0;
        goto exit; 
      }
    }
  }

exit:;
  //digitalWrite(LED_PIN,cur_status);
}



