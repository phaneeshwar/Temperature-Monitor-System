#include "mbed.h"
#include "N5110.h"

N5110 lcd(p7,p8,p9,p10,p11,p13,p21);// Pin selection for LCD
AnalogIn lm35(p15);
DigitalIn button_A(p19); // Initialize pin 19 to button A
DigitalIn button_B(p29); // Initialize pin 29 to button A

float average();
float max_min_value();
static float a2[2]; // Global variable

int main() 
{ 
float Average;  //Initialize variable average
float vol,temp,fahrenheit,kelvin; //Initializing variables
char buffer[14];    // Intializing buffer variable
 
while(1)
{    
     lcd.init(); //
          
     Average = average(); // calling average function
     lcd.refresh();        
     
     while(button_A == 1) // for Maximum,Minimum,Average values
     {   
         max_min_value(); // calling Maximum and Minimum function
         
         sprintf(buffer,"Max =%.2f C",a2[1]); 
         lcd.printString(buffer,0,0); //  // print the output maximum value
        
         sprintf(buffer,"Min =%.2f C",a2[0]);
         lcd.printString(buffer,0,1);// print the output minimum value
         
         sprintf(buffer,"Avg =%.2f C",Average);
         lcd.printString(buffer,0,2);// print the output average value
        
         lcd.refresh();      
         wait(1.0);
         lcd.clear();  
                     
      }           
         
     while (button_B == 1) // Button 2 for plot graph
     {     
         
        float array[84];
        for (int i = 0; i < 84; i++) 
        {
            array[i] = 10*lm35.read() ; // Read 84 values from sensor
        }
        
        
        lcd.refresh();
        
        for (int j = 0; j < 84; j++){  
        lcd.plotArray(array);
        } 
        wait(1.0);
         }
     vol = 3.3f * lm35.read();
     temp = 100.0f*vol;              
     fahrenheit = ((9*temp/5)+ 32); // fahrenheit calculations
     kelvin = fahrenheit + 273;  // kelvin calculations

     
     sprintf(buffer,"T =%.2f C",temp);
     lcd.printString(buffer,0,0); //print the output temp value
     
     sprintf(buffer,"F =%.2f F",fahrenheit);
     lcd.printString(buffer,0,1); //print the output fahrenheit value
     
     sprintf(buffer,"K =%.2f K",kelvin);
     lcd.printString(buffer,0,2); //print the output kelvin value
     
     lcd.printString("But1 Avg,M,m : ",0,3);
    
     lcd.printString("Button2:Graph: ",0,4);  
     
     lcd.refresh();      
     wait(1.0);
     lcd.clear(); 
    
             
     }
 
 }   
  
float average() // function definition to calculate average value of the temperature
{   float a[100];
    float avg = 0, sum = 0;
    for (int q = 0; q<=100; q++)
          { a[q] = lm35.read();
            sum = sum + a[q];
            } 
     avg = sum/100;
     avg = 3.3f * avg;
     avg = 100.0f * avg;
     return avg;
    }
    
float max_min_value() // function definition to calculate max an min value of the temperature
    {
     
     float a1[100];
     
     for (int q = 0; q<=100; q++)
          { a1[q] = lm35.read();
            } 
         float Minimum_value = a1[1];
         float Maximum_value = 0;
     
         for (int k = 1; k<=100; k++)
            {  if (a1[k] > a1[k-1])
              { if(Maximum_value < a1[k])
              {
                 Maximum_value = a1[k];
                 continue;
             }
                  }
            else if (a1[k] ==  a1[k-1])
           { 
               if (a1[k] > Maximum_value)
               { Maximum_value = a1[k];
               }
               else if (a1[k] < Minimum_value)
                  { 
                     Minimum_value = a1[k];
                  }
           }
           else if (a1[k] < Minimum_value)
           {Minimum_value = a1[k];
            }
            else
              continue;
            }
         
         a2[1] = 3.3f * Maximum_value;
         a2[1] = 100.0f* a2[1];
         a2[0] = 3.3f * Minimum_value;
         a2[0] = 100.0f* a2[0];
         return 0;
    }
    


