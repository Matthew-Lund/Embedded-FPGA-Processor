/* Author: Jennifer Stander (Edited by Matthew Lund)
 * Course: ECE3829
 * Project: Lab 4
 * Description: Starting project for Lab 4.
 * Implements two functions
 * 1- reading switches and lighting their corresponding LED
 * 2 - It outputs a middle C tone to the AMP2
 * It also initializes the anode and segment of the 7-seg display
 * for future development
 */


// Header Inclusions
/* xparameters.h set parameters names
 like XPAR_AXI_GPIO_0_DEVICE_ID that are referenced in you code
 each hardware module as a section in this file.
*/
#include "xparameters.h"
/* each hardware module type as a set commands you can use to
 * configure and access it. xgpio.h defines API commands for your gpio modules
 */
#include "xgpio.h"
/* this defines the recommend types like u32 */
#include "xil_types.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "sleep.h"
#include "xtmrctr.h"


void check_switches(u32 *sw_data, u32 *sw_data_old, u32 *sw_changes);
void check_buttons(u32 *btn_data, u32 *btn_data_old, u32 *btn_changes);
void update_LEDs(u32 led_data);
void update_amp2(u32 *amp2_data, u32 target_count, u32 *last_count);
void octave_switch(u32 *sw_data, u32 *octave);
void note_change(u32 *octave, u32 *btn_data, u32 *note);


// Block Design Details
/* Timer device ID
 */
#define TMRCTR_DEVICE_ID XPAR_TMRCTR_0_DEVICE_ID
#define TIMER_COUNTER_0 0


/* LED are assigned to GPIO (CH 1) GPIO_0 Device
 * DIP Switches are assigned to GPIO2 (CH 2) GPIO_0 Device
 */
#define GPIO0_ID XPAR_GPIO_0_DEVICE_ID
#define GPIO0_LED_CH 1
#define GPIO0_SW_CH 2
// 16-bits of LED outputs (not tristated)
#define GPIO0_LED_TRI 0x00000000
#define GPIO0_LED_MASK 0x0000FFFF
// 16-bits SW inputs (tristated)
#define GPIO0_SW_TRI 0x0000FFFF
#define GPIO0_SW_MASK 0x0000FFFF

/*  7-SEG Anodes are assigned to GPIO (CH 1) GPIO_1 Device
 *  7-SEG Cathodes are assined to GPIO (CH 2) GPIO_1 Device
 */
#define GPIO1_ID XPAR_GPIO_1_DEVICE_ID
#define GPIO1_ANODE_CH 1
#define GPIO1_CATHODE_CH 2
//4-bits of anode outputs (not tristated)
#define GPIO1_ANODE_TRI 0x00000000
#define GPIO1_ANODE_MASK 0x0000000F
//8-bits of cathode outputs (not tristated)
#define GPIO1_CATHODE_TRI 0x00000000
#define GPIO1_CATHODE_MASK 0x000000FF

// Push buttons are assigned to GPIO (CH_1) GPIO_2 Device
#define GPIO2_ID XPAR_GPIO_2_DEVICE_ID
#define GPIO2_BTN_CH 1
// 4-bits of push button (not tristated)
#define GPIO2_BTN_TRI 0x00000000
#define GPIO2_BTN_MASK 0x0000000F

// AMP2 pins are assigned to GPIO (CH1 1) GPIO_3 device
#define GPIO3_ID XPAR_GPIO_3_DEVICE_ID
#define GPIO3_AMP2_CH 1
#define GPIO3_AMP2_TRI 0xFFFFFFF4
#define GPIO3_AMP2_MASK 0x00000001

//Note Definitions
#define OFF 0xFFFFFFFF
#define C3 (1.0/(2.0*130.81*10e-9))
#define D3 (1.0/(2.0*146.83*10e-9))
#define E3 (1.0/(2.0*164.81*10e-9))
#define F3 (1.0/(2.0*174.61*10e-9))
#define G3 (1.0/(2.0*196*10e-9))
#define A3 (1.0/(2.0*220*10e-9))
#define B3 (1.0/(2.0*246.94*10e-9))
#define C4 (1.0/(2.0*261.63*10e-9))
#define D4 (1.0/(2.0*293.66*10e-9))
#define E4 (1.0/(2.0*329.63*10e-9))
#define F4 (1.0/(2.0*349.23*10e-9))
#define G4 (1.0/(2.0*392*10e-9))
#define A4 (1.0/(2.0*440*10e-9))
#define B4 (1.0/(2.0*493.88*10e-9))
#define C5 (1.0/(2.0*523.25*10e-9))
#define D5 (1.0/(2.0*587.33*10e-9))


// Timer Device instance
XTmrCtr TimerCounter;

// GPIO Driver Device
XGpio device0;
XGpio device1;
XGpio device2;
XGpio device3;

// IP Tutorial  Main
int main() {
	u32 sw_data = 0;
	u32 sw_data_old = 0;
	u32 btn_data = 0;
	u32 btn_data_old = 0;
	// bit[3] = SHUTDOWN_L and bit[1] = GAIN, bit[0] = Audio Input
	u32 amp2_data = 0x8;
	u32 target_count = 0xffffffff;
	u32 last_count = 0;
	u32 sw_changes = 0;
	u32 btn_changes = 0;	//changes in buttons
	u32 octave = 0;			//octave value
	u32 note = 0;			//note should be played
	u32 note_null = 0;		//just for no sound
	XStatus status;


	//Initialize timer
	status = XTmrCtr_Initialize(&TimerCounter, XPAR_TMRCTR_0_DEVICE_ID);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization Timer failed\n\r");
		return 1;
	}
	//Make sure the timer is working
	status = XTmrCtr_SelfTest(&TimerCounter, TIMER_COUNTER_0);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization Timer failed\n\r");
		return 1;
	}
	//Configure the timer to Autoreload
	XTmrCtr_SetOptions(&TimerCounter, TIMER_COUNTER_0, XTC_AUTO_RELOAD_OPTION);
	//Initialize your timer values
	//Start your timer
	XTmrCtr_Start(&TimerCounter, TIMER_COUNTER_0);



	// Initialize the GPIO devices
	status = XGpio_Initialize(&device0, GPIO0_ID);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization GPIO_0 failed\n\r");
		return 1;
	}
	status = XGpio_Initialize(&device1, GPIO1_ID);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization GPIO_1 failed\n\r");
		return 1;
	}
	status = XGpio_Initialize(&device2, GPIO2_ID);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization GPIO_2 failed\n\r");
		return 1;
	}
	status = XGpio_Initialize(&device3, GPIO3_ID);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization GPIO_3 failed\n\r");
		return 1;
	}

	// Set directions for data ports tristates, '1' for input, '0' for output
	XGpio_SetDataDirection(&device0, GPIO0_LED_CH, GPIO0_LED_TRI);
	XGpio_SetDataDirection(&device0, GPIO0_SW_CH, GPIO0_SW_TRI);
	XGpio_SetDataDirection(&device1, GPIO1_ANODE_CH, GPIO1_ANODE_TRI);
	XGpio_SetDataDirection(&device1, GPIO1_CATHODE_CH, GPIO1_CATHODE_TRI);
	XGpio_SetDataDirection(&device2, GPIO2_BTN_CH, GPIO2_BTN_TRI);
	XGpio_SetDataDirection(&device3, GPIO3_AMP2_CH, GPIO3_AMP2_TRI);

	xil_printf("Demo initialized successfully\n\r");

	XGpio_DiscreteWrite(&device3, GPIO3_AMP2_CH, amp2_data);


	XGpio_DiscreteWrite(&device1, GPIO1_ANODE_CH, 0xE); //Enable only the rightmost seven segment display


	//Song to play on startup: Super Mario Bros. Theme
		//E4 -> E4 -> E4 -> C4 -> E4 -> G4 -> G3
	//Variables for what startup should do
	u32 disp_start[7] = {0x0086, 0x0086, 0x0086, 0x00C6, 0x0086, 0x0090, 0x0090};
	u32 note_startup[7] = {E4, E4, E4, C4, E4, G4, G3};
	//Startup sequence
	for(int i = 0; i < 7; i++){ //Run through each note at constant pace
		u32 start_count = XTmrCtr_GetValue(&TimerCounter, TIMER_COUNTER_0); //Establish reference tick count

		XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, disp_start[i]);
		target_count = note_startup[i]; //Adjust target_count based on current note of note_list

		while(XTmrCtr_GetValue(&TimerCounter, TIMER_COUNTER_0) < start_count + 20000000){ //For .2s play current note
			update_amp2(&amp2_data, target_count, &last_count);
		};

		note_change(0x0000, 0x0000, &note_null);
		target_count = note_null; //Silence current note for staccato

		while(XTmrCtr_GetValue(&TimerCounter, TIMER_COUNTER_0) < start_count + 25000000){ //For .1s play silence before next note
			update_amp2(&amp2_data, target_count, &last_count);
		};
	}

			note_change(0x0000, 0x0000, &note_null);
			target_count = note_null; //Reset current note to none

	// this loop checks for changes in the input switches
	// if they changed it updates the LED outputs to match the switch values.
	// target_count = (period of sound)/(2*10nsec)), 10nsec is the processor clock frequency
	while (1) {
		//Determines switches and buttons
		check_switches(&sw_data, &sw_data_old, &sw_changes);
		check_buttons(&btn_data, &btn_data_old, &btn_changes);
		//Determines note  and octave
		octave_switch(&sw_data, &octave);
		note_change(&octave, &btn_data, &note);
		if (sw_changes || btn_changes) {	//if any change; update note and LEDs
			update_LEDs(sw_data);
			target_count = note;
		}
		update_amp2(&amp2_data, target_count, &last_count);
	}

}

// reads the value of the input switches and outputs if there were changes from last time
void check_switches(u32 *sw_data, u32 *sw_data_old, u32 *sw_changes) {
	*sw_data = XGpio_DiscreteRead(&device0, GPIO0_SW_CH);
	*sw_data &= GPIO0_SW_MASK;
	*sw_changes = 0;
	if (*sw_data != *sw_data_old) {
		// When any bswitch is toggled, the LED values are updated
		//  and report the state over UART.
		*sw_changes = *sw_data ^ *sw_data_old;
		*sw_data_old = *sw_data;
	}
}

// reads the value of the input buttons and outputs if there were changes from last time
void check_buttons(u32 *btn_data, u32 *btn_data_old, u32 *btn_changes) {
	*btn_data = XGpio_DiscreteRead(&device2, GPIO2_BTN_CH);
	*btn_data &= GPIO2_BTN_MASK;
	*btn_changes = 0;
	if (*btn_data != *btn_data_old) {
		// When any button is toggled, the values are updated
		//  and report the state over UART.
		*btn_changes = *btn_data ^ *btn_data_old;
		*btn_data_old = *btn_data;
	}
}

// writes the value of led_data to the LED pins
void update_LEDs(u32 led_data) {
	led_data = (led_data) & GPIO0_LED_MASK;
	XGpio_DiscreteWrite(&device0, GPIO0_LED_CH, led_data);
}

// if the current count is - last_count > target_count toggle the amp2 output
void update_amp2(u32 *amp2_data, u32 target_count, u32 *last_count) {
	u32 current_count = XTmrCtr_GetValue(&TimerCounter, TIMER_COUNTER_0);
	if ((current_count - *last_count) > target_count) {
		// toggling the LSB of amp2 data
		*amp2_data = ((*amp2_data & 0x01) == 0) ? (*amp2_data | 0x1) : (*amp2_data & 0xe);
		XGpio_DiscreteWrite(&device3, GPIO3_AMP2_CH, *amp2_data );
		*last_count = current_count;
	}
}

//Determines Octave Based on Switches
void octave_switch(u32 *sw_data, u32 *octave){
	u32 octave_mask = 0x0003;	//Mask for looking at only SW[1:0]
	u32 octave_switch = *sw_data & octave_mask;
	switch(octave_switch){
		case(0x0000)://No switches = 2'd0
			*octave = 0x0003;//Octave 3
			break;
		case(0x0001)://SW0 = 2'd1
			*octave = 0x0004; //Octave 4
			break;
		case(0x0002)://SW1 = 2'd2
			*octave = 0x0005;	//Octave 5
			break;
		default:	//Nothing should play when both switches are up as that is 2'd3
			*octave = 0x0000;
			break;
	}
}

//Determines Note Based on Buttons after octave determined
void note_change(u32 *octave, u32 *btn_data, u32 *note){
	switch(*octave){
		case(0x0005):	//5th Octave
			switch(*btn_data){
				case(0x0000)://No Buttons
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00FF);
					*note = OFF;
					break;
				case(0x0001):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00C6);
					*note = C5; //C5 - 1100 0110
					break;
				case(0x0002):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00A1);
					*note = D5; //D5 - 1010 0001
					break;
				default:	//No other button combinations should be pressed
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00FF);
					*note = OFF;
					break;
			}
			break;
		case(0x0004):	//4th Octave
			switch(*btn_data){
				case(0x0000)://No Buttons
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00FF);
					*note = OFF;
					break;
				case(0x0001):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00C6);
					*note = C4; //C4 - 1100 0110
					break;
				case(0x0002):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00A1);
					*note = D4; //D4 - 1010 0001
					break;
				case(0x0003):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0086);
					*note = E4; //E4 - 1000 0110
					break;
				case(0x0004):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x008E);
					*note = F4; //F4 - 1000 1110
					break;
				case(0x0005):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0090);
					*note = G4; //G4 - 1001 0000
					break;
				case(0x0006):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0088);
					*note = A4; //A4 - 1000 1000
					break;
				case(0x0007):
					XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0083);
					*note = B4; //B4 - 1000 0011
				}
			break;
		case(0x0003):	//3rd Octave
				switch(*btn_data){
					case(0x0000)://No Buttons
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00FF);
					*note = OFF;
						break;
					case(0x0001):
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00C6);
					*note = C3; //C3 - 1100 0110
						break;
					case(0x0002):
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00A1);
					*note = D3; //D3 - 1010 0001
						break;
					case(0x0003):
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0086);
					*note = E3; //E3 - 1000 0110
						break;
					case(0x0004):
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x008E);
					*note = F3; //F3 - 1000 1110
						break;
					case(0x0005):
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0090);
					*note = G3; //G3 - 1001 0000
						break;
					case(0x0006):
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0088);
					*note = A3; //A3 - 1000 1000
						break;
					case(0x0007):
						XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x0083);
					*note = B3; //B3 - 1000 0011
						break;
				}
			break;
		default:	//Any other button combination
			XGpio_DiscreteWrite(&device1, GPIO1_CATHODE_CH, 0x00FF);
			*note = OFF;
			break;
	}
}
