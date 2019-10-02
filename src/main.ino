/*
Projet: Le nom du script
Equipe: Votre numero d'equipe
Auteurs: Les membres auteurs du script
Description: Breve description du script
Date: Derniere date de modification
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

void Move(float distance)
{
  int pulses = DistanceToPulses(distance);
}

void PID(float speed, int pulses)
{
  int totalPulsesLeft = 0, totalPulsesRight = 0,
    deltaPulsesLeft = 0, deltaPulsesRight = 0;
  ENCODER_Reset(LEFT);
  ENCODER_Reset(RIGHT);
  MOTOR_SetSpeed(LEFT, speed);
  MOTOR_SetSpeed(RIGHT, speed);
  while(totalPulsesRight < pulses)
  {
    deltaPulsesLeft = ENCODER_ReadReset(LEFT);
    deltaPulsesRight = ENCODER_ReadReset(RIGHT);
  }
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
  Run();
  delay(1000);
  Stop();
}


/* ****************************************************************************
Fonctions d'initialisation (setup)
**************************************************************************** */
// -> Se fait appeler au debut du programme
// -> Se fait appeler seulement un fois
// -> Generalement on y initilise les varibbles globales

void setup(){
  BoardInit();
  Parcours();
}


/* ****************************************************************************
Fonctions de boucle infini (loop())
**************************************************************************** */
// -> Se fait appeler perpetuellement suite au "setup"

void loop() {
  // SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
  delay(10);// Delais pour décharger le CPU
}