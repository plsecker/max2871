#include "max32625.h"
#include "mbed.h"
#include <cstdio>
#include "MAX2871.h"
#include "max2871_registers.h"
#include <ratio>
#include <stdio.h>

   
#define D6   P0_6
#define D8   P1_4
#define D9   P1_5

#define ADIV    4


char message[256] = {0};


long long gcd(long long a, long long b)
{
    if (a == 0)
        return b;
    else if (b == 0)
        return a;
    if (a < b)
        return gcd(a, b % a);
    else
        return gcd(b, a % b);
}


struct Numdennom {
        int f, m;
    };

typedef struct Numdennom synthratio;


//Function to convert decimal to fraction
synthratio decimalToFraction(double number)
{
    // Fetch integral value of the decimal
    double intVal = floor(number);
    synthratio  s;
 
    // Fetch fractional part of the decimal
    double fVal = number - intVal;
 
    // Consider precision value to
    // convert fractional part to
    // integral equivalent
    const long pVal = 1000000000;
 
    // Calculate GCD of integral
    // equivalent of fractional
    // part and precision value
    long long gcdVal
        = gcd(round(fVal * pVal), pVal);
 
    // Calculate num and deno
    long long num
        = round(fVal * pVal) / gcdVal;
    long long deno = pVal / gcdVal;
 
    // Print the fraction
    s.f = num;
    s.m = deno;

    return(s);
}
void cget_message(BufferedSerial *pc, DigitalOut  led, char *buffer) {
        char cbuffer = {0};
        int i = 0;

        while (cbuffer != '\r') {
            while (!pc->readable()) {}
            if (uint32_t num = pc->read(&cbuffer, sizeof(cbuffer))) {
                // Toggle the LED.
                led = !led;

                // Echo the input back to the terminal.
                if (cbuffer != '\b') {
                    pc->write(&cbuffer, num);
                    buffer[i++] = cbuffer;
                }
            }
        }
}

void clear_message(BufferedSerial *pc ) {
        for (int i=0;i<sizeof(message);i++) {
            message[i] = 0;
        }
        pc->write(message,sizeof(message));
}

int main() {

    SPI         spi(SPI1_MOSI,SPI1_MISO,SPI1_SCK);           //mosi, miso, sclk (P1.2, P1.1, P1.0)
    BufferedSerial   pc(CONSOLE_TX,CONSOLE_RX,115200);       //tx, rx, baud

    DigitalOut  le(SPI1_SS,1);              //Latch enable pin for MAX2871
    DigitalIn   ld(D6);                     //Lock detect output pin
    DigitalOut  led(LED_BLUE,1);            //blue LED on MAX32600MBED board

    DigitalOut  rfouten(D8,1);              //RF output enable pin
    DigitalOut  ce(D9,1);                   //Chip enable pin 
    
    static double freq_entry;               //variable to store user frequency input
    char buffer[256] = {0};                 //array to hold string input from terminal


    double v_tune, temperature;         //stores TUNE voltage and die temperature of MAX2871
    uint32_t vco, ndiv;                       //stores active VCO in use
    double freq_rfouta, pfd = 50.0;     //variable to calculate ouput frequency from register settings
    double  fvco_fpfd, fract, fout_synth;

    int i;
   

    spi.format(8,0);                    //CPOL = CPHA = 0, 8 bits per frame
    spi.frequency(1000000);             //1 MHz SPI clock

    MAX2871 max2871(spi,SPI1_SS);           //create object of class MAX2871, assign latch enable pin

    MAX2871_registers max2871_regs;

    max2871.powerOn(true);              //set all hardware enable pins and deassert software shutdown bits
    
    max2871.setPFD(40.0,1);             //inputs are reference frequency and R divider to set phase/frequency detector comparison frequency
    
    max2871_regs.max2871Set_INT(0);     //set individual bits




    //The routine in the while(1) loop will ask the user to input a desired
    //output frequency, check that it is in range, calculate the corresponding
    //register settings, update the MAX2871 registers, and then independently
    //use the programmed values to re-calculate the output frequency chosen
    while(1){

        sprintf(message,"\n\rEnter a pfd freq in MHz: (%2.3f, R=1)",pfd);
        pc.write(message,sizeof(message));
        clear_message(&pc);

        cget_message(&pc, led, buffer);

        pfd = floor(1000*atof(buffer))/1000; 

        sprintf(message,"\n\rEnter a freq in MHz:");
        pc.write(message,sizeof(message));
        clear_message(&pc);

        cget_message(&pc, led, buffer);

        freq_entry = floor(1000*atof(buffer))/1000;         //convert string to a float with 1kHz precision
        if((freq_entry < 23.5) || (freq_entry > 6000.0)) {   //check the entered frequency is in MAX2871 range
            sprintf(message,"\n\rNot a valid frequency entry.");
            pc.write(message,sizeof(message));
            clear_message(&pc);

        }
        else
        {
            fvco_fpfd = ADIV*freq_entry/pfd;
            ndiv = floor(fvco_fpfd);
            fract = fvco_fpfd-ndiv;
            sprintf(message,"\n\rfvco_fpfd: %f, ndiv: %d",fvco_fpfd, ndiv);   //report the frequency derived from user's input
            pc.write(message,sizeof(message));
            clear_message(&pc);

            synthratio ratio;
            ratio = decimalToFraction(fract);
            fout_synth = (ndiv+(float)ratio.f/ratio.m)*pfd/ADIV;

            sprintf(message,"\n\rfract: %f, f: %d  m: %d",fract, ratio.f,ratio.m);   //report the frequency derived from user's input
            pc.write(message,sizeof(message));
            clear_message(&pc);


            sprintf(message,"\n\rTarget: %.3f MHz\n\r",freq_entry);   //report the frequency derived from user's input
            pc.write(message,sizeof(message));

            sprintf(message,"\n\rRatio Target: %.3f MHz\n\r",fout_synth);   //report the frequency derived from user's input
            pc.write(message,sizeof(message));


            max2871.setPFD(pfd,1);                       //update MAX2871 registers for new frequency

            max2871.setRFOUTA(freq_entry);                  //update MAX2871 registers for new frequency

            while(!ld)                                      //blink an LED while waiting for MAX2871 lock detect signal to assert
            {
                led = !led;
                wait_us(30000);
            }
            led = 1;
        
            vco = max2871.readVCO();                        //read the active VCO from MAX2871
            v_tune = max2871.readADC();                     //read the digitized TUNE voltage
            pfd = max2871.getPFD();                         //get pfd
            freq_rfouta = max2871.getRFOUTA();              //calculate the output frequency of channel A
            temperature = max2871.readTEMP();               //read die temperature from MAX2871

 
            //print the achieved output frequency and MAX2871 diagnostics
            sprintf(message,"\n\rActual: %.3f MHz (Pfd: %.3f MHz)\n\r",freq_rfouta, pfd);
            pc.write(message,sizeof(message));
            sprintf(message,"\n\rVTUNE: %.3f V, VCO: %d, TEMP: %f\n\r",v_tune,vco,temperature);
            pc.write(message,sizeof(message));
        }
        clear_message(&pc);
    }   
}
