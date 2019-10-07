/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre
Description: Programme pour deplacer le ROBUS pour le defi du parcours.
Date: 5 octobre 2019
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

/* ****************************************************************************
Vos propres fonctions sont creees ici
**************************************************************************** */

///Converts a distance to the according number of pulses.
///distance : The distance in centimeters.
int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

///Function to move ROBUS in a straight line.
///speed : The base speed.
///pulses : The distance to travel in pulses.
void PID(float speed, int pulses)
{
    int totalPulsesLeft = 0, totalPulsesRight = 0, 
        deltaPulsesLeft = 0, deltaPulsesRight = 0,
        errorDelta = 0, errorTotal = 0;
    float newSpeedRight = 0;
    const float kp = 0.005, ki = 0.001;

    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);

    //we could make it accelerate over time
    MOTOR_SetSpeed(LEFT, speed);
    MOTOR_SetSpeed(RIGHT, speed);

    while(abs(totalPulsesRight) < pulses)
    {
        deltaPulsesLeft = ENCODER_ReadReset(LEFT);
        deltaPulsesRight = ENCODER_ReadReset(RIGHT);
        totalPulsesLeft += deltaPulsesLeft;
        totalPulsesRight += deltaPulsesRight;
        errorDelta = deltaPulsesLeft - deltaPulsesRight;
        errorTotal = totalPulsesLeft - totalPulsesRight;
        newSpeedRight = speed + (errorDelta * kp) + (errorTotal * ki);
        MOTOR_SetSpeed(RIGHT, newSpeedRight);
        delay(30);
    }

    //we could make it decelerate over time
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Move ROBUS according to the distance specified.
///distance : The distance to cover in centimeters. Can be negative.
void Move(float distance)
{
    const float baseSpeed = 0.38;
    int distanceSign = distance > 0 ? 1 : -1;
    //distanceSign is used to reverse the speed if the distance is negative
    PID(distanceSign * baseSpeed, DistanceToPulses(distanceSign * distance));
}

///Turn ROBUS according to the angle specified.
///angle : the rotation angle in radians. Positive makes it go left,
///        while negative makes it go right.
void Turn(float angle)
{
    //only one wheel will move
    float radius = 19.33; //in centimeters
    int motor = angle > 0 ? RIGHT : LEFT;
    int totalPulses = 0, deltaPulses = 0;
    float pulsesRotation = DistanceToPulses(abs(radius * angle));
    Serial.println(pulsesRotation);

    ENCODER_Reset(motor);
    MOTOR_SetSpeed(motor, 0.3);
    while(totalPulses < pulsesRotation)
    {
        deltaPulses = ENCODER_ReadReset(motor);
        Serial.println(deltaPulses);
        totalPulses += deltaPulses;
        delay(20); //might not be necessary
    }
    MOTOR_SetSpeed(motor, 0);    
}

//----------------

///Test function to accelerate.
void Run()
{
    //Accelerate over a second
    for(int i = 5; i > 0; i--)
    {
        float speed = 0.5 / i;
        MOTOR_SetSpeed(LEFT, speed);
        MOTOR_SetSpeed(RIGHT, speed);
        delay(250);
    }
    //Set normal speed
    MOTOR_SetSpeed(LEFT, 0.5);
    MOTOR_SetSpeed(RIGHT, 0.5);
}

///Test function to slow down until full stop.
void Stop()
{ 
    //Slows down over a second
    for(int i = 1; i < 5; i++)
    {
        float speed = 0.5 / i;
        MOTOR_SetSpeed(LEFT, speed);
        MOTOR_SetSpeed(RIGHT, speed);
        delay(250);
    }
    //full stop
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

void UTurn()
{
    Turn(-PI/2);
    Move(-19.33); //radius
    Turn(-PI/2);
}

void Parcours()
{
    //Test pour le parcours
    Move(112);
    Turn(PI/2);
    Move(70);
    Turn(-PI/2);
    Move(80);
    Turn(-PI/4);
    Move(170);
    Turn(PI/2);
    Move(45);
    Turn(-PI/4);
    Move(120);

    UTurn();

    Move(120);
    Turn(PI/4);
    Move(45);
    Turn(-PI/2);
    Move(170);
    Turn(PI/4);
    Move(80);
    Turn(PI/2);
    Move(70);
    Turn(-PI/2);
    Move(120);
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
        delay(1000);
        Parcours();
    }
    // SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}