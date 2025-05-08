#define  GSR 34
unsigned int sensorValue;

unsigned char counter;
unsigned long temp[11];
unsigned int  rate[10];
unsigned long sub;
boolean firstBeat = true;
unsigned char countInt=0;
boolean data_effect=true;
boolean Start_send=false;
unsigned int heart_rate;

const int max_heartpluse_duty = 2000;

void arrayInit();
void interrupt();
void Send_value();
void Send_withouthartrate();

void setup(){

  pinMode(2,INPUT);
  Serial.begin(57600);
  Serial.println("Please ready your chest belt.");
  delay(1000);
  Serial.println("Heart rate test begin.");
  attachInterrupt(0, interrupt, RISING);        //设定上升沿中断D2中断0
}

void loop(){

  if(Start_send==true){
    Send_value();
  }
  else {
    Send_withouthartrate();
  }
}

void Send_value(){
  cli();  
  sensorValue=analogRead(GSR);
  Serial.print("G:");
  Serial.print(sensorValue);
  Serial.print(" B:");
  Serial.print(heart_rate);
  Serial.println("");
  sei();  
  delay(250);
}

void Send_withouthartrate(){
  cli();  
  sensorValue=analogRead(GSR);
  Serial.print("G:");
  Serial.print(sensorValue);
  Serial.print(" B:");
  Serial.print(0);
  Serial.println("");
  sei();  
  delay(250);
}


void interrupt(){
  
  if(countInt<3){                   //必须要进入3次中断后才开始计算心率
    countInt++;
    if(countInt==3){
      arrayInit();              
      Start_send=false;           
    }
  }
  else{                      
    Start_send=true;
    temp[counter]=millis();      
    switch(counter){
      case 0:
        sub=temp[counter]-temp[10];
        break;
      default:
        sub=temp[counter]-temp[counter-1];
        break;
    }       
   if(sub>max_heartpluse_duty) { 
      data_effect=0;
    }
    
   if(firstBeat){
      firstBeat = false;          
      for(int i=0; i<=9; i++){            
        rate[i] = sub;                      
      } 
   } 
      
    word runningTotal = 0;
    for(int i=0; i<=8; i++){   
      rate[i] = rate[i+1];            
      runningTotal += rate[i];           
    }
  
    rate[9] = sub;                         
    runningTotal += rate[9];               
    runningTotal /= 10;                     
    heart_rate = 60000/runningTotal;        
              
    if (counter==10&&data_effect){
      counter=0;
    }
    else if(counter!=10&&data_effect)
      counter++;
    else {
      counter=0;
      data_effect=1;
      Start_send=false;
      firstBeat=true;
      countInt=0;
      Serial.println("Heart rate measure error,test will restart!" );
      arrayInit(); 
      heart_rate=0;
    }
  }  
}

void arrayInit(){
  for(unsigned char i=0;i < 10;i ++){
    temp[i]=0;
    rate[i]=0;
  }
  temp[10]=millis();
}
