// DEFINE REFLOW PARAMETERS
// All user-defined times in seconds
// All temperatures are in Celsius
double MAX_STARTING_TEMP = 30; 

#define RAMP_TO_SOAK_TARGET_TEMP       120
#define RAMP_TO_SOAK_TEMP_ERROR          3
#define RAMP_TO_SOAK_DURATION           60
#define RAMP_TO_SOAK_CONTROL_INTERVAL    1

#define SOAK_TARGET_TEMP               160
#define SOAK_TEMP_ERROR                  3
#define SOAK_DURATION                   80
#define SOAK_CONTROL_INTERVAL            1

#define RAMP_TO_REFLOW_TARGET_TEMP     240
#define RAMP_TO_REFLOW_TEMP_ERROR        1
#define RAMP_TO_REFLOW_DURATION         40
#define RAMP_TO_REFLOW_CONTROL_INTERVAL  1

#define REFLOW_TARGET_TEMP             245
#define REFLOW_TEMP_ERROR                1
#define REFLOW_DURATION                 60
#define REFLOW_CONTROL_INTERVAL          1
 
#define COOLING_TARGET_TEMP             40
#define COOLING_TEMP_ERROR               2
#define COOLING_DURATION                60
#define COOLING_CONTROL_INTERVAL         1
