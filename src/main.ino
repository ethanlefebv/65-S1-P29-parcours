/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre
Description: Programme pour deplacer le ROBUS pour le defi du parcours.
Date: 2 octobre 2019
*/

/* ****************************************************************************
Inclure les librairies de functions que vous voulez utiliser
**************************************************************************** */

#include <Arduino.h>
#include <LibRobus.h> // Essentielle pour utiliser RobUS


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

int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

///Description: Move ROBUS according to the distance specified.
///distance: The distance to cover in centimeters.
///<param name='distance'>The distance to cover in centimeters.</param>
void Move(float distance)
{
    PID(0.4 /*base speed*/, DistanceToPulses(distance));
}

void PID(float speed, int pulses)
{
    int totalPulsesLeft = 0, totalPulsesRight = 0, 
        deltaPulsesLeft = 0, deltaPulsesRight = 0,
        errorDelta = 0, errorTotal = 0;
    float newSpeedRight = 0;
    const float kp = 0.0005, ki = 0.0005;

    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);

    //we could make it accelerate over time
    MOTOR_SetSpeed(LEFT, speed);
    MOTOR_SetSpeed(RIGHT, speed);

    while(totalPulsesRight < pulses)
    {
        deltaPulsesLeft = ENCODER_ReadReset(LEFT);
        deltaPulsesRight = ENCODER_ReadReset(RIGHT);
        totalPulsesLeft += deltaPulsesLeft;
        totalPulsesRight += deltaPulsesRight;
        errorDelta = deltaPulsesLeft - deltaPulsesRight;
        errorTotal = totalPulsesLeft - totalPulsesRight;
        newSpeedRight = speed + (errorDelta * kp) + (errorTotal * ki);
        MOTOR_SetSpeed(RIGHT, newSpeedRight);
        delay(80);
    }

    //we could make it decelerate over time
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

//----------------

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

void Parcours()
{
    //Run();
    //delay(1000);
    //Stop();
    Move(100);
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
    Parcours();
}


/* ****************************************************************************
Fonctions de boucle infini (loop())
**************************************************************************** */
// -> Se fait appeler perpetuellement suite au "setup"

void loop() 
{
    // SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}