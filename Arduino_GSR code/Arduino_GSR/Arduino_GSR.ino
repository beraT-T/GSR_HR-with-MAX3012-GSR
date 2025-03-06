/* 
GSR connection pins to Arduino microcontroller

Arduino           GSR

GND               GND
5V                VCC
A2                SIG
*/
#define  GSR A2
unsigned int sensorValue;

unsigned char counter;
unsigned long temp[11];
unsigned int  rate[10];
unsigned long sub;
boolean firstBeat = true;
unsigned char countInt=0;
boolean data_effect=true;
boolean Start_send=false;
unsigned int heart_rate;//the measurement result of heart rate

const int max_heartpluse_duty = 2000;//检测间隔最高为2S

void arrayInit();
void interrupt();
void Send_value();

void setup(){
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

/*Function: Interrupt service routine.Get the sigal from the external interrupt*/
void interrupt(){
  if(countInt<3){                   //必须要进入3次中断后才开始计算心率
    countInt++;
    if(countInt==3){
      arrayInit();                  //清空数组并记录运行时间
      Start_send=false;             //不发送数据
    }
  }
  else{                             //心率计算
    Start_send=true;
    temp[counter]=millis();         //记录当前时间
    switch(counter){
      case 0:
        sub=temp[counter]-temp[10];//记录心率间隔
        break;
      default:
        sub=temp[counter]-temp[counter-1];//记录心率间隔
        break;
    }       
   if(sub>max_heartpluse_duty) {          //心率间隔超过了2s
      data_effect=0;
    }
    
   if(firstBeat){                        // if this is the second beat, if secondBeat == TRUE
      firstBeat = false;                  // clear secondBeat flag
      for(int i=0; i<=9; i++){             // seed the running total to get a realisitic BPM at startup
        rate[i] = sub;                      
      } 
   } 
      
    word runningTotal = 0;                  // clear the runningTotal variable    
    for(int i=0; i<=8; i++){                // shift data in the rate array
      rate[i] = rate[i+1];                  // and drop the oldest IBI value 
      runningTotal += rate[i];              // add up the 9 oldest IBI values
    }
  
    rate[9] = sub;                          // add the latest IBI to the rate array
    runningTotal += rate[9];                // add the latest IBI to runningTotal
    runningTotal /= 10;                     // average the last 20 IBI values 
    heart_rate = 60000/runningTotal;               // how many beats can fit into a minute? that's BPM!  
//    Serial.print("rate:");
//    Serial.println(heart_rate);                
    if (counter==10&&data_effect){
      counter=0;
    }
    else if(counter!=10&&data_effect)
      counter++;
    else {
      counter=0;
      heart_rate=0;
      data_effect=1;
      Start_send=false;
      firstBeat=true;
      countInt=0;
      Serial.println("Heart rate measure error,test will restart!" );
      arrayInit();
    }
  }  
}
/*Function: Initialization for the array(temp)*/
void arrayInit(){
  for(unsigned char i=0;i < 10;i ++){
    temp[i]=0;
    rate[i]=0;
  }
  temp[10]=millis();
}
