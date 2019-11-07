/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre
Description: Programme pour deplacer le ROBUS pour le defi du combattant.
Date: 04 novembre 2019
*/

/* ****************************************************************************
Inclure les librairies de functions que vous voulez utiliser
**************************************************************************** */

#include <Arduino.h>
#include <LibRobus.h> // Essentielle pour utiliser RobUS
#include <math.h>


/* ****************************************************************************
Variables globales et defines
**************************************************************************** */
// -> defines...
// L'ensemble des fonctions y ont acces
const float WHEEL_CIRCUMFERENCE = 23.94;
const int PULSES_PER_CYCLE = 3200;
const float BASE_SPEED = 0.8;
const unsigned long TIME_TO_STOP_AT = 20000; //in ms

unsigned long timeAtStart;


enum class Color { Green, Red, Yellow, Blue};

/* ****************************************************************************
Vos propres fonctions sont creees ici
**************************************************************************** */

///A timer that will stop everything after the time set in the constant TIME_TO_STOP_AT.
void TimeBomb()
{
    Serial.println("Just called Timebomb().");
    if((millis() - timeAtStart) > TIME_TO_STOP_AT)
    {
        Serial.println("Starting infinite loop.");
        while(true){} //yeah that's done on purpose
        //delay(500000); //arbitrary number
    }
}

///Converts a distance to the according number of pulses.
///distance : The distance in centimeters.
int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

///Returns true if at least one sensor has some black under it.
bool IsOnALine()
{
    bool isOnALine = false;
    for(int i = A0; i <= A7 && !isOnALine; i++)
    {
        isOnALine = analogRead(i) > 600;
    }
    return isOnALine;
}

void LineTrackerCalculateSpeed(float baseSpeed, float* speedLeft, float* speedRight, float kLine)
{
    float avgLeft = 0, avgRight = 0,
          avgLeftProp = 0, avgRightProp = 0, //proportional, the errors on the extremes are amplified
          avgErrorLeft = 0, avgErrorRight = 0;

    avgLeftProp = ((4 * analogRead(A7)) + (2 * analogRead(A6)) + (1 * analogRead(A5))) / 3;
    avgRightProp = ((1 * analogRead(A2)) + (2 * analogRead(A1)) + (4 * analogRead(A0))) / 3;
    avgLeft = (analogRead(A7) + analogRead(A6) + analogRead(A5)) / 3;
    avgRight = (analogRead(A2) + analogRead(A1) + analogRead(A0)) / 3;
    avgErrorLeft = (avgRightProp - avgLeftProp) / (avgLeft + avgRight);
    avgErrorRight = (avgLeftProp - avgRightProp) / (avgLeft + avgRight);

    *speedLeft = baseSpeed + (kLine * avgErrorLeft);
    *speedRight = baseSpeed + (kLine * avgErrorRight);
}

///Function to move ROBUS in a straight line.
///speed : The base speed.
///pulses : The distance to travel in pulses. Should always be positive.
///useLineTracker : Enables/disables the line tracking feature.
void PID(float speed, int pulses)
{
    int totalPulsesLeft = 0, totalPulsesRight = 0, 
        deltaPulsesLeft = 0, deltaPulsesRight = 0,
        errorDelta = 0, errorTotal = 0;

    const float kp = 0.001, ki = 0.001;
    
    //the number of pulses ROBUS will accelerate/decelerate
    float pulsesAcceleration = 1/10.0 * pulses;

    float masterSpeed = 0, slaveSpeed = 0, slowSpeed = 0.2;
    int speedSign = speed > 0 ? 1 : -1;
    speed *= speedSign; //we'll convert it back to the negative value (if that's the case) at the end
    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);

    //we give it a slow speed to start
    MOTOR_SetSpeed(LEFT, slowSpeed);
    MOTOR_SetSpeed(RIGHT, slowSpeed);

    while(totalPulsesRight < pulses)
    {
        deltaPulsesLeft = abs(ENCODER_ReadReset(LEFT));
        deltaPulsesRight = abs(ENCODER_ReadReset(RIGHT));
        totalPulsesLeft += deltaPulsesLeft;
        totalPulsesRight += deltaPulsesRight;

        //change the 2 following lines if we change the master
        errorDelta = deltaPulsesRight - deltaPulsesLeft;
        errorTotal = totalPulsesRight - totalPulsesLeft;

        if(totalPulsesRight < pulsesAcceleration)
        {
            masterSpeed = slowSpeed + (totalPulsesRight / pulsesAcceleration * (speed - slowSpeed));
        }
        else if(totalPulsesRight > pulses - pulsesAcceleration)
        {
            masterSpeed = slowSpeed + ((pulses - totalPulsesRight) / pulsesAcceleration * (speed - slowSpeed));
        }
        else //not accelerating nor decelerating
        {
            masterSpeed = speed;
        }
        slaveSpeed = masterSpeed + (errorDelta * kp) + (errorTotal * ki);
        MOTOR_SetSpeed(LEFT, speedSign * slaveSpeed);
        MOTOR_SetSpeed(RIGHT, speedSign * masterSpeed);
        delay(40);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Function to make ROBUS follow a line.
///speed : The base speed, needs to be positive.
///pulses : The distance to travel in pulses. Should always be positive.
void LineTracker(float speed, int pulses)
{
    int avgPulses = 0;
    const float kLine = 0.4;
    float speedLeft = 0, speedRight = 0;
    speed = fabs(speed); //just in case the provided speed is negative

    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);
    MOTOR_SetSpeed(LEFT, speed);
    MOTOR_SetSpeed(RIGHT, speed);

    while(avgPulses < pulses)
    {
        //this is not very accurate but still works a bit
        avgPulses += (abs(ENCODER_ReadReset(LEFT)) + abs(ENCODER_ReadReset(RIGHT))) / 2;

        LineTrackerCalculateSpeed(speed, &speedLeft, &speedRight, kLine);

        MOTOR_SetSpeed(LEFT, speedLeft);
        MOTOR_SetSpeed(RIGHT, speedRight);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Function to make ROBUS follow a line.
///speed : The base speed, needs to be positive.
void LineTracker(float speed)
{
    const float kLine = 0.4;
    speed = fabs(speed); //just in case the provided speed is negative
    float speedLeft = speed, speedRight = speed;

    MOTOR_SetSpeed(LEFT, speedLeft);
    MOTOR_SetSpeed(RIGHT, speedRight);

    while(IsOnALine())
    {
        LineTrackerCalculateSpeed(speed, &speedLeft, &speedRight, kLine);

        MOTOR_SetSpeed(LEFT, speedLeft);
        MOTOR_SetSpeed(RIGHT, speedRight);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Move ROBUS according to the distance specified.
///distance : The distance to cover in centimeters. Can be negative.
///speed : The base speed. Must be between 0.15 and 1 for the ROBUS to move correctly.
void Move(float distance, float speed)
{
    int distanceSign = distance > 0 ? 1 : -1;
    //distanceSign is used to reverse the speed if the distance is negative
    PID(distanceSign * speed, DistanceToPulses(fabs(distance)));
}

///Make ROBUS follow a line for a certain distance.
///speed : The base speed. Must be between 0.15 and 1 for the ROBUS to move correctly.
///distance : The distance to cover in centimeters. Must be positive for now.
void FollowLine(float speed, float distance)
{
    int distanceSign = distance > 0 ? 1 : -1;
    //distanceSign is used to reverse the speed if the distance is negative
    LineTracker(/*distanceSign * */speed, DistanceToPulses(fabs(distance)));
}

///Make ROBUS follow a line until it leaves the line.
///speed : The base speed. Must be between 0.15 and 1 for the ROBUS to move correctly.
void FollowLine(float speed)
{
    LineTracker(speed);
}

///Turn ROBUS according to the angle specified.
///angle : the rotation angle in radians. Positive makes it go left,
///        while negative makes it go right.
void Turn(float angle)
{
    //both wheels will move
    int totalPulsesLeft = 0, totalPulsesRight = 0,
        deltaPulsesLeft = 0, deltaPulsesRight = 0,
        errorDelta = 0, errorTotal = 0;
    
    float radius = 19.33 / 2; //in centimeters
    float baseSpeed = 0.2, correctedSpeed = 0;
    const float kp = 0.0001, ki = 0.0005;

    int motorLeftSign = angle > 0 ? -1 : 1;
    float pulsesRotation = DistanceToPulses(fabs(radius * angle));
    
    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);
    MOTOR_SetSpeed(LEFT, motorLeftSign * baseSpeed);
    MOTOR_SetSpeed(RIGHT, -1 * motorLeftSign * baseSpeed);
    while(totalPulsesLeft < pulsesRotation)
    {
        //deltaPulsesLeft = abs(ENCODER_ReadReset(LEFT));
        //deltaPulsesRight = abs(ENCODER_ReadReset(RIGHT));
        totalPulsesLeft = abs(ENCODER_Read(LEFT)); //+= deltaPulsesLeft;
        totalPulsesRight = abs(ENCODER_Read(RIGHT)); //+= deltaPulsesRight;

        //errorDelta = deltaPulsesRight - deltaPulsesLeft;
        errorTotal = totalPulsesRight - totalPulsesLeft;

        correctedSpeed = baseSpeed /*+ (errorDelta * kp)*/ + (errorTotal * ki);

        MOTOR_SetSpeed(LEFT, motorLeftSign * correctedSpeed);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

void Grip()
{
    delay(500);
    SERVO_SetAngle(0, 82);
    delay(500);
}

void Loosen()
{
    SERVO_SetAngle(0, 100);
}

void Release()
{
    delay(500);
    SERVO_SetAngle(0, 135);
    delay(500);
}

void MoveToLine(void)
{
    while(((analogRead(A7) + (analogRead(A0)) < 1000)))
    {
    MOTOR_SetSpeed(LEFT , 0.15);
    MOTOR_SetSpeed(RIGHT , 0.15);
    }
    MOTOR_SetSpeed(LEFT , 0);
    MOTOR_SetSpeed(RIGHT , 0);
    delay(250);
    while(analogRead(A0) < analogRead(A7))
    {
    MOTOR_SetSpeed(RIGHT , 0.1);
     }
     while(analogRead(A7) < analogRead(A0))
    {
    MOTOR_SetSpeed(LEFT , 0.1);
     }
    delay(250);
    MOTOR_SetSpeed(LEFT , 0);
    MOTOR_SetSpeed(RIGHT , 0);
}

void MoveToPosition(Color colorZoneB)
{
    if(colorZoneB == Color::Green) 
    {
        Move(-20, 0.4);
        Turn(-PI/4);
        delay(200);
        Move(76 ,0.6);
        delay(200);
        Turn(PI/2);
        delay(300);
        MoveToLine();
        delay(50);
        Move(3,0.1);
        delay(150);
        Turn(PI/2);
        delay(300);
        SERVO_SetAngle(0, 100);
        FollowLine(0.55);
        Move(35, 0.5);
    }
    else if(colorZoneB == Color::Red) 
    {
        Move(-20, 0.4);
        Turn(PI/4);
        delay(200);
        Move(76 ,0.6);
        delay(300);
        Turn(-PI/2);
        delay(200);
        MoveToLine();
        delay(50);
        Move(3,0.1);
        delay(150);
        Turn(-PI/2);
        delay(300);
        SERVO_SetAngle(0, 100);
        FollowLine(0.55);
        Move(35, 0.5);
    }
    else if(colorZoneB == Color::Blue) 
    {
        Move(-30, 0.4);
        Turn(-PI/4);
        MoveToLine();
        delay(150);
        Move(3,0.1);
        delay(50);
        Turn(PI/2);
        delay(300);
        SERVO_SetAngle(0, 100);
        FollowLine(0.55);
        Move(35, 0.5);
    }
    else if(colorZoneB == Color::Yellow) 
    {
        Move(-30, 0.4);
        Turn(PI/4);
        MoveToLine();
        delay(150);
        Move(3,0.1);
        delay(50);
        Turn(-PI/2);
        delay(300);
        SERVO_SetAngle(0, 90);
        FollowLine(0.55);
        Move(35, 0.5);
    }
}
///Main program for the combattant challenge, robot A.
///colorZoneA : The color of the zone the robot has to pick up the ball.
///colorZoneB : The color of the zone robot B has to push the ball to.
void CombattantA(Color colorZoneA)//, Color colorZoneB)
{
    //start a timer so that the robot stops everything after a minute
    /* turns out it doesn't do shit yet
    timeAtStart = millis();
    SOFT_TIMER_Enable(0);
    */

    //move to the middle of the arena
    Move(39, 0.2); //will probably need to change the distance

    //rotate to face the right color
    float angle = 0;
    switch (colorZoneA)
    {
        case Color::Green:
            angle = -PI/4;
            break;
        case Color::Red:
            angle = PI/4;
            break;
        case Color::Yellow:
            angle = 3*PI/4;
            break;
        case Color::Blue:
            angle = -3*PI/4;
            break;
    }
    Turn(angle);

    //follow the line until the zone is reached
    FollowLine(0.3);

    //move forward a bit
    Move(28, 0.3);
    //Red :
    //Green : 28cm
    //Yellow :
    //Blue :  28cm

    //grip the ball using the motorized arm
    Grip();
    delay(500);
    Move(-5.5,0.3);
    Release();
    delay(400);
    Move(4.5,0.09);
    delay(250);
    Grip();
    //180 turn
    Move(-10, 0.4);
    delay(250);
    Turn(PI);

    //maybe go forward a bit, then follow line until reached the center
    Move(10, 0.4); //ROBUS should now be back on the line
    Loosen();
    FollowLine(0.4, 70);
    delay(2000);
    //let go of the ball
    Release();

    //move out of the way
    /*if(colorZoneA != colorZoneB) //it'll back up to the zone where it picked up the ball
    {
        Move(-95, 0.4);
    }
    else
    {
        Move(-60, 0.4);
        Turn(PI/2);
        Move(55, 0.4); //it goes to a near wall
    }*/
    //Robot A's purpose is now complete
    Move(-90, 0.5);
}

///Main program for the combattant challenge, robot B.
///colorZone : The color of the zone the robot has to drop/push the ball.
void CombattantB(Color colorZoneB)
{   
    //wait a minute before starting
    //delay(60000);
    SERVO_SetAngle(0, 180);
    MoveToPosition(colorZoneB);
}

//---------------- TESTS ----------------

void TestLineTrackerValues()
{
    //do some tests
    Serial.print("values : ");
    Serial.print(analogRead(A0)); Serial.print("\t");
    Serial.print(analogRead(A1)); Serial.print("\t");
    Serial.print(analogRead(A2)); Serial.print("\t");
    Serial.print(analogRead(A3)); Serial.print("\t");
    Serial.print(analogRead(A4)); Serial.print("\t");
    Serial.print(analogRead(A5)); Serial.print("\t");
    Serial.print(analogRead(A6)); Serial.print("\t");
    Serial.print(analogRead(A7)); Serial.print("\n");
    Serial.println();
}

void Tests()
{
    //timeAtStart = millis();
    //SOFT_TIMER_Enable(0);
    
    //move forward a bit
    Move(33, 0.6);

    //grip the ball using the motorized arm
    Grip();

    //180 turn
    Move(-10, 0.4);
    delay(250);
    Turn(PI);

    //maybe go forward a bit, then follow line until reached the center
    Move(10, 0.4); //ROBUS should now be back on the line
    Loosen();
}

/* ****************************************************************************
Fonctions d'initialisation (setup)
**************************************************************************** */
// -> Se fait appeler au debut du programme
// -> Se fait appeler seulement un fois
// -> Generalement on y initilise les varibbles globales

void setup()
{
    BoardInit();
    timeAtStart = 0;
    SOFT_TIMER_SetCallback(0, &TimeBomb);
    SOFT_TIMER_SetDelay(0, 500);

    SERVO_Enable(0);
    Release();
}


/* ****************************************************************************
Fonctions de boucle infini (loop())
**************************************************************************** */
// -> Se fait appeler perpetuellement suite au "setup"

void loop()
{
    //SERVO_SetAngle(1,0);

    if(ROBUS_IsBumper(REAR))
    {
        delay(500);
        //Tests();
        
        //CombattantA(Color::Red); //uncomment either A or B, and change the colors
        CombattantB(Color::Yellow);
    }
    SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}