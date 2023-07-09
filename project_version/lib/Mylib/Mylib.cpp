#include <Mylib.h>
int value, real_value;

uint8_t ReadHumidity(uint8_t GPIO_NUM_34){
  for(int i=0;i<=9;i++){
   	real_value+=analogRead(GPIO_NUM_34);
  }
  value=real_value/10;
    int percent = map(value,1378,4095,0,100);
	real_value = 0;
	percent = 100 - percent;
	return percent;
}

String FixValToDisplay(uint8_t val){
    if(val<10) return "0"+String(val);
    else return String(val);
}