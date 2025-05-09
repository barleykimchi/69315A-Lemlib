#include "main.h"
#include "lemlib/api.hpp" // IWYU pragma: keep
#include "lemlib/chassis/trackingWheel.hpp"
#include "pros/adi.hpp"
#include "pros/misc.h"
#include "pros/misc.hpp"
#include "autons.hpp"
#include "pros/motor_group.hpp"
#include "pros/motors.h"

// Chassis constructors
pros::MotorGroup leftMotors{{-1, 2, 3}, pros::MotorGearset::blue};
pros::MotorGroup rightMotors{{-13, -12, 11}, pros::MotorGearset::blue};

// Motor constructors
pros::Motor intake(14, pros::MotorGearset::blue);
pros::MotorGroup lb({19, -20}, pros::MotorGearset::green);

// Pneumatic constructors
pros::adi::Pneumatics mogoL('A', true);
pros::adi::Pneumatics mogoR('B', true);
pros::adi::Pneumatics doinkerL('C', false);
pros::adi::Pneumatics doinkerR('D', false);

// Sensor constructors
pros::Imu imu(17);
pros::Rotation rSensor(16);
pros::Optical oSensor(5);

// Controller constructor
pros::Controller controller(pros::E_CONTROLLER_MASTER);

// Robodash brain screen constructor
rd::Console brain;

// Extend mogo function
void extendMogo(){
    mogoL.extend();
    mogoR.extend();
}  

// Retract mogo function
void retractMogo(){
    mogoL.retract();
    mogoR.retract();
}

// ----------- L A D Y B R O W N ----------- //
// Ladybrown states arrays
// int autonStates[4] = {150, 12000, 25000, 50000};      // Remember: centidegrees; 150, 12000, 50000
int driverStates[3] = {0, 110, 600}; // 600
int numStates = 3;
// 1 less 0

// Ladybrown variables
int currState = 0;
int target = driverStates[currState];  // Default to driver states

// Ladybrown flags
// bool isAutonMode = false;
bool lbTaskEnabled = true;

// Depending on flag, cycles through states
// void nextState() {
//     int numStates = isAutonMode ? 4 : 3;
//     currState = (currState + 1) % numStates;
//     target = isAutonMode ? autonStates[currState] : driverStates[currState];

//     if(currState == 0){
//         brain.printf("\nState rest");
//     }
// }

// void nextState(){
//     currState += 1;
//     if(currState == 3){
//         currState = 0;
//     }
// }

void nextState() {

    currState += 1;
    if(currState == numStates){
        currState = 0;
    }
    target = driverStates[currState];

    // int numStates = 3;
    // currState = (currState + 1) % numStates;
    // target = isAutonMode ? autonStates[currState] : driverStates[currState];
    // brain.printf("\nState: %d, Target: %d", currState, target);
	// brain.printf("\nCurrent: %dmA", lb.get_current_draw());
}

// Set up Ladybrown PID & controls velocity 
void liftControl() {
    double kp = 0.4;
    lb.move(kp * (target - rSensor.get_position()/100.0));
    lb.set_brake_mode((pros::E_MOTOR_BRAKE_HOLD));
}

// void debugEncoder() {
//     while(true) {
//         brain.printf("Pose: %d", chassis.getPose());
//         pros::delay(500);
//     }
// }

// void liftControl() {
//     double kp = 0.8;  // Increased gain
//     int error = target - rSensor.get_position();
//     int output = -(kp * error);  // Keep negative sign
    
//     // Add speed limiting
//     if (output > 127) output = 127;
//     if (output < -127) output = -127;
    
//     lb.move(output);
//     lb.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
    
//     // Debug output
//     brain.printf("\nTgt:%d Pos:%d Out:%d", target, rSensor.get_position(), output);
// }

// ----------- C O L O R S O R T -----------//
// Color Sort Task Variables
int targetHue = 220;
const int HUE_TOLERANCE = 10;
bool sorterEnabled = false;

void colorSortTask(void* param) {
    while(true){
        if(sorterEnabled){
          oSensor.set_led_pwm(100);
          int currHue = oSensor.get_hue();
          controller.set_text(2, 0, "Color Sort: On ");
    
          if(abs(currHue - targetHue) <= HUE_TOLERANCE){
            intake.move_voltage(12000);
            pros::delay(75);
            intake.move_voltage(0);
            pros::delay(600);
            intake.move_voltage(12000);
          }
    
        } else {
          controller.set_text(2, 0, "Color Sort: Off");
        }
        pros::delay(20);
      }
}

rd::Selector selector({
    {"Blue Positive WQ", NEW_B_P_qual},
    {"Blue Negative WQ", NEW_B_N_qual},
    {"Red Positive WQ", NEW_R_P_qual},
    {"Red Negative WQ", NEW_R_N_qual},
    {"Testing/tuning", turnTest},
    {"Blue Positive GOAL RUSH", B_P_goalrush},
    {"Skills", skills},
    {"Red Positive Ring Rush", R_P_ringrush},
    {"Blue Positive Ring Rush", B_P_ringrush},
    {"RED Negative Ring Rush", R_N_ringrush},
    {"BLUE Negative Ring Rush", B_N_ringrush}
});

// tracking wheels
pros::Rotation horizontalEnc(4);
pros::Rotation verticalEnc(-18);
lemlib::TrackingWheel horizontal(&horizontalEnc, lemlib::Omniwheel::NEW_275, -4); 
lemlib::TrackingWheel vertical(&verticalEnc, lemlib::Omniwheel::NEW_275, 0.0625); //0.125

// drivetrain settings
lemlib::Drivetrain drivetrain(&leftMotors, // left motor group
                              &rightMotors, // right motor group
                              10.25, // 10 inch track width
                              lemlib::Omniwheel::NEW_325, // using new 4" omnis
                              450, // drivetrain rpm is 360
                              2 // horizontal drift is 2. If we had traction wheels, it would have been 8
);

// lateral PID controller
lemlib::ControllerSettings lateral_controller(9, // proportional gain (kP)
                                              0, // integral gain (kI)
                                              30, // derivative gain (kD)
                                              0, // anti windup
                                              0, // small error range, in inches
                                              0, // small error range timeout, in milliseconds
                                              0, // large error range, in inches
                                              0, // large error range timeout, in milliseconds
                                              0 // maximum acceleration (slew)
);

// angular PID controller
lemlib::ControllerSettings angular_controller(2, // proportional gain (kP)
                                              0, // integral gain (kI)
                                              11, // derivative gain (kD)
                                              0, // anti windup
                                              0, // small error range, in inches
                                              0, // small error range timeout, in milliseconds
                                              0, // large error range, in inches
                                              0, // large error range timeout, in milliseconds
                                              0 // maximum acceleration (slew)
);

// sensors for odometry
lemlib::OdomSensors sensors(&vertical, // vertical tracking wheel
                            nullptr, // vertical tracking wheel 2, set to nullptr as we don't have a second one
                            &horizontal, // horizontal tracking wheel
                            nullptr, // horizontal tracking wheel 2, set to nullptr as we don't have a second one
                            &imu // inertial sensor
);

// input curve for throttle input during driver control
lemlib::ExpoDriveCurve throttleCurve(3, // joystick deadband out of 127
                                     13, // minimum output where drivetrain will move out of 127
                                     1.019 // expo curve gain
);

// input curve for steer input during driver control
lemlib::ExpoDriveCurve steerCurve(3, // joystick deadband out of 127
                                  11, // minimum output where drivetrain will move out of 127
                                  1.019 // expo curve gain
);

// create the chassis
lemlib::Chassis chassis(drivetrain, lateral_controller, angular_controller, sensors, &throttleCurve, &steerCurve);

void initialize() {
    chassis.calibrate(); // calibrate sensors
	chassis.setBrakeMode(pros::E_MOTOR_BRAKE_HOLD);
    intake.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
    lb.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);

    // thread to for brain screen and position logging
    pros::Task screenTask([&]() {
        while (true) {
            // print robot location to the brain screen
            // brain.printf("X: %f", chassis.getPose().x); // x
            brain.printf("\nY: %f", chassis.getPose().y); // y
            // brain.printf("\nTheta: %f", chassis.getPose().theta); // heading
            // log position telemetry
            lemlib::telemetrySink()->info("Chassis pose: {}", chassis.getPose());
            // delay to save resources
            pros::delay(50);
        }
    });

	// Ladybrown Lift Task
    pros::Task liftControlTask([]{
        while (true) {
            if(lbTaskEnabled){
                liftControl();
            }
            pros::delay(10);
        }
    });

    // Color Sort Task
    pros::Task colorSortTaskHandler(colorSortTask);
}

void disabled() {}
void competition_initialize() {}

void autonomous() {

    // isAutonMode = true;
    sorterEnabled = true;
    extendMogo();
    oSensor.set_led_pwm(100);

    selector.run_auton();
}

void opcontrol() {

    lbTaskEnabled = true;
    bool rDoinkerToggle = false;
    bool lDoinkerToggle = false;

    while (true) {
        // get joystick positions
        int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
        // move the chassis with curvature drive
        chassis.arcade(leftY, rightX);

        // isAutonMode = false;
        sorterEnabled = false;

		// Intake controls
        if (controller.get_digital(DIGITAL_R1)){
            intake.move_velocity(12000);         
        } else if (controller.get_digital(DIGITAL_R2)){
            intake.move_velocity(-12000); 
        } else {
            intake.move_velocity(0); 
        }

        // LB lift task control
        if (controller.get_digital_new_press(DIGITAL_RIGHT)) {
            lbTaskEnabled = true;
            nextState();
            pros::delay(20);
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_UP)){
            lbTaskEnabled = false;
            lb.move(127);
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_DOWN)){
            lbTaskEnabled = false;
            lb.move(-40);
        } else {
            lb.move(0);
        }   

        // Mogo mech control
        if (controller.get_digital(DIGITAL_L1)){
            extendMogo();
        } else if (controller.get_digital(DIGITAL_L2)){
            retractMogo();
        }

        // Right doinker control
        if(controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A)){
            if(rDoinkerToggle == false){
                doinkerR.extend();
                rDoinkerToggle = true;
            } else {
                doinkerR.retract();
                rDoinkerToggle = false;
            }
        }

        // Left doinker control
        if(controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_Y)){
            if(lDoinkerToggle == false){
                doinkerL.extend();
                lDoinkerToggle = true;
            } else {
                doinkerL.retract();
                lDoinkerToggle = false;
            }
        }

        if(controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_LEFT)){
            rSensor.reset_position();
        }

        controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A);

        // delay to save resources
        pros::delay(10);
    }
}
