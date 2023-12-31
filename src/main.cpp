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
// pros::Rotation horizontalEnc(4);
// // horizontal tracking wheel. 2.75" diameter, 3.7" offset, back of the robot
// lemlib::TrackingWheel horizontal(&horizontalEnc, lemlib::Omniwheel::NEW_275, -3.7);

// drivetrain settings
lemlib::Drivetrain drivetrain(
	&leftMotors, // left motor group
    &rightMotors, // right motor group
    12, // 12 inch track width (CHANGE WITH NEW MEASUREMENTS)
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
    nullptr, // horizontal tracking wheel 1
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
    chassis.setPose(0, 0, 0); //set the pose to origin
    pros::lcd::initialize(); // initialize brain screen
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
void competition_initialize() {
    chassis.calibrate(); // calibrate sensors
    chassis.setPose(0, 0, 0); //set the pose to origin
}

// get a path used for pure pursuit
// this needs to be put outside a function
// '.' replaced with "_" to make c++ happy
ASSET(pathUnderHang_txt);   //path for curve under goal. After 35in,
                            //drop off triball.
ASSET(pathCurveGoal_txt);   //path that curves 

/**
 * Runs during auto
 *
 * This is an example autonomous routine which demonstrates a lot of the features LemLib has to offer
 */
void autonomous() {
    chassis.setPose(33,-53, 0); //set the pose to origin

    wings.set_value(true);
    chassis.moveToPose(11, -4, 309, 1000);
    chassis.waitUntil(1);
    wings.set_value(false);
    intake.move(127);
    // total time: 1000

    chassis.moveToPose(41, -4, 90, 800);
    chassis.waitUntil(2);
    wings.set_value(true);
    chassis.waitUntil(4);
    intake.move(-127);
    // total time: 1800
    
    chassis.moveToPoint(20, -4, 600, false);
    wings.set_value(false);
    // total time: 2400

    chassis.moveToPose(11, -20, 240, 700);
    intake.move(127);
    // total time: 3100

    chassis.follow(pathUnderHang_txt, 15, 3500);
    chassis.waitUntil(35);
    intake.move(-127);
    chassis.waitUntil(40);
    intake.move(127);
    // total time: 6600

    chassis.moveToPoint(30, -58, 300, false);
    // total time: 6900

    chassis.turnTo(40, -58, 600);
    // total time: 7500

    chassis.follow(pathCurveGoal_txt, 10, 3000);
    // total time: 10500

    chassis.follow(pathCurveGoal_txt, 10, 3000, false);
    // total time: 13500

    chassis.moveToPoint(8, -58, 300, false);
    // total time: 13800

    // total excess time: 15000 - 13800 = 1200 msec. Distribute accordingly to testing.




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
        if(abs(leftY)<15){
            leftY= 0;
        }
        if(abs(rightX)<15){
            rightX= 0;
        }

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
        // cata.move(127 * controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2));
        if(controller.get_digital(pros::E_CONTROLLER_DIGITAL_R2)){
            cata.move(127);     //if the button is pressed, cata moves
        }
        else{
            cata.move(127);
            if (cata_rot.get_angle() > 55 && cata_rot.get_angle() < 350) {
                cata.move(0);   //if button isn't pressed, cata moves until out of angle range
            }
        }

        //intake spin
        if(!controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1)){
            intake.move(127 * controller.get_digital(pros::E_CONTROLLER_DIGITAL_L1));
        }
        if(!controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2)){
            intake.move(-127 * controller.get_digital(pros::E_CONTROLLER_DIGITAL_L2));
        }
        // delay to save resources
        pros::delay(10);
    }
}