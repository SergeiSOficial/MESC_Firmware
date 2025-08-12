/*
 * MP2_V0_1.h
 *
 *  Created on: Dec 16, 2022
 *      Author: HPEnvy
 */

#ifndef INC_MP2_V0_1_H_
#define INC_MP2_V0_1_H_
//Pick a motor for default
// #define MCMASTER_70KV_8080//QS165

#define CUSTOM_DEADTIME 800 //ns

#define SHUNT_POLARITY -1.0f

#define ABS_MAX_PHASE_CURRENT 50.0f
#define ABS_MAX_BUS_VOLTAGE 45.0f
#define ABS_MIN_BUS_VOLTAGE 16.0f
#define R_SHUNT 0.0005f
#define OPGAIN 20.0f

#define R_VBUS_BOTTOM 3010.0f //Phase and Vbus voltage sensors
#define R_VBUS_TOP 72300.0f


#define MAX_ID_REQUEST 2.0f
#define MAX_IQ_REQUEST 70.0f

#define SEVEN_SECTOR		//Normal SVPWM implemented as midpoint clamp. If not defined, you will get 5 sector, bottom clamp
//#define DEADTIME_COMP		//This injects extra PWM duty onto the timer which effectively removes the dead time.
#define DEADTIME_COMP_V 10

//Inputs
#define GET_THROTTLE_INPUT _motor->Raw.ADC_in_ext1 = ADC_buffer[2]  // Throttle

#define USE_FIELD_WEAKENINGV2

#define USE_LR_OBSERVER

/////////////////////Related to ANGLE ESTIMATION////////////////////////////////////////
//#define INTERPOLATE_V7_ANGLE

#define DEFAULT_SENSOR_MODE MOTOR_SENSOR_MODE_SENSORLESS

#define USE_HFI
#define HFI_VOLTAGE 8.0f
#define HFI_TEST_CURRENT 1.0f
#define HFI_THRESHOLD 0.0f
#define HFI45
#define DEFAULT_HFI_TYPE HFI_TYPE_45
//#define DEFAULT_HFI_TYPE HFI_TYPE_45
// #define DEFAULT_HFI_TYPE HFI_TYPE_D
//#define DEFAULT_HFI_TYPE HFI_TYPE_SPECIAL
#define DEFAULT_STARTUP_SENSOR STARTUP_SENSOR_HFI

//#define USE_HALL_START
#define HALL_VOLTAGE_THRESHOLD 1.5f

// #ifdef DEFAULT_MOTOR_PP
// #undef DEFAULT_MOTOR_PP
// #define DEFAULT_MOTOR_PP 10
// #endif


#define MAX_MOTOR_PHASE_CURRENT 50.0f //2A seems like a reasonable default for any motor
#define DEFAULT_MOTOR_POWER 1000.0f //
#define DEFAULT_FLUX_LINKAGE 0.0135f//Could be absolutely anything.
#define DEFAULT_MOTOR_Ld 0.000087f //Henries Could be anything, but setting it low means the current controller P term will be more stable/slower
#define DEFAULT_MOTOR_Lq 0.000087f//Henries
#define DEFAULT_MOTOR_R 0.0050f //Ohms Could be anything, but setting it lower means slower I term on the PID
#define DEFAULT_MOTOR_PP 10 //Pole Pairs
// #define MIN_FLUX_LINKAGE DEFAULT_FLUX_LINKAGE
// #define MAX_FLUX_LINKAGE DEFAULT_FLUX_LINKAGE
// #define FLUX_LINKAGE_GAIN (10.0f * sqrtf(DEFAULT_FLUX_LINKAGE))
// #define NON_LINEAR_CENTERING_GAIN 5000.0f

#define ADC_OFFSET_DEFAULT 1960

//#define LOGGING

//#define USE_SALIENT_OBSERVER //If not defined, it assumes that Ld and Lq are equal, which is fine usually.

#endif /* INC_MP2_V0_1_H_ */
