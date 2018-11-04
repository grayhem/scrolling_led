// in which we use an arduino mega, which has another block of i/o pins that aren't hooked up to the
// serial. so we can communicate over native serial while driving the sign.

// adapted from https://github.com/blkhwks19/LED-Marquee/blob/master/Phase1_Uno.ino
// thanks guys


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////IMPLEMENTATION SPECIFIC DETAILS/////////////////////////////////
////////////////////////////////////VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV/////////////////////////////////
// Used to receive data on a virtual RX pin instead of the usual pin 0

// The length of the buffer used to read a new string from the serial port
#define STRINGBUFFER_LEN 600

// Leave room for a safety null terminator at the end.
char str[STRINGBUFFER_LEN+1];

// 19200 appears to be stable. 28800 is a little faster and ~usually~ seems to work.
const int BAUD_RATE = 28800;

// Change this to be at least as long as your pixel string (too long will work fine, just be a
// little slower)
// Number of pixels in the string. I use 3m of 60/m.
#define PIXELS 180
#define CHARACTERS 15

// Bit pattern to write to port to turn on all pins connected to LED strips.
// If you do not want to use all 8 pins, you can mask off the ones you don't want.
// Note that these will still get 0 written to them when we send pixels.
// TODO: If we have time, we could even add a variable that will and/or into the bits before writing
// to the port to support any combination of bits/values
static const uint8_t onBits=0b11111111;



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////DO NOT TOUCH THIS PART//////////////////////////////////////////
////////////////////////////////////VVVVVVVVVVVVVVVVVVVVVV//////////////////////////////////////////

// These values depend on which pins your 8 strings are connected to and what board you are using
// More info on how to find these at http://www.arduino.cc/en/Reference/PortManipulation

// PORTD controls Digital Pins 0-7 on the Uno

// You'll need to look up the port/bit combination for other boards.

// Note that you could also include the DigitalWriteFast header file to not need to to this lookup.

#define PIXEL_PORT  PORTA  // Port of the pin the pixels are connected to
#define PIXEL_DDR   DDRA   // Port of the pin the pixels are connected to



// These are the empirical timing constraints taken mostly from measuring the output from the
// Adafruit library strandtest program.

// Note that some of these defined values are for reference only - the actual timing is determined
// by the hard code.

#define T1H  814    // Width of a 1 bit in ns - 13 cycles
#define T1L  438    // Width of a 1 bit in ns -  7 cycles

#define T0H  312    // Width of a 0 bit in ns -  5 cycles
#define T0L  936    // Width of a 0 bit in ns - 15 cycles

// Phase #1 - Always 1  - 5 cycles
// Phase #2 - Data part - 8 cycles
// Phase #3 - Always 0  - 7 cycles

#define RES 50000   // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays

// Note that this has to be SIGNED since we want to be able to check for negative values of
// derivatives
#define NS_PER_SEC (1000000000L)

#define CYCLES_PER_SEC (F_CPU)

#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )

#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )


// Sends a full 8 bits down all the pins, represening a single color of 1 pixel
// We walk though the 8 bits in colorbyte one at a time. If the bit is 1 then we send the 8 bits of
// row out. Otherwise we send 0.
// We send onBits at the first phase of the signal generation. We could just send 0xff, but that
// might enable pull-ups on pins that we are not using.

// Unforntunately we have to drop to ASM for this so we can interleave the computations during the
// delays, otherwise things get too slow.

// OnBits is the mask of which bits are connected to strips. We pass it on so that we
// do not turn on unused pins becuase this would enable the pullup. Also, hopefully passing this
// will cause the compiler to allocate a Register for it and avoid a reload every pass.

static inline void sendBitx8(const uint8_t row, const uint8_t colorbyte, const uint8_t onBits) {

    asm volatile (


      "L_%=: \n\r"

      "out %[port], %[onBits] \n\t"                 // (1 cycles) - send either T0H or the first part of T1H. Onbits is a mask of which bits have strings attached.

      // Next determine if we are going to be sending 1s or 0s based on the current bit in the color....

      "mov r0, %[bitwalker] \n\t"                   // (1 cycles)
      "and r0, %[colorbyte] \n\t"                   // (1 cycles)  - is the current bit in the color byte set?
      "breq OFF_%= \n\t"                            // (1 cycles) - bit in color is 0, then send full zero row (takes 2 cycles if branch taken, count the extra 1 on the target line)

      // If we get here, then we want to send a 1 for every row that has an ON dot...
      "nop \n\t  "                                  // (1 cycles)
      "out %[port], %[row]   \n\t"                  // (1 cycles) - set the output bits to [row] This is phase for T0H-T1H.
                                                    // ==========
                                                    // (5 cycles) - T0H (Phase #1)


      "nop \n\t nop \n\t "                          // (2 cycles)
      "nop \n\t nop \n\t "                          // (2 cycles)
      "nop \n\t nop \n\t "                          // (2 cycles)
      "nop \n\t "                                   // (1 cycles)

      "out %[port], __zero_reg__ \n\t"              // (1 cycles) - set the output bits to 0x00 based on the bit in colorbyte. This is phase for T0H-T1H
                                                    // ==========
                                                    // (8 cycles) - Phase #2

      "ror %[bitwalker] \n\t"                      // (1 cycles) - get ready for next pass. On last pass, the bit will end up in C flag

      "brcs DONE_%= \n\t"                          // (1 cycles) Exit if carry bit is set as a result of us walking all 8 bits. We assume that the process around us will tak long enough to cover the phase 3 delay

      "nop \n\t \n\t "                             // (1 cycles) - When added to the 5 cycles in S:, we gte the 7 cycles of T1L

      "jmp L_%= \n\t"                              // (3 cycles)
                                                   // (1 cycles) - The OUT on the next pass of the loop
                                                   // ==========
                                                   // (7 cycles) - T1L


      "OFF_%=: \n\r"                                // (1 cycles)    Note that we land here becuase of breq, which takes takes 2 cycles

      "out %[port], __zero_reg__ \n\t"              // (1 cycles) - set the output bits to 0x00 based on the bit in colorbyte. This is phase for T0H-T1H
                                                    // ==========
                                                    // (5 cycles) - T0H

      "ror %[bitwalker] \n\t"                      // (1 cycles) - get ready for next pass. On last pass, the bit will end up in C flag

      "brcs DONE_%= \n\t"                          // (1 cycles) Exit if carry bit is set as a result of us walking all 8 bits. We assume that the process around us will tak long enough to cover the phase 3 delay

      "nop \n\t nop \n\t "                          // (2 cycles)
      "nop \n\t nop \n\t "                          // (2 cycles)
      "nop \n\t nop \n\t "                          // (2 cycles)
      "nop \n\t nop \n\t "                          // (2 cycles)
      "nop \n\t "                                   // (1 cycles)

      "jmp L_%= \n\t"                               // (3 cycles)
                                                    // (1 cycles) - The OUT on the next pass of the loop
                                                    // ==========
                                                    //(15 cycles) - T0L


      "DONE_%=: \n\t"

      // Don't need an explicit delay here since the overhead that follows will always be long
      // enough

      ::
      [port]    "I" (_SFR_IO_ADDR(PIXEL_PORT)),
      [row]   "d" (row),
      [onBits]   "d" (onBits),
      [colorbyte]   "d" (colorbyte ),     // Phase 2 of the signal where the actual data bits show up.
      [bitwalker] "r" (0x80)              // Alocate a register to hold a bit that we will walk down though the color byte

    );

    // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the reset
    // timeout (which is A Long Time)

}


// Just wait long enough without sending any bits to cause the pixels to latch and display the last
// sent frame.
// We round up since the delay must be _at_least_ this long (too short might not work, too long not
// a problem)
void show() {
  delayMicroseconds( (RES / 1000UL) + 1);
}


// Send 3 bytes of color data (R,G,B) for a signle pixel down all the connected strings at the same
// time.
// A 1 bit in "row" means send the color, and a 0 bit means send black.
static inline void sendRowRGB( uint8_t row ,  uint8_t r,  uint8_t g,  uint8_t b ) {
  sendBitx8( row , g , onBits);    // WS2812 takes colors in GRB order
  sendBitx8( row , r , onBits);    // WS2812 takes colors in GRB order
  sendBitx8( row , b , onBits);    // WS2812 takes colors in GRB order
}

// Turn off all pixels
static inline void clear() {
  cli();
  for( unsigned int i=0; i< PIXELS; i++ ) {
    sendRowRGB( 0 , 0 , 0 , 0 );
  }
  sei();
  show();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////YOU CAN MODIFY THE FONT BUT BE CAREFUL///////////////////////////////////
///////////////////////////VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV///////////////////////////////////


// This nice 5x7 font from here...
// http://sunge.awardspace.com/glcd-sd/node4.html

// Font details:
// 1) Each char is fixed 5x7 pixels.
// 2) Each byte is one column.
// 3) Columns are left to right order, leftmost byte is leftmost column of pixels.
// 4) Each column is 8 bits high.
// 5) Bit #7 is top line of char, Bit #1 is bottom.
// 6) Bit #0 is always 0, becuase this pin is used as serial input and setting to 1 would enable the pull-up.
// note actually that we're not using hardware serial at all so it's safe to turn on all bits above
// and use pin 0 to drive a strip.

// defines ascii characters 0x20-0x7F (32-127)
// PROGMEM after variable name as per https://www.arduino.cc/en/Reference/PROGMEM

#define FONT_WIDTH 5
#define INTERCHAR_SPACE 1
#define ASCII_OFFSET 0x20    // ASSCI code of 1st char in font array

const uint8_t Font5x7[] PROGMEM = {
0x00,0x00,0x00,0x00,0x00,//
0x00,0x00,0xfa,0x00,0x00,// !
0x00,0xe0,0x00,0xe0,0x00,// "
0x28,0xfe,0x28,0xfe,0x28,// #
0x24,0x54,0xfe,0x54,0x48,// $
0xc4,0xc8,0x10,0x26,0x46,// %
0x6c,0x92,0xaa,0x44,0x0a,// &
0x00,0xa0,0xc0,0x00,0x00,// '
0x00,0x38,0x44,0x82,0x00,// (
0x00,0x82,0x44,0x38,0x00,// )
0x10,0x54,0x38,0x54,0x10,// *
0x10,0x10,0x7c,0x10,0x10,// +
0x00,0x0a,0x0c,0x00,0x00,// ,
0x10,0x10,0x10,0x10,0x10,// -
0x00,0x06,0x06,0x00,0x00,// .
0x04,0x08,0x10,0x20,0x40,// /
0x7c,0x8a,0x92,0xa2,0x7c,// 0
0x00,0x42,0xfe,0x02,0x00,// 1
0x42,0x86,0x8a,0x92,0x62,// 2
0x84,0x82,0xa2,0xd2,0x8c,// 3
0x18,0x28,0x48,0xfe,0x08,// 4
0xe4,0xa2,0xa2,0xa2,0x9c,// 5
0x3c,0x52,0x92,0x92,0x0c,// 6
0x80,0x8e,0x90,0xa0,0xc0,// 7
0x6c,0x92,0x92,0x92,0x6c,// 8
0x60,0x92,0x92,0x94,0x78,// 9
0x00,0x6c,0x6c,0x00,0x00,// :
0x00,0x6a,0x6c,0x00,0x00,// ;
0x00,0x10,0x28,0x44,0x82,// <
0x28,0x28,0x28,0x28,0x28,// =
0x82,0x44,0x28,0x10,0x00,// >
0x40,0x80,0x8a,0x90,0x60,// ?
0x4c,0x92,0x9e,0x82,0x7c,// @
0x7e,0x88,0x88,0x88,0x7e,// A
0xfe,0x92,0x92,0x92,0x6c,// B
0x7c,0x82,0x82,0x82,0x44,// C
0xfe,0x82,0x82,0x44,0x38,// D
0xfe,0x92,0x92,0x92,0x82,// E
0xfe,0x90,0x90,0x80,0x80,// F
0x7c,0x82,0x82,0x8a,0x4c,// G
0xfe,0x10,0x10,0x10,0xfe,// H
0x00,0x82,0xfe,0x82,0x00,// I
0x04,0x02,0x82,0xfc,0x80,// J
0xfe,0x10,0x28,0x44,0x82,// K
0xfe,0x02,0x02,0x02,0x02,// L
0xfe,0x40,0x20,0x40,0xfe,// M
0xfe,0x20,0x10,0x08,0xfe,// N
0x7c,0x82,0x82,0x82,0x7c,// O
0xfe,0x90,0x90,0x90,0x60,// P
0x7c,0x82,0x8a,0x84,0x7a,// Q
0xfe,0x90,0x98,0x94,0x62,// R
0x62,0x92,0x92,0x92,0x8c,// S
0x80,0x80,0xfe,0x80,0x80,// T
0xfc,0x02,0x02,0x02,0xfc,// U
0xf8,0x04,0x02,0x04,0xf8,// V
0xfe,0x04,0x18,0x04,0xfe,// W
0xc6,0x28,0x10,0x28,0xc6,// X
0xc0,0x20,0x1e,0x20,0xc0,// Y
0x86,0x8a,0x92,0xa2,0xc2,// Z
0x00,0x00,0xfe,0x82,0x82,// [
0x40,0x20,0x10,0x08,0x04,// (backslash)
0x82,0x82,0xfe,0x00,0x00,// ]
0x20,0x40,0x80,0x40,0x20,// ^
0x02,0x02,0x02,0x02,0x02,// _
0x00,0x80,0x40,0x20,0x00,// `
0x04,0x2a,0x2a,0x2a,0x1e,// a
0xfe,0x12,0x22,0x22,0x1c,// b
0x1c,0x22,0x22,0x22,0x04,// c
0x1c,0x22,0x22,0x12,0xfe,// d
0x1c,0x2a,0x2a,0x2a,0x18,// e
0x10,0x7e,0x90,0x80,0x40,// f
0x10,0x28,0x2a,0x2a,0x3c,// g
0xfe,0x10,0x20,0x20,0x1e,// h
0x00,0x22,0xbe,0x02,0x00,// i
0x04,0x02,0x22,0xbc,0x00,// j
0x00,0xfe,0x08,0x14,0x22,// k
0x00,0x82,0xfe,0x02,0x00,// l
0x3e,0x20,0x18,0x20,0x1e,// m
0x3e,0x10,0x20,0x20,0x1e,// n
0x1c,0x22,0x22,0x22,0x1c,// o
0x3e,0x28,0x28,0x28,0x10,// p
0x10,0x28,0x28,0x18,0x3e,// q
0x3e,0x10,0x20,0x20,0x10,// r
0x12,0x2a,0x2a,0x2a,0x04,// s
0x20,0xfc,0x22,0x02,0x04,// t
0x3c,0x02,0x02,0x04,0x3e,// u
0x38,0x04,0x02,0x04,0x38,// v
0x3c,0x02,0x0c,0x02,0x3c,// w
0x22,0x14,0x08,0x14,0x22,// x
0x30,0x0a,0x0a,0x0a,0x3c,// y
0x22,0x26,0x2a,0x32,0x22,// z
0x00,0x10,0x6c,0x82,0x00,// {
0x00,0x00,0xfe,0x00,0x00,// |
0x00,0x82,0x6c,0x10,0x00,// }
0x10,0x10,0x54,0x38,0x10,// ~
0x10,0x38,0x54,0x10,0x10,// 
};


////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////THIS IS THE INTERESTING PART (HOW TO DISPLAY)////////////////////////////
///////////////////////////VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV////////////////////////////

// Send the pixels to form the specified char, not including interchar space
// skip is the number of pixels to skip at the begining to enable sub-char smooth scrolling

// TODO: Subtract the offset from the char before starting the send sequence to save time if
// necessary
// TODO: Also could pad the begining of the font table to aovid the offset subtraction at the cost
// of 20*8 bytes of progmem
// TODO: Could pad all chars out to 8 bytes wide to turn the the multiply by FONT_WIDTH into a shift

static inline void sendChar(uint8_t c, uint8_t skip, uint8_t r, uint8_t g, uint8_t b) {

  const uint8_t *charbase = Font5x7 + (( c -' ')* FONT_WIDTH) ;

  uint8_t col=FONT_WIDTH;

  while (skip--) {
      charbase++;
      col--;
  }

  while (col--) {
      sendRowRGB( pgm_read_byte_near( charbase), r, g, b);
      sendRowRGB( pgm_read_byte_near( charbase++), r, g, b);
  }

  // TODO: FLexible interchar spacing

  sendRowRGB( 0, r, g, b);    // Interchar space
  sendRowRGB( 0, r, g, b);    // Interchar space

}


// Show the passed string. The last letter of the string will be in the rightmost pixels of the
// display.
// Skip is how many cols of the 1st char to skip for smooth scrolling

static inline void sendString(
  const char *s,
  uint8_t skip,
  const uint8_t r,
  const uint8_t g,
  const uint8_t b) {

  unsigned int l=PIXELS/((FONT_WIDTH+INTERCHAR_SPACE)*2);

  //First char is special case becuase it can be stepped for smooth scrolling
  sendChar( *s , skip ,  r , g , b );

  while ( *(++s) && l--) {

    sendChar( *s , 0,  r , g , b );

  }
}




// Set up the specified pins as digital out
void ledsetup() {
  PIXEL_DDR |= onBits;   // Set all used pins to output
  Serial.begin(BAUD_RATE);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
}



void setup() {
  ledsetup();
}




void scroll(int len) {

  const char *m = str;


  int right_here = len-CHARACTERS;
  // Text foreground color cycle effect
  //uint8_t sector = 0;
  //uint8_t step = 0;


  while (*m and (right_here > 0)) {
//  while (*m) {
    //step++;

    // step though each column of the 1st char for smooth scrolling
    for( uint8_t step=0; step<FONT_WIDTH+INTERCHAR_SPACE ; step++ ) {


      cli();

      sendString( m , step , 0x00, 0x00, 0x40 );

      sei();

      delay(60); // speed. lower = faster

      PORTE|=0x01;
      delay(1);
      PORTE&=~0x01;

      }
    m++;
    right_here--;
  }
}

void flash() {
  const char *m = str;
  uint8_t step = 0;
  cli();
  sendString( m , step , 0x00, 0x00, 0x40 );
  sei();
  PORTE|=0x01;
  delay(1);
  PORTE&=~0x01;

}


// Notifies the host computer that we're ready to retrieve custom message data
int getCustomData() {
  // send special symbol so host knows to respond with custom message data
  Serial.print("~");
  // loop until its available
  while (!Serial.available()){
    delay(500);
  }

  // do i need to put the next two lines inside a while (Serial.available()){...} loop?
  // no

  int len = Serial.readBytesUntil('\r', str, STRINGBUFFER_LEN);
  // Null terminate
  str[len] = 0x00;
  return len;
}


void loop() {


  int len = getCustomData();
  // flash();
  scroll(len);


  // TODO: Actually sample the state of the pullup on unused pins and OR it into the mask so we maintain the state.
  // Must do AFTER the cli().
  // TODO: Add offBits also to maintain the pullup state of unused pins.

  return;
}

