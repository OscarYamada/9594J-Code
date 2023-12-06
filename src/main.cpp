#include "main.h"
#include "lemlib/api.hpp"
#include "lemlib/logger/stdout.hpp"
#include "pros/misc.h"

// Controller and Sensors
pros::Controller controller(pros::E_CONTROLLER_MASTER);
pros::Imu imu(17);
pros::Rotation cata_rot(16);

// Pneumatics and 3Wire
pros::ADIDigitalOut wings ('G'); 
pros::ADIDigitalOut blocker ('H');

// other motors
pros::Motor cata(15, pros::E_MOTOR_GEARSET_36, true); // cata motor. port 8
pros::Motor intake(14, pros::E_MOTOR_GEARSET_06); // intake motor. port 20

// drive motors
pros::Motor lF(20, pros::E_MOTOR_GEARSET_06, true); // left front motor. port 20, reversed
pros::Motor lM(18, pros::E_MOTOR_GEARSET_06, true); // left middle motor. port 19, reversed
pros::Motor lB(19, pros::E_MOTOR_GEARSET_06, false); // left back motor. port 18
pros::Motor rF(11, pros::E_MOTOR_GEARSET_06, false); // right front motor. port 11
pros::Motor rM(13, pros::E_MOTOR_GEARSET_06, false); // right middle motor. port 13
pros::Motor rB(12, pros::E_MOTOR_GEARSET_06, true); // right back motor. port 12, reversed

// motor groups
pros::MotorGroup leftMotors({lF, lM, lB}); // left motor group
pros::MotorGroup rightMotors({rF, rM, rB}); // right motor group

// tracking wheels
pros::Rotation horizontalEnc(4);
// horizontal tracking wheel. 2.75" diameter, 3.7" offset, back of the robot
lemlib::TrackingWheel horizontal(&horizontalEnc, lemlib::Omniwheel::NEW_275, -3.7);

// drivetrain settings
lemlib::Drivetrain drivetrain(
	&leftMotors, // left motor group
    &rightMotors, // right motor group
    10, // 10 inch track width
    lemlib::Omniwheel::NEW_325, // using new 3.25" omnis
    360, // drivetrain rpm is 360
    8 // chase power is 8. If we didn't have traction wheels, it would be 2
);

// lateral motion controller
lemlib::ControllerSettings linearController(
	10, // proportional gain (kP)
    30, // derivative gain (kD)
    1, // small error range, in inches
    100, // small error range timeout, in milliseconds
    3, // large error range, in inches
    500, // large error range timeout, in milliseconds
    20 // maximum acceleration (slew)
);

// angular motion controller
lemlib::ControllerSettings angularController(
	2, // proportional gain (kP)
	10, // derivative gain (kD)
    1, // small error range, in degrees
    100, // small error range timeout, in milliseconds
    3, // large error range, in degrees
    500, // large error range timeout, in milliseconds
    20 // maximum acceleration (slew)
);

// sensors for odometry
// note that in this example we use internal motor encoders, so we don't pass vertical tracking wheels
lemlib::OdomSensors sensors(
	nullptr, // vertical tracking wheel 1, set to nullptr as we don't have one
    nullptr, // vertical tracking wheel 2, set to nullptr as we don't have one
    &horizontal, // horizontal tracking wheel 1
    nullptr, // horizontal tracking wheel 2, set to nullptr as we don't have a second one
    &imu // inertial sensor
);

// create the chassis
lemlib::Chassis chassis(drivetrain, linearController, angularController, sensors);

/**
 * Runs initialization code. This occurs as soon as the program is started.
 *
 * All other competition modes are blocked by initialize; it is recommended
 * to keep execution time for this mode under a few seconds.
 */

void initialize() {
    pros::lcd::initialize(); // initialize brain screen
    chassis.calibrate(); // calibrate sensors
    chassis.setPose(0, 0, 0); //set the pose to origin
    //set motors brake modes
    leftMotors.set_brake_modes(pros::E_MOTOR_BRAKE_COAST);
    rightMotors.set_brake_modes(pros::E_MOTOR_BRAKE_COAST);
    cata.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
    intake.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);

    // the default rate is 50. however, if you need to change the rate, you
    // can do the following.
    // lemlib::bufferedStdout().setRate(...);
    // If you use bluetooth or a wired connection, you will want to have a rate of 10ms

    // for more information on how the formatting for the loggers
    // works, refer to the fmtlib docs

    // thread to for brain screen and position logging
    pros::Task screenTask([&]() {
        lemlib::Pose pose(0, 0, 0); //creates new pose object
        while (true) {
            // print robot location to the brain screen
            pros::lcd::print(0, "X: %f", chassis.getPose().x); // x
            pros::lcd::print(1, "Y: %f", chassis.getPose().y); // y
            pros::lcd::print(2, "Theta: %f", chassis.getPose().theta); // heading
            // log position telemetry
            lemlib::telemetrySink()->info("Chassis pose: {}", chassis.getPose());
            // delay to save resources
            pros::delay(50);
        }
    });
}

/**
 * Runs while the robot is disabled
 */
void disabled() {}

/**
 * runs after initialize if the robot is connected to field control
 */
void competition_initialize() {}

// get a path used for pure pursuit
// this needs to be put outside a function
ASSET(example_txt); // '.' replaced with "_" to make c++ happy

/**
 * Runs during auto
 *
 * This is an example autonomous routine which demonstrates a lot of the features LemLib has to offer
 */
void autonomous() {
    // example movement: Move to x: 20 and y:15, and face heading 90. Timeout set to 4000 ms
    chassis.moveToPose(20, 15, 90, 4000);
    //example movement: Move to 0, 0. Timeout set to 15000 ms.
    chassis.moveToPoint(0, 0, 15000);
    // example movement: Turn to face the point x:45, y:-45. Timeout set to 1000
    // dont turn faster than 60 (out of a maximum of 127)
    chassis.turnTo(45, -45, 1000, true, 60);
    // example movement: Follow the path in path.txt. Lookahead at 15, Timeout set to 4000
    // following the path with the back of the robot (forwards = false)
    // see line 116 to see how to define a path
    chassis.follow(example_txt, 15, 4000, false);
    // wait until the chassis has travelled 10 inches. Otherwise the code directly after
    // the movement will run immediately
    // Unless its another movement, in which case it will wait
    chassis.waitUntil(10);
    pros::lcd::print(4, "Travelled 10 inches during pure pursuit!");
    // wait until the movement is done
    chassis.waitUntilDone();
    pros::lcd::print(4, "pure pursuit finished!");
}

/**
 * Runs in driver control
 */
bool wingsvalue = false;
bool blockervalue = false; 
void opcontrol() {
    // controller
    // loop to continuously update motors
    while (true) {
        // get joystick positions
        int leftY = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int rightX = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_Y);
        // move the chassis with tank drive
        chassis.tank(leftY, rightX);
		//toggle wings
		if(controller.get_digital(pros::E_CONTROLLER_DIGITAL_A)){
            wingsvalue = !wingsvalue;
            wings.set_value(wingsvalue);
		}
        //toggle blocker
        if(controller.get_digital(pros::E_CONTROLLER_DIGITAL_B)){
            blockervalue = !blockervalue;
            blocker.set_value(blockervalue);
		}
        //cata move function
        cata.move(127 * controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2));
        // if(controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2)){
        //     cata.move(127);     //if the button is pressed, cata moves
        // }
        // else{
        //     cata.move(127);
        //     if (cata_rot.get_angle() < 50 && cata_rot.get_angle() > 40) {
        //         cata.move(0);   //if button isn't pressed, cata moves until in angle range 50-40.
        //     }
        // }
        //intake spin
        if(!controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1)){
            intake.move(127 * controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2));
        }
        if(!controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2)){
            intake.move(-127 * controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1));
        }
        // delay to save resources
        pros::delay(10);
    }
}