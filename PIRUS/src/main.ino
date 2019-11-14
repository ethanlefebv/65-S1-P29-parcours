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

enum class Color { Green, Red, Yellow, Blue};

/* ****************************************************************************
Vos propres fonctions sont creees ici
**************************************************************************** */

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

//---------------- TESTS ----------------

void Tests()
{
    
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
}


/* ****************************************************************************
Fonctions de boucle infini (loop())
**************************************************************************** */
// -> Se fait appeler perpetuellement suite au "setup"

void loop()
{
    if(ROBUS_IsBumper(REAR))
    {
        delay(500);
        //do something
    }
    //SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}