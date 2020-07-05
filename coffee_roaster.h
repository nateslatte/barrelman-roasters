#define state_idle 0 //Idle state
#define state_idle_transition 1
#define state_idle_to_debug_transition 2
#define state_roasting 3 //Roast started
#define state_roasting_transition 4
#define state_cooling 5 //Planned cooling behavior
#define state_cooling_transition 6
#define state_debug 7
#define state_debug_transition 8

#define LED_BUILTIN_TX 30

int CurrentState = 0;  

//Initialize all the state flags
int idle_state_flag = 0;
int roasting_state_flag = 0;
int cooling_state_flag = 0;
int debug_state_flag = 0;
int thermocouple_flag = 0;
int refresh_display_flag = 0;
int check_buttons_flag = 0;

//Initialize Roast type and default value
char roast[4] = "200";
int roast_val = 200;
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
