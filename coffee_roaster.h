#define state_idle 0 //Idle state
#define state_idle_transition 1
#define state_roasting 4 //Coffee beans added
#define state_roasting_transition 5
#define state_cooling 6 //Planned cooling behavior
#define state_cooling_transition 7
#define LED_BUILTIN_TX 30

int CurrentState = 0;  

//Initialize all the state flags
int idle_state_flag = 0;
int warming_state_pre_flag = 0;
int warming_state_post_flag = 0;
int roasting_state_flag = 0;
int cooling_state_flag = 0;
int error_state_flag = 0;
int thermocouple_flag = 0;

//Initialize Roast type and default value
char roast[4] = "180";
int roast_val = 180;
