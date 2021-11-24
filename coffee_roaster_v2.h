#define state_idle 0 //Idle state
#define state_idle_transition 1
#define state_idle_to_debug_transition 2
#define state_roasting 3 //Roast started
#define state_roasting_transition 4
#define state_cooling 5 //Planned cooling behavior
#define state_cooling_transition 6
#define state_preroast 7
#define state_preroast_transition 8
#define state_debug 9
#define state_debug_transition 10

#define LED_BUILTIN_TX 30

int CurrentState = 0;  


//Initialize all the state flags
boolean idle_state_flag = false;
boolean preroast_state_flag = false;
boolean roasting_state_flag = false;
boolean cooling_state_flag = false;
boolean interrupt_trigger_flag = false;
int check_buttons_flag = false;
boolean button_changed_flag = false;
int ADC_sample_flag = 0;
const unsigned long ADC_sample_period = 100;


// button definitions
const byte FIRST_BUTTON   =   0x08;
const byte SECOND_BUTTON  =   0x04;
const byte THIRD_BUTTON   =   0x02;
const byte FOURTH_BUTTON =   0x01;

boolean unit_F = false;
boolean sample_temp1 = true;
String ArtisanCommand;

//Initialize Roast type and default value
char roast[4] = "250";
byte roast_val = 250;
char fan[4] = "  0";
byte fan_val = 0;
byte motor_val = 0;

//Intialize time counters
unsigned long roast_time = 0;
char roast_time_char[6] = "00:00";
unsigned long cool_time = 0;
char cool_time_char[6] = "00:00";

//Initialize Thermal temp in Celsius
float Input1 = 10;
float Input1_sample1 = 10;
float Input1_sample2 = 10;
float Input1_average = 10;
float Input2 = 10;
float Input2_sample1 = 10;
float Input2_sample2 = 10;
float Input2_average = 10;
char Input_buffer [4] = "  0";
