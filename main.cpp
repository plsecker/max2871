#include "max32625.h"
#include "mbed.h"
#include <cstdio>
#include <stdio.h>
#include "MAX2871.h"
#include "max2871_registers.h"

int main() {
    
#define D6   P0_6
#define D8   P1_4
#define D9   P1_5

    
    SPI         spi(SPI1_MOSI,SPI1_MISO,SPI1_SCK);           //mosi, miso, sclk (P1.2, P1.1, P1.0)
    BufferedSerial   pc(CONSOLE_TX,CONSOLE_RX,115200);       //tx, rx, baud

    DigitalOut  le(SPI1_SS,1);              //Latch enable pin for MAX2871
    DigitalIn   ld(D6);                     //Lock detect output pin
    DigitalOut  led(LED_BLUE,1);            //blue LED on MAX32600MBED board

    DigitalOut  rfouten(D8,1);              //RF output enable pin
    DigitalOut  ce(D9,1);                   //Chip enable pin 
    
    static double freq_entry;               //variable to store user frequency input
    char buffer[256] = {0};                 //array to hold string input from terminal
    char cbuffer = {0};
    char message[256] = {0};


    double v_tune, temperature, pfd;         //stores TUNE voltage and die temperature of MAX2871
    uint32_t vco;                       //stores active VCO in use
    double freq_rfouta;                 //variable to calculate ouput frequency from register settings

    static int i;

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

        sprintf(message,"\n\rEnter a freq in MHz:");
        pc.write(message,sizeof(message));

        i=0;
        cbuffer = 0;
        while (cbuffer != '\r') {
            while (!pc.readable()) {}
            if (uint32_t num = pc.read(&cbuffer, sizeof(cbuffer))) {
                // Toggle the LED.
                led = !led;

                // Echo the input back to the terminal.
                pc.write(&cbuffer, num);
                buffer[i++] = cbuffer;
            }
        }

        freq_entry = floor(1000*atof(buffer))/1000;         //convert string to a float with 1kHz precision
        if((freq_entry < 23.5) || (freq_entry > 6000.0)) {   //check the entered frequency is in MAX2871 range
            sprintf(message,"\n\rNot a valid frequency entry.");
            pc.write(message,sizeof(message));
        }
        else
        {
            sprintf(message,"\n\rTarget: %.3f MHz\n\r",freq_entry);   //report the frequency derived from user's input
            pc.write(message,sizeof(message));

            max2871.setPFD(50,1);                       //update MAX2871 registers for new frequency

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
        for (i=0;i<sizeof(message);i++) {
            message[i] = 0;
        }
        pc.write(message,sizeof(message));

    }   
}
