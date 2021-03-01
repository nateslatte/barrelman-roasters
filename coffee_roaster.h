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
boolean debug_state_flag = false;
boolean refresh_display_flag = false;
int check_buttons_flag = false;
int check_buttons_100ms_flag = false;
boolean button_changed_flag = false;
int ADC_sample_flag = 0;
const unsigned long ADC_sample_period = 100;

//Initialize Roast type and default value
char roast[4] = "250";
int roast_val = 250;
char fan[4] = "  0";
int fan_val = 0;
int motor_val = 0;

//Intialize time counters
unsigned long roast_time = 0;
char roast_time_char[6] = "00:00";
unsigned long cool_time = 0;
char cool_time_char[6] = "00:00";

//Initialize Thermal temp in Celsius
double Input = 10;
char Input_buffer [4] = "  0";
