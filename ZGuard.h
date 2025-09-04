#ifndef ZGUARD_H
#define ZGUARD_H
#include<Arduino.h>
#include "Vector.h"
class ZGuard{
private:
bool Theft=false,UnderAttack=false,First_Weight_Added=false;
Vector<int> user_logs;
int Initial_Weight,cs;
public:
void reset(){
  Initial_Weight=0;
  First_Weight_Added=false;
  user_logs.clear();
}
void addDerivate(int dw){
  if(dw==1&&dw<0)user_logs.push_back(dw);
}
int checkSum(int weight){
if(!First_Weight_Added){
Initial_Weight=weight;
First_Weight_Added=true;
}

int sum=0;
for(int i;i<user_logs.size();i++){
sum=sum+user_logs[i];
}
cs=weight-(sum+Initial_Weight);
return cs;
}
bool isSafe(){
  if(cs==0)return true;
  else return false;
}
void setInitialWeight(int w){
  Initial_Weight=w;
}
};



#endif