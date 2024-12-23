#include "main.h"
#include "lemlib/api.hpp" // IWYU pragma: keep
#include "lemlib/chassis/chassis.hpp"
#include "liblvgl/llemu.hpp"
#include "pros/abstract_motor.hpp"
#include "pros/adi.hpp"
#include "pros/llemu.hpp"
#include "pros/misc.h"
#include "pros/motor_group.hpp"
#include "pros/motors.h"
#include "pros/rotation.hpp"
#include "pros/rtos.hpp"
#include <cstdio>
#include "robodash/api.h"
#include "robodash/views/console.hpp"

pros::MotorGroup left_motor_group({-1, 17, -16}); // left motors on ports 1 (reversed), 2 (forwards), and 3 (reversed)
pros::MotorGroup right_motor_group({3, -12, 14}); // right motors on ports 4 (forwards), 5 (reversed), and 6 (forwards)
pros::Motor ladybrown(-20);
pros::MotorGroup intake({21, 10});
pros::adi::DigitalOut clamp('A');
pros::adi::DigitalOut doinker('C');
pros::adi::DigitalOut pistonLift('B');
pros::Imu imu(11);
// horizontal tracking wheel encoder. Rotation sensor, port 20, not reversedx
pros::Rotation horizontalEnc(-7);
// vertical tracking wheel encoder. Rotation sensor, port 11, reversed
pros::Rotation verticalEnc(13);
pros::Rotation wallrotational(5);
// horizontal tracking wheel
//lemlib::TrackingWheel horizontal_tracking_wheel(&horizontalEnc, lemlib::Omniwheel::NEW_2, -0.5);
// vertical tracking wheel
lemlib::TrackingWheel horizontal_tracking_wheel(&horizontalEnc, lemlib::Omniwheel::NEW_2, 2);
lemlib::TrackingWheel vertical_tracking_wheel(&verticalEnc, lemlib::Omniwheel::NEW_2, 0);
pros::Controller controller(pros::E_CONTROLLER_MASTER);

bool clampbool = LOW;
bool pistonliftbool = LOW;
bool doink = LOW;
bool R2_pressed = false;
bool B_pressed = false;
bool idk = false;


// drivetrain setting
lemlib::Drivetrain drivetrain(&left_motor_group, // left motor group
                              &right_motor_group, // right motor group
                              11.3125, // 10 inch track width
                              lemlib::Omniwheel::NEW_325, // using new 4" omnis
                              480, // drivetrain rpm is 360
                              0 // horizontal drift is 2. If we had traction wheels, it would have been 8
);

lemlib::OdomSensors sensors(&vertical_tracking_wheel, // vertical tracking wheel 1, set to null
                            nullptr, // vertical tracking wheel 2, set to nullptr as we are using IMEs
                            &horizontal_tracking_wheel, // horizontal tracking wheel 1
                            nullptr, // horizontal tracking wheel 2, set to nullptr as we don't have a second one
                            &imu // inertial sensor
);

lemlib::ControllerSettings lateral_controller(6.9, // proportional gain (kP)
                                              0.02, // integral gain (kI)
                                              8, // derivative gain (kD)
                                              3, // anti windup
                                              1, // small error range, in inches
                                              700, // small error range timeout, in milliseconds
                                              5, // large error range, in inches
                                              700, // large error range timeout, in milliseconds
                                              60 // maximum acceleration (slew
);

// angular PID controller
lemlib::ControllerSettings angular_controller(2.5, // proportional gain (kP) 2.5
                                              0, // integral gain (kI)
                                              20, // derivative gain (kD)
                                              3, // anti windup
                                              1, // small error range, in inches
                                              100, // small error range timeout, in milliseconds
                                              5, // large error range, in inches
                                              500, // large error range timeout, in milliseconds
                                              70 // maximum acceleration (slew)
);

// input curve for throttle input during driver control
lemlib::ExpoDriveCurve throttle_curve(3, // joystick deadband out of 127
                                     10, // minimum output where drivetrain will move out of 127 
                                     1.019 // expo curve gain
);

// input curve for steer input during driver control
lemlib::ExpoDriveCurve steer_curve(3, // joystick deadband out of 127
                                  10, // minimum output where drivetrain will move out of 127
                                  1.019 // expo curve gain
);

// create the chassis
lemlib::Chassis chassis(drivetrain, // drivetrain settings
                        lateral_controller, //  lateral PID settings
                        angular_controller, // angular PID settings
                        sensors, // odometry sensors
                        &throttle_curve, 
                        &steer_curve
);
bool arm_moving = false;
int setpos = wallrotational.get_position();
void arm_move_load(){ //macro to load disk
    arm_moving = true;
    while(arm_moving){
        ladybrown.move(10);
        while(wallrotational.get_position()>16100){
            ladybrown.move(30);
            pros::delay(5);
        }
        if(wallrotational.get_position()<16100){
            arm_moving = false;
        }
    }
    ladybrown.set_brake_mode(pros::MotorBrake::hold);
    ladybrown.set_brake_mode_all(pros::E_MOTOR_BRAKE_HOLD);
    ladybrown.move(0); 
}

void arm_move_set(){ //macro to score
    while(wallrotational.get_position()>2500){
        ladybrown.move(70);
        pros::delay(5);
    }
    ladybrown.move(0);
}

void arm_move_return(){ //macro to come back down
    while(wallrotational.get_position()<18300){
        ladybrown.move(-70);
        pros::delay(5);
    }
    ladybrown.move(0);
}

void armdown(){ //moves lady brown downwards
    ladybrown.move(-127);

}


/**
 * A callback function for LLEMU's center button.
 *
 * When this callback is fired, it will toggle line 2 of the LCD text between
 * "I was pressed!" and nothing.
 */
void on_center_button() {
}

/**
 * Runs initialization code. This occurs as soon as the program is started.
 *
 * All other competition modes are blocked by initialize; it is recommended
 * to keep execution time for this mode under a few seconds.
 */
void initialize() {
    pros::lcd::initialize(); // initialize brain screen
    chassis.calibrate(); // calibrate sensors
    ladybrown.set_brake_mode(pros::MotorBrake::hold);
}

/**
 * Runs while the robot is in the disabled state of Field Management System or
 * the VEX Competition Switch, following either autonomous or opcontrol. When
 * the robot is enabled, this task will exit.
 */
void disabled() {}

/**
 * Runs after initialize(), and before autonomous when connected to the Field
 * Management System or the VEX Competition Switch. This is intended for
 * competition-specific initialization routines, such as an autonomous selector
 * on the LCD.
 *
 * This task will exit when the robot is enabled and autonomous or opcontrol
 * starts.
 */
void competition_initialize() {}

/**
 * Runs the user autonomous code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the autonomous
 * mode. Alternatively, this function may be called in initialize or opcontrol
 * for non-competition testing purposes.
 *
 * If the robot is disabled or communications is lost, the autonomous task
 * will be stopped. Re-enabling the robot will restart the task, not re-start it
 * from where it left off.
 */ 

int current_auton_selection =7;
void autonomous() { 
    switch(current_auton_selection){
        case 0: //redneg
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -30, 1700, {.forwards=false, .maxSpeed = 40}); //p1
            chassis.waitUntilDone();
            clamp.set_value(true);
            chassis.turnToHeading(70, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 27, 1500); //p1
            chassis.waitUntilDone();
            intake.move(127);
            chassis.moveToPoint(0, 19, 1500, {.forwards = false}); //p1
            chassis.turnToHeading(90, 1000);
            chassis.waitUntilDone();
            intake.move(-127);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 14, 1200); //p1
            pros::delay(750);
            chassis.moveToPoint(0, 0, 1200, {.forwards = false}); //p1
            chassis.turnToHeading(270, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(127);
            chassis.moveToPoint(0, 10, 1200);
            chassis.turnToHeading(90, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 13.5, 1200); //p1
            chassis.waitUntilDone();
            intake.move(-127);
            pros::delay(1000);
            chassis.moveToPoint(0, -14, 1700, {.forwards = false}); //p1
            chassis.waitUntilDone();
            //chassis.moveToPoint(0, -18, 2000, {.forwards=false}); //p1






            // chassis.setPose(0, 0, 0);
            // chassis.moveToPoint(0, -28, 2000, {.forwards=false}); //p1
            // chassis.turnToHeading(-333, 300);
            // chassis.moveToPoint(-7.8, -44, 1000, {.forwards=false}); //p2
            // chassis.waitUntilDone();
            // clamp.set_value(true);
            // pros::delay(100);
            // intake.move(-127);
            // pros::delay(200);
            // chassis.moveToPoint(-20, -27, 1000, {.forwards=true, .maxSpeed = 80}); //p3
            // chassis.waitUntilDone();
            // pros::delay(700);
            // clamp.set_value(false);
            // intake.move(127);
            // chassis.moveToPoint(-44.612, -35.767, 1500, {.forwards=false}); //p4
            // chassis.waitUntilDone();
            // clamp.set_value(true);
            // pros::delay(100);
            // pistonLift.set_value(true);
            // intake.move(-127);
            // chassis.moveToPoint(-75, -10, 1800, {.forwards=true, .maxSpeed=70});
            // chassis.waitUntilDone();
            // pistonLift.set_value(false);
            // intake.move(-127);
            // pros::delay(250);
            // chassis.turnToHeading(-30, 500);
            // chassis.moveToPoint(-60, -8.25, 1500, {.forwards=false, .maxSpeed = 60});
            // pros::delay(500);
            // intake.move(-127);
            // chassis.moveToPoint(-65.581, -27.499, 1500, {.forwards=true, .maxSpeed = 70});
            // intake.move(127);
            // pros::delay(75);
            // intake.move(-127);
            break;
        case 1: //blueneg
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -30, 1200, {.forwards=false, .maxSpeed = 50}); //p1
            chassis.waitUntilDone();
            clamp.set_value(true);
            chassis.turnToHeading(-70, 750);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 27, 1000); //p1
            chassis.waitUntilDone();
            chassis.moveToPoint(0, 19, 1000, {.forwards = false}); //p1
            chassis.turnToHeading(-86, 750);
            chassis.waitUntilDone();
            intake.move(-127);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 13, 1000); //p1
            pros::delay(250);
            chassis.moveToPoint(14, 0, 1500, {.forwards = false}); //p1
            // chassis.turnToHeading(-270, 750);
            // chassis.waitUntilDone();
            // chassis.setPose(0, 0, 0);
            // intake.move(127);
            // chassis.moveToPoint(0, 10, 1000);
            // chassis.turnToHeading(-90, 750);
            chassis.waitUntilDone();
            chassis.turnToHeading(0, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 13.5, 1000); //p1
            chassis.waitUntilDone();
            intake.move(-127);
            pros::delay(500);
            chassis.moveToPoint(0, 0, 1000, {.forwards = false}); //p1
            chassis.waitUntilDone();
            chassis.turnToHeading(246, 1200);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            pistonLift.set_value(true);
            chassis.moveToPoint(0, 57, 2000, {.forwards = true, .maxSpeed = 100}); //p1
            chassis.waitUntilDone();
            pistonLift.set_value(false);
            chassis.moveToPoint(0, 42, 1000, {.forwards = false}); //p1
            break;



            // chassis.setPose(0, 0, 0);
            // chassis.moveToPoint(0, -28, 2000, {.forwards=false}); //p1
            // chassis.turnToHeading(333, 300);
            // chassis.moveToPoint(7.8, -44, 1000, {.forwards=false, .maxSpeed = 80}); //p2
            // chassis.waitUntilDone();
            // clamp.set_value(true);
            // pros::delay(100);
            // intake.move(-127);
            // pros::delay(200);
            // chassis.moveToPoint(20, -27, 1000, {.forwards=true, .maxSpeed = 80}); //p3
            // chassis.waitUntilDone();
            // pros::delay(700);
            // clamp.set_value(false);
            // intake.move(127);
            // chassis.moveToPoint(44.612, -35.767, 1500, {.forwards=false}); //p4
            // chassis.waitUntilDone();
            // clamp.set_value(true);
            // pros::delay(100);
            // pistonLift.set_value(true);
            // intake.move(-127);
            // chassis.moveToPoint(75, -10, 1800, {.forwards=true, .maxSpeed=70});
            // chassis.waitUntilDone();
            // pistonLift.set_value(false);
            // intake.move(-127);
            // pros::delay(250);
            // chassis.turnToHeading(30, 500);
            // chassis.moveToPoint(60, -8.25, 1500, {.forwards=false, .maxSpeed = 60});
            // pros::delay(500);
            // intake.move(-127);
            // chassis.moveToPoint(60.581, -27.499, 1500, {.forwards=true, .maxSpeed = 70});
            // intake.move(127);
            // pros::delay(75);
            // intake.move(-127);
            break;
        case 2: //redpos
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -30, 1700, {.forwards=false, .maxSpeed = 40}); //p1
            chassis.waitUntilDone();
            clamp.set_value(true);
            chassis.turnToHeading(-70, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 27, 1500); //p1
            chassis.waitUntilDone();
            intake.move(127);
            chassis.setPose(0, 0, 0);
            chassis.turnToHeading(160, 1000);
            chassis.waitUntilDone();
            pistonLift.set_value(true);
            intake.move(-127);
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 55, 6000, {.forwards=true, .maxSpeed = 45}); //p1
            chassis.waitUntilDone();
            // pros::delay(1000);
            // pistonLift.set_value(false);
            // chassis.moveToPoint(0, 52, 1000, {.forwards=true, .maxSpeed = 70}); //p1
            pros::delay(500);
            chassis.moveToPoint(0, 40, 1000, {.forwards=false, .maxSpeed = 50}); //p1
            chassis.waitUntilDone();
            pistonLift.set_value(false);
            chassis.turnToHeading(75, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 18, 6000, {.forwards=true, .maxSpeed = 40}); //p1





            // chassis.moveToPoint(0, -1, 1500, {.maxSpeed = 10});
            // intake.move(-127);
            // pros::delay(1000);
            // chassis.moveToPoint(0, 9, 1500, {.minSpeed = 10});
            // chassis.turnToHeading(270, 750);
            // chassis.moveToPoint(15, 7, 1500, {.forwards = false, .maxSpeed = 50});
            // chassis.waitUntilDone();
            // pros::delay(100);
            // clamp.set_value(true);
            // pros::delay(400);
            // chassis.moveToPoint(20, 28, 1500, {.forwards = true, .minSpeed = 20});
            // chassis.moveToPoint(30, 40, 1500, {.forwards = true, .minSpeed = 70});
            // chassis.moveToPoint(40, 80, 2000, {.forwards = true});
            // pros::delay(800);
            // chassis.moveToPoint(48, 55, 1500, {.forwards = true});
            // pros::Task task{[] {
            //     arm_move_load();
            // }};
            // chassis.moveToPoint(35, 62, 1500, {.forwards = false});
            break;
        case 3: //bluepos
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -30, 1700, {.forwards=false, .maxSpeed = 40}); //p1
            chassis.waitUntilDone();
            clamp.set_value(true);
            chassis.turnToHeading(70, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 27, 1500); //p1
            chassis.waitUntilDone();
            intake.move(127);
            chassis.setPose(0, 0, 0);
            chassis.turnToHeading(-160, 1000);
            chassis.waitUntilDone();
            pistonLift.set_value(true);
            intake.move(-127);
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 55, 6000, {.forwards=true, .maxSpeed = 45}); //p1
            chassis.waitUntilDone();
            // pros::delay(1000);
            // pistonLift.set_value(false);
            // chassis.moveToPoint(0, 52, 1000, {.forwards=true, .maxSpeed = 70}); //p1
            pros::delay(500);
            chassis.moveToPoint(0, 40, 1000, {.forwards=false, .maxSpeed = 50}); //p1
            chassis.waitUntilDone();
            pistonLift.set_value(false);
            chassis.turnToHeading(-75, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 18, 6000, {.forwards=true, .maxSpeed = 40}); //p1





            // chassis.moveToPoint(0, -1, 1500, {.maxSpeed = 10});
            // intake.move(-127);
            // pros::delay(1000);
            // chassis.moveToPoint(0, 9, 1500, {.minSpeed = 10});
            // chassis.turnToHeading(270, 750);
            // chassis.moveToPoint(15, 7, 1500, {.forwards = false, .maxSpeed = 50});
            // chassis.waitUntilDone();
            // pros::delay(100);
            // clamp.set_value(true);
            // pros::delay(400);
            // chassis.moveToPoint(20, 28, 1500, {.forwards = true, .minSpeed = 20});
            // chassis.moveToPoint(30, 40, 1500, {.forwards = true, .minSpeed = 70});
            // chassis.moveToPoint(40, 80, 2000, {.forwards = true});
            // pros::delay(800);
            // chassis.moveToPoint(48, 55, 1500, {.forwards = true});
            // pros::Task task{[] {
            //     arm_move_load();
            // }};
            // chassis.moveToPoint(35, 62, 1500, {.forwards = false});
            break;
        case 4: //rednegwin
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 13.5, 1000);
            chassis.turnToHeading(90, 700);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 9, 700);
            chassis.waitUntilDone();
            ladybrown.move(127);
            pros::delay(500);
            ladybrown.move(-127);
            pros::delay(500);
            ladybrown.move(0);
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -10, 700, {.forwards = false});
            chassis.waitUntilDone();
            chassis.turnToHeading(320, 750);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -34, 1700, {.forwards = false, .maxSpeed = 50});
            chassis.waitUntilDone();
            clamp.set_value(true);
            chassis.turnToHeading(135, 1000);

            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 27, 1000); //p1
            chassis.waitUntilDone();
            chassis.moveToPoint(0, 22, 1000, {.forwards = false}); //p1
            chassis.turnToHeading(90, 800);
            chassis.waitUntilDone();
            intake.move(-127);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 16, 1000); //p1
            pros::delay(750);
            chassis.moveToPoint(0, 0, 1000, {.forwards = false}); //p1
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.turnToHeading(90, 800);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 40, 1000, {.forwards = true, .maxSpeed = 100}); //p1
            break;
        case 5: //bluenegwin
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 13.5, 1000);
            chassis.turnToHeading(-90, 700);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 9, 700);
            chassis.waitUntilDone();
            ladybrown.move(127);
            pros::delay(500);
            ladybrown.move(-127);
            pros::delay(500);
            ladybrown.move(0);
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -10, 700, {.forwards = false});
            chassis.waitUntilDone();
            chassis.turnToHeading(-320, 750);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, -34, 1500, {.forwards = false, .maxSpeed = 50});
            chassis.waitUntilDone();
            clamp.set_value(true);
            chassis.turnToHeading(-135, 1000);

            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            intake.move(-127);
            chassis.moveToPoint(0, 27, 1000); //p1
            chassis.waitUntilDone();
            chassis.moveToPoint(0, 22, 1000, {.forwards = false}); //p1
            chassis.turnToHeading(-90, 800);
            chassis.waitUntilDone();
            intake.move(-127);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 15, 1000); //p1
            pros::delay(750);
            chassis.moveToPoint(0, 0, 1000, {.forwards = false}); //p1
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.turnToHeading(-90, 800);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 40, 1000, {.forwards = true, .maxSpeed = 100}); //p1
            break;
        case 6: //skills old
            chassis.setPose(0, 0, 0);
            ladybrown.move(127);
            pros::delay(400);
            ladybrown.move(-127);
            pros::delay(400);
            chassis.moveToPoint(0, -15, 1000, {.forwards = false});
            chassis.turnToHeading(270, 890);
            chassis.waitUntilDone();
            chassis.setPose(0,0,0);
            chassis.moveToPoint(0, -25, 1200, {.forwards = false, .maxSpeed = 45});
            chassis.waitUntilDone();
            clamp.set_value(true);
            pros::delay(250);
            chassis.turnToHeading(270, 1000);
            intake.move(-127);
            chassis.waitUntilDone();
            chassis.setPose(0,0,0);
            chassis.moveToPoint(0, 20, 1000);
            chassis.turnToHeading(263, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0,0,0);
            chassis.moveToPoint(0, 25, 1000);
            chassis.turnToHeading(67, 1000);
            chassis.waitUntilDone();
            chassis.setPose(0,0,0);
            intake.move(-127);
            chassis.moveToPoint(0, 32, 1250);
            chassis.moveToPoint(0, 7, 1150, {.forwards = false});
            chassis.turnToHeading(207, 1200);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 20, 750, {.minSpeed = 45});
            chassis.moveToPoint(0, 36, 1000, {.maxSpeed = 45});
            chassis.moveToPoint(0, 23, 1000, {.forwards = false});
            chassis.turnToHeading(90, 800);
            chassis.waitUntilDone();
            chassis.setPose(0, 0, 0);
            chassis.moveToPoint(0, 13, 1000);
            chassis.moveToPoint(-15.5, 28, 1500, {.forwards = false});
            chassis.waitUntilDone();
            clamp.set_value(false);
            chassis.setPose(0,0,0);
            chassis.moveToPoint(0, 15, 750);
            chassis.turnToHeading(-135, 800);
            chassis.waitUntilDone();
            chassis.setPose(0,0,0);
            chassis.moveToPoint(0, -70, 1500, {.forwards = false, .minSpeed = 40});
            chassis.moveToPoint(0, -80, 700, {.forwards = false, .maxSpeed = 40});
            break;
        case 7://skills new

            //QUADRANT 1
            chassis.setPose(0, 0, 0);
            ladybrown.move(127); //scores on alliance wall stake by raising ladybrown
            pros::delay(400);
            ladybrown.move(-127); //lowers ladybrown
            pros::delay(400);
            chassis.moveToPoint(0, -3, 500, {.forwards = false, .minSpeed = 40});
            chassis.moveToPoint(22, -9, 1900, {.forwards = false, .maxSpeed = 60}); //moves to mogo 1
            chassis.waitUntilDone();
            pros::delay(100);
            clamp.set_value(true); //picks up mogo 1
            pros::delay(500);
            intake.move(-127);
            chassis.moveToPoint(28, -24, 1700, {.minSpeed = 20}); //ring 1
            chassis.moveToPoint(70, -49, 1900, {.maxSpeed = 90}); //ring 2
            chassis.moveToPoint(64, -35, 2800, {.maxSpeed = 90}); //ring 3
            chassis.turnToHeading(0, 750);
            chassis.moveToPoint(64, 5, 3800, {.maxSpeed = 55}); //ring 4 and 5
            chassis.moveToPoint(56, -13, 1500, {.forwards = false});
            chassis.waitUntilDone();
            chassis.moveToPoint(75, -13, 1500, {.maxSpeed = 70}); //ring 6
            chassis.waitUntilDone();
            pros::delay(500);
            chassis.moveToPoint(83, 2, 1500, {.forwards = false}); //moves to corner
            chassis.waitUntilDone();
            clamp.set_value(false); //drops mogo 1
            intake.move(127);


            //QUADRANT 2
            pros::delay(75);
            chassis.moveToPoint(73, -20, 1000);
            chassis.waitUntilDone();
            intake.move(-127);
            chassis.moveToPoint(25, -11.5, 1700, {.forwards = false, .maxSpeed = 90, .minSpeed = 40});
            chassis.moveToPoint(3, -11.5, 1800,  {.forwards = false, .maxSpeed = 35}); //move to mogo 2
            chassis.waitUntilDone();
            pros::delay(200);
            clamp.set_value(true); //clamp mogo
            pros::delay(200);
            chassis.moveToPoint(2, -27, 1300, {.minSpeed = 20}); //ring 1
            chassis.moveToPoint(-37.9, -55, 2000, {.maxSpeed = 90}); //ring 2
            chassis.moveToPoint(-28, -57, 1700, {.forwards = false});
            chassis.turnToHeading(0, 800);
            chassis.moveToPoint(-28, 6, 3300, {.maxSpeed = 40}); //ring 3, 4, and 5
            chassis.moveToPoint(-20, -10, 1500, {.forwards = false});
            chassis.moveToPoint(-40, -10, 1500, {}); //ring 6
            pros::delay(500); //can remove if needed
            chassis.moveToPoint(-42, 6, 1500, {.forwards = false}); //moves to corner
            chassis.waitUntilDone();
            clamp.set_value(false); //drop mogo 2
            intake.move(127);
            pros::delay(200);
            intake.move(-127);
            
            //QUADRANT 3
            chassis.moveToPoint(-25, -47, 2000, {});
            chassis.moveToPoint(-2, -82, 2500, {.maxSpeed = 90}); //ring 1
            pros::delay(700);
            intake.move(0);
            chassis.turnToHeading(315, 880); //can remove
            chassis.moveToPoint(20, -95, 2500, {.forwards = false}); //move to mogo 2
            chassis.waitUntilDone();
            clamp.set_value(true); //clamp mogo 2
            pros::delay(100);
            intake.move(-127); //scores ring 1 on mogo 3
            chassis.moveToPoint(-37, -55, 2000, {.maxSpeed = 90}); //ring 2
            chassis.moveToPoint(-29, -60, 800, {.forwards = false});
            chassis.turnToHeading(180, 800);
            chassis.moveToPoint(-29, -103, 2800, {.maxSpeed = 70}); //ring 3 and 4
            chassis.moveToPoint(-29, -83, 1700, {.forwards = false});
            chassis.moveToPoint(-45, -83, 1700, {.maxSpeed = 70}); //ring 5
            chassis.moveToPoint(-53, -105, 1700, {.forwards = false, .maxSpeed = 70}); //move to corner
            chassis.waitUntilDone();
            clamp.set_value(false); //drop mogo 3
            pros::delay(100);
            chassis.turnToHeading(90, 1000);

            //QUADRANT 4
            chassis.moveToPoint(70, -105, 2700, {}); //run into mogo and push into corner
            chassis.moveToPoint(40, -105, 2700, {}); //back out of corner slightly

            break;

        case 8: // 10/28 matchauto red negative side
            chassis.setPose(0,0,0);
            chassis.moveToPoint(0, -30, 1000, {.forwards = false}); //mogo 1
            chassis.waitUntilDone();
            clamp.set_value(true);
            pros::delay(100);
            intake.move(-127); //spins intake to score ring 1 (preload)
            chassis.turnToHeading(90, 800);
            chassis.moveToPoint(28.5, -30, 1000); //ring 2
            chassis.turnToHeading(180, 800);
            chassis.moveToPoint(28.5, -50, 1000); //ring 3
            chassis.moveToPoint(28.5, -30, 1000, {.forwards = false});
            break;

        case 9: // 10/11 matchauto red negative side
            chassis.setPose(0,0,0);
            chassis.moveToPoint(0, -28, 1000, {.forwards = false}); //mogo 1
            chassis.waitUntilDone();
            clamp.set_value(true);
            pros::delay(300);
            intake.move(-127); //spins intake to score ring 1 (preload)
            chassis.turnToHeading(90, 800);
            chassis.moveToPoint(30, -28, 1000); //ring 2
            break;

        case 10: // 10/29 matchauto red positive side
            chassis.setPose(0,0,0);
            chassis.moveToPoint(10, -52.5, 1700, {.forwards = false}); //mogo 1
            chassis.waitUntilDone();
            clamp.set_value(true);
            pros::delay(100);
            intake.move(-127); //spins intake to score ring 1 (preload)
            chassis.turnToHeading(0, 800);
            chassis.moveToPoint(10, -35, 1200); //ring 2
            break; 
        case 11: // 10/30 match auto red positive side
            chassis.setPose(0, 0, 330);
            chassis.moveToPoint(-5, -10, 1000, {.forwards = false});
            intake.move(-127); //scores ring 1 on alliance stake
            chassis.moveToPoint(20, 17, 2000); //pick up ring 2
            intake.move(0); //stop ring 2 in the intake
            chassis.moveToPoint(10, 32, 1700); //move to mogo
            chassis.waitUntilDone();
            clamp.set_value(true); //pick up mogo
            pros::delay(100);
            intake.move (-127); //score ring 2 on mogo
    }
}

/**
 * Runs the operator control code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the operator
 * control mode.
 *
 * If no competition control is connected, this function will run immediately
 * following initialize().
 *
 * If the robot is disabled or communications is lost, the
 * operator control task will be stopped. Re-enabling the robot will restart the
 * task, not resume it from where it left off.
 */
void opcontrol() {
    pros::delay(20);
    while (true) {
         // print robot location to the brain screen
        pros::lcd::print(0, "X: %f", chassis.getPose().x); // x
        pros::lcd::print(1, "Y: %f", chassis.getPose().y); // y
        pros::lcd::print(2, "Theta: %f", chassis.getPose().theta); // heading
        pros::lcd::print(7, "pos:%ld", wallrotational.get_position());
        
        // get left y and right x positions
        int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
        

        //move ladybrown mech
        if(controller.get_digital(pros::E_CONTROLLER_DIGITAL_DOWN)){
            ladybrown.move(-127);
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_UP)){
			ladybrown.move(127);
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_R1)){
			ladybrown.move(127);
        } else{
            ladybrown.move(0);
        }


        if(controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A)){
            pros::Task task{[] {
                arm_move_load();
            }};
        }



        //move intake (NOT TOGGLE | MUST HOLD)
        if(controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1)){
			intake.move(-127);
        } else if (controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2)){
			intake.move(127);
        } else{
            intake.move(0);
        }

        // mogo mech controls
		if(controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_R2)){
		    if(R2_pressed){
                clamp.set_value(false);
                R2_pressed = false;
            } else{
                clamp.set_value(true);
                R2_pressed = true;
            }
		}
        
        //piston lift controls
        if(controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_B)){
		    if(B_pressed){
                pistonLift.set_value(false);
                B_pressed = false;
            } else{
                pistonLift.set_value(true);
                B_pressed = true;
            }
		}

        // BOT MOVEMENT!!!!!!!

        double ctrX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X); 
        double ctrY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);

        double pwrF = ctrY;
        double pwrT = ctrX;

        if(1){
            double pwrF3 = pwrF*pwrF*pwrF;
            pwrF3 = pwrF3/10000;
        }

        if(1){
            double pwrT3 = pwrT*pwrT*pwrT;
            pwrT3 = pwrT3/10000;
            pwrT*=.925; //makes turning less sensitive
        }

        double pwrL = pwrF+pwrT;
        double pwrR = pwrF-pwrT;
        left_motor_group.move(pwrL);
        right_motor_group.move(pwrR);

        // delay to save resources
        pros::delay(25);
    }
}