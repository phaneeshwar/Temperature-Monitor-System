#include "mbed.h"
#include "N5110.h"

// overloaded constructor includes power pin - LCD Vcc connected to GPIO pin
// this constructor works fine with LPC1768 - enough current sourced from GPIO
// to power LCD. Doesn't work well with K64F.
N5110::N5110(PinName const pwrPin,
             PinName const scePin,
             PinName const rstPin,
             PinName const dcPin,
             PinName const mosiPin,
             PinName const sclkPin,
             PinName const ledPin)
{
    // set up pins as required
    _spi = new SPI(mosiPin,NC,sclkPin); // create new SPI instance and initialise
    _led = new PwmOut(ledPin);
    _pwr = new DigitalOut(pwrPin);
    _sce = new DigitalOut(scePin);
    _rst = new DigitalOut(rstPin);
    _dc = new DigitalOut(dcPin);
}

// overloaded constructor does not include power pin - LCD Vcc must be tied to +3V3
// Best to use this with K64F as the GPIO hasn't sufficient output current to reliably
// drive the LCD.
N5110::N5110(PinName const scePin,
             PinName const rstPin,
             PinName const dcPin,
             PinName const mosiPin,
             PinName const sclkPin,
             PinName const ledPin)
{
    // set up pins as required
    _spi = new SPI(mosiPin,NC,sclkPin); // create new SPI instance and initialise
    _pwr = NULL;  // pwr not needed so null it to be safe
    _led = new PwmOut(ledPin);
    _sce = new DigitalOut(scePin);
    _rst = new DigitalOut(rstPin);
    _dc = new DigitalOut(dcPin);
}

// initialise function - powers up and sends the initialisation commands
void N5110::init()
{
    turnOn();     // power up
    reset();      // reset LCD - must be done within 100 ms

    initSPI();
    // function set - extended
    sendCommand(0x20 | CMD_FS_ACTIVE_MODE | CMD_FS_HORIZONTAL_MODE | CMD_FS_EXTENDED_MODE);
    // Don't completely understand these parameters - they seem to work as they are
    // Consult the datasheet if you need to change them
    sendCommand(CMD_VOP_7V38);    // operating voltage - these values are from Chris Yan's Library
    sendCommand(CMD_TC_TEMP_2);   // temperature control
    sendCommand(CMD_BI_MUX_48);   // changing this can sometimes improve the contrast on some displays

    // function set - basic
    sendCommand(0x20 | CMD_FS_ACTIVE_MODE | CMD_FS_HORIZONTAL_MODE | CMD_FS_BASIC_MODE);
    normalMode();  // normal video mode by default
    sendCommand(CMD_DC_NORMAL_MODE);  // black on white

    clearRAM();      // RAM is undefined at power-up so clear
    clear();   // clear buffer
    setBrightness(0.5);
}

// sets normal video mode (black on white)
void N5110::normalMode()
{
    sendCommand(CMD_DC_NORMAL_MODE);
}

// sets normal video mode (white on black)
void N5110::inverseMode()
{
    sendCommand(CMD_DC_INVERT_VIDEO);
}

// function to power up the LCD and backlight - only works when using GPIO to power
void N5110::turnOn()
{
    if (_pwr != NULL) {
        _pwr->write(1);  // apply power
    }
}

// function to power down LCD
void N5110::turnOff()
{
    clear(); // clear buffer
    refresh();
    setBrightness(0.0);  // turn backlight off
    clearRAM();   // clear RAM to ensure specified current consumption
    // send command to ensure we are in basic mode
    sendCommand(0x20 | CMD_FS_ACTIVE_MODE | CMD_FS_HORIZONTAL_MODE | CMD_FS_BASIC_MODE);
    // clear the display
    sendCommand(CMD_DC_CLEAR_DISPLAY);
    // enter the extended mode and power down
    sendCommand(0x20 | CMD_FS_POWER_DOWN_MODE | CMD_FS_HORIZONTAL_MODE | CMD_FS_EXTENDED_MODE);
    // small delay and then turn off the power pin
    wait_ms(10);

    // if we are powering the LCD using the GPIO then make it low to turn off
    if (_pwr != NULL) {
        _pwr->write(0);  // turn off power
    }

}

// function to change LED backlight brightness
void N5110::setBrightness(float brightness)
{
    // check whether brightness is within range
    if (brightness < 0.0f)
        brightness = 0.0f;
    if (brightness > 1.0f)
        brightness = 1.0f;
    // set PWM duty cycle
    _led->write(brightness);
}


// pulse the active low reset line
void N5110::reset()
{
    _rst->write(0);  // reset the LCD
    _rst->write(1);
}

// function to initialise SPI peripheral
void N5110::initSPI()
{
    _spi->format(8,1);    // 8 bits, Mode 1 - polarity 0, phase 1 - base value of clock is 0, data captured on falling edge/propagated on rising edge
    _spi->frequency(4000000);  // maximum of screen is 4 MHz
}

// send a command to the display
void N5110::sendCommand(unsigned char command)
{
    _dc->write(0);  // set DC low for command
    _sce->write(0); // set CE low to begin frame
    _spi->write(command);  // send command
    _dc->write(1);  // turn back to data by default
    _sce->write(1); // set CE high to end frame (expected for transmission of single byte)
}

// send data to the display at the current XY address
// dc is set to 1 (i.e. data) after sending a command and so should
// be the default mode.
void N5110::sendData(unsigned char data)
{
    _sce->write(0);   // set CE low to begin frame
    _spi->write(data);
    _sce->write(1);  // set CE high to end frame (expected for transmission of single byte)
}

// this function writes 0 to the 504 bytes to clear the RAM
void N5110::clearRAM()
{
    _sce->write(0);  //set CE low to begin frame
    for(int i = 0; i < WIDTH * HEIGHT; i++) { // 48 x 84 bits = 504 bytes
        _spi->write(0x00);  // send 0's
    }
    _sce->write(1); // set CE high to end frame
}

// function to set the XY address in RAM for subsequenct data write
void N5110::setXYAddress(unsigned int const x,
                         unsigned int const y)
{
    if (x<WIDTH && y<HEIGHT) {  // check within range
        sendCommand(0x80 | x);  // send addresses to display with relevant mask
        sendCommand(0x40 | y);
    }
}

// These functions are used to set, clear and get the value of pixels in the display
// Pixels are addressed in the range of 0 to 47 (y) and 0 to 83 (x).  The refresh()
// function must be called after set and clear in order to update the display
void N5110::setPixel(unsigned int const x,
                     unsigned int const y)
{
    if (x<WIDTH && y<HEIGHT) {  // check within range
        // calculate bank and shift 1 to required position in the data byte
        buffer[x][y/8] |= (1 << y%8);
    }
}

void N5110::clearPixel(unsigned int const x,
                       unsigned int const y)
{
    if (x<WIDTH && y<HEIGHT) {  // check within range
        // calculate bank and shift 1 to required position (using bit clear)
        buffer[x][y/8] &= ~(1 << y%8);
    }
}

int N5110::getPixel(unsigned int const x,
                    unsigned int const y) const
{
    if (x<WIDTH && y<HEIGHT) {  // check within range
        // return relevant bank and mask required bit

        int pixel = (int) buffer[x][y/8] & (1 << y%8);

        if (pixel)
            return 1;
        else
            return 0;
    }

    return 0;

}

// function to refresh the display
void N5110::refresh()
{
    setXYAddress(0,0);  // important to set address back to 0,0 before refreshing display
    // address auto increments after printing string, so buffer[0][0] will not coincide
    // with top-left pixel after priting string

    _sce->write(0);  //set CE low to begin frame

    for(int j = 0; j < BANKS; j++) {  // be careful to use correct order (j,i) for horizontal addressing
        for(int i = 0; i < WIDTH; i++) {
            _spi->write(buffer[i][j]);  // send buffer
        }
    }
    _sce->write(1); // set CE high to end frame

}

// fills the buffer with random bytes.  Can be used to test the display.
// The rand() function isn't seeded so it probably creates the same pattern everytime
void N5110::randomiseBuffer()
{
    int i,j;
    for(j = 0; j < BANKS; j++) {  // be careful to use correct order (j,i) for horizontal addressing
        for(i = 0; i < WIDTH; i++) {
            buffer[i][j] = rand()%256;  // generate random byte
        }
    }

}

// function to print 5x7 font
void N5110::printChar(char const          c,
                      unsigned int const  x,
                      unsigned int const  y)
{
    if (y<BANKS) {  // check if printing in range of y banks

        for (int i = 0; i < 5 ; i++ ) {
            int pixel_x = x+i;
            if (pixel_x > WIDTH-1)  // ensure pixel isn't outside the buffer size (0 - 83)
                break;
            buffer[pixel_x][y] = font5x7[(c - 32)*5 + i];
            // array is offset by 32 relative to ASCII, each character is 5 pixels wide
        }

    }
}

// function to print string at specified position
void N5110::printString(const char         *str,
                        unsigned int const  x,
                        unsigned int const  y)
{
    if (y<BANKS) {  // check if printing in range of y banks

        int n = 0 ; // counter for number of characters in string
        // loop through string and print character
        while(*str) {

            // writes the character bitmap data to the buffer, so that
            // text and pixels can be displayed at the same time
            for (int i = 0; i < 5 ; i++ ) {
                int pixel_x = x+i+n*6;
                if (pixel_x > WIDTH-1) // ensure pixel isn't outside the buffer size (0 - 83)
                    break;
                buffer[pixel_x][y] = font5x7[(*str - 32)*5 + i];
            }
            str++;  // go to next character in string
            n++;    // increment index
        }
    }
}

// function to clear the screen buffer
void N5110::clear()
{
    memset(buffer,0,sizeof(buffer));
}

// function to plot array on display
void N5110::plotArray(float const array[])
{
    for (int i=0; i<WIDTH; i++) {  // loop through array
        // elements are normalised from 0.0 to 1.0, so multiply
        // by 47 to convert to pixel range, and subtract from 47
        // since top-left is 0,0 in the display geometry
        setPixel(i,47 - int(array[i]*47.0f));
    }

}

// function to draw circle
void N5110:: drawCircle(unsigned int const x0,
                        unsigned int const y0,
                        unsigned int const radius,
                        unsigned int const fill)
{
    // from http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
    int x = radius;
    int y = 0;
    int radiusError = 1-x;

    while(x >= y) {

        // if transparent, just draw outline
        if (fill == 0) {
            setPixel( x + x0,  y + y0);
            setPixel(-x + x0,  y + y0);
            setPixel( y + x0,  x + y0);
            setPixel(-y + x0,  x + y0);
            setPixel(-y + x0, -x + y0);
            setPixel( y + x0, -x + y0);
            setPixel( x + x0, -y + y0);
            setPixel(-x + x0, -y + y0);
        } else {  // drawing filled circle, so draw lines between points at same y value

            int type = (fill==1) ? 1:0;  // black or white fill

            drawLine(x+x0,y+y0,-x+x0,y+y0,type);
            drawLine(y+x0,x+y0,-y+x0,x+y0,type);
            drawLine(y+x0,-x+y0,-y+x0,-x+y0,type);
            drawLine(x+x0,-y+y0,-x+x0,-y+y0,type);
        }

        y++;
        if (radiusError<0) {
            radiusError += 2 * y + 1;
        } else {
            x--;
            radiusError += 2 * (y - x) + 1;
        }
    }

}

void N5110::drawLine(unsigned int const x0,
                     unsigned int const y0,
                     unsigned int const x1,
                     unsigned int const y1,
                     unsigned int const type)
{
    int y_range = y1-y0;  // calc range of y and x
    int x_range = x1-x0;
    int start,stop,step;

    // if dotted line, set step to 2, else step is 1
    step = (type==2) ? 2:1;

    // make sure we loop over the largest range to get the most pixels on the display
    // for instance, if drawing a vertical line (x_range = 0), we need to loop down the y pixels
    // or else we'll only end up with 1 pixel in the x column
    if ( abs(x_range) > abs(y_range) ) {

        // ensure we loop from smallest to largest or else for-loop won't run as expected
        start = x1>x0 ? x0:x1;
        stop =  x1>x0 ? x1:x0;

        // loop between x pixels
        for (int x = start; x<= stop ; x+=step) {
            // do linear interpolation
            int y = y0 + (y1-y0)*(x-x0)/(x1-x0);

            if (type == 0)   // if 'white' line, turn off pixel
                clearPixel(x,y);
            else
                setPixel(x,y);  // else if 'black' or 'dotted' turn on pixel
        }
    } else {

        // ensure we loop from smallest to largest or else for-loop won't run as expected
        start = y1>y0 ? y0:y1;
        stop =  y1>y0 ? y1:y0;

        for (int y = start; y<= stop ; y+=step) {
            // do linear interpolation
            int x = x0 + (x1-x0)*(y-y0)/(y1-y0);

            if (type == 0)   // if 'white' line, turn off pixel
                clearPixel(x,y);
            else
                setPixel(x,y);  // else if 'black' or 'dotted' turn on pixel

        }
    }

}

void N5110::drawRect(unsigned int const x0,
                     unsigned int const y0,
                     unsigned int const width,
                     unsigned int const height,
                     unsigned int const fill)
{
    if (fill == 0) { // transparent, just outline
        drawLine(x0,y0,x0+(width-1),y0,1);  // top
        drawLine(x0,y0+(height-1),x0+(width-1),y0+(height-1),1);  // bottom
        drawLine(x0,y0,x0,y0+(height-1),1);  // left
        drawLine(x0+(width-1),y0,x0+(width-1),y0+(height-1),1);  // right
    } else { // filled rectangle
        int type = (fill==1) ? 1:0;  // black or white fill
        for (int y = y0; y<y0+height; y++) {  // loop through rows of rectangle
            drawLine(x0,y,x0+(width-1),y,type);  // draw line across screen
        }
    }
}
