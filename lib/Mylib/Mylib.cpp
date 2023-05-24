#include <Mylib.h>

uint8_t ReadHumidity(uint8_t pin){
    if(analogRead(pin)<1800)
    
        return 100;

    return map(analogRead(pin),1800,4095,100,0);
}

String FixValToDisplay(uint8_t val){
    if(val<10) return "0"+String(val);
    else return String(val);
}