/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre
Description: Programme qui gere le ROBUS pour le concours PIRUS.
Date: 18 novembre 2019
*/

//--------------- Libraries ---------------

#include <Arduino.h>
#include <LibRobus.h> // Essentielle pour utiliser RobUS
#include <math.h>

//--------------- Function declarations ---------------

int DistanceToPulses(float);

//--------------- Constants ---------------

const float WHEEL_CIRCUMFERENCE = 23.94;
const float RADIUS_ROTATION = 19.33 / 2; //19.33 is the distance between the wheels
const int PULSES_PER_CYCLE = 3200;
const float BASE_SPEED = 0.3;

//normal distance for a move is specified in centimeters as the parameter
const int DISTANCE = DistanceToPulses(20); 
const int DISTANCE_ROTATION = DistanceToPulses(RADIUS_ROTATION * PI/2); //the rotations will always be of 90 degrees
//there are 4 movements (in order) : forward, backwards, turn left, turn right
const int MOVEMENTS[4][2] = { {DISTANCE, DISTANCE}, {-DISTANCE, -DISTANCE}, {-DISTANCE_ROTATION, DISTANCE_ROTATION}, {DISTANCE_ROTATION, -DISTANCE_ROTATION}};

//--------------- Enumerations ---------------

enum class Mode { Sleep, Move, Simon};
enum class Orientation { North, West, South, East};
enum class Axis { X, Y};

//--------------- Variables ---------------

//----- General -----
unsigned long time;
Mode currentMode;
bool buttonPressed; //the button to stop the alarm

//----- Movements -----
int totalPulsesLeft, totalPulsesRight, 
    deltaPulsesLeft, deltaPulsesRight,
    errorDelta, errorTotal;
const float kp = 0.001, ki = 0.001;

Orientation currentOrientation;
int position[2]; //X and Y
int pulsesToTravel[2]; //LEFT and RIGHT
bool moveCompleted;



//---------------------------------------------------------------------------
//------------------------------   Functions   ------------------------------
//---------------------------------------------------------------------------

///Converts a distance to the according number of pulses.
///distance : The distance in centimeters.
int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

int GetRandomData()
{
    return abs(analogRead(A1) + analogRead(A2) / analogRead(A3) - analogRead(A4) * analogRead(A5) + analogRead(A6) * analogRead(A7) + analogRead(A8)); 
}

///Returns a random number in the range provided.
///min : the included minimum value.
///max : the included maximum value.
int Random(int min, int max)
{
    static bool first = true;
    if (first) 
    {  
        srand(GetRandomData()); //seeding for the first time only, until the program restarts
        first = false;
    }
    return min + (rand() % (max + 1 - min));
}

//---------------- Movement functions ----------------

///Updates the position of the ROBUS according to the move done.
///variation : 1 if it moves forward, -1 if it moves backwards
void UpdatePosition(int variation)
{
    switch (currentOrientation)
    {
        case Orientation::North:
            position[(int)Axis::Y] += variation;
            break;
        case Orientation::West:
            position[(int)Axis::X] -= variation;
            break;
        case Orientation::South:
            position[(int)Axis::Y] -= variation;
            break;
        case Orientation::East:
            position[(int)Axis::X] += variation;
            break;
    }
}

///Generates a random move for the ROBUS. Makes sure it doesn't go out of the allowed area.
void GenerateRandomMove()
{
    //check orientation and position, so the next move is chosen accordingly
    
    //pick a random move
    int newMove = Random(0,3);
    
    //set the new distance to travel
    pulsesToTravel[LEFT] = MOVEMENTS[newMove][LEFT];
    pulsesToTravel[RIGHT] = MOVEMENTS[newMove][RIGHT];

    //set the new position or orientation
    switch (newMove)
    {
        case 0: //forward
            UpdatePosition(1);
            break;
        case 1: //backwards
            UpdatePosition(-1);
            break;
        case 2: //turns left
            currentOrientation = (Orientation)(((int)currentOrientation + 1) % 4);
            break;
        case 3: //turns right
            currentOrientation = (Orientation)(((int)currentOrientation - 1 + 4) % 4);
            break;
    }

    //make sure the next loop starts the move
    moveCompleted = false;
}

///Moves the ROBUS according to the values in pulsesToTravel.
void Move()
{
    float correctedSpeed = 0;
    int speedSignLeft = pulsesToTravel[LEFT] / abs(pulsesToTravel[LEFT]);
    int speedSignRight = pulsesToTravel[RIGHT] / abs(pulsesToTravel[RIGHT]);

    if(totalPulsesLeft > abs(pulsesToTravel[LEFT]))
    {
        //the current movement is complete, so we reset the values
        MOTOR_SetSpeed(LEFT, 0);
        MOTOR_SetSpeed(RIGHT, 0);
        totalPulsesLeft = 0;
        totalPulsesRight = 0;
        moveCompleted = true;
    }
    else if(totalPulsesLeft == 0)
    {
        //it starts the movement, so we initialize correctly some stuff
        MOTOR_SetSpeed(LEFT, speedSignLeft * BASE_SPEED);
        MOTOR_SetSpeed(RIGHT, speedSignRight * BASE_SPEED);
        totalPulsesLeft = 1;
        totalPulsesRight = 1;
    }
    else
    {
        //it's in the middle of a movement
        deltaPulsesLeft = abs(ENCODER_ReadReset(LEFT));
        deltaPulsesRight = abs(ENCODER_ReadReset(RIGHT));
        totalPulsesLeft += deltaPulsesLeft;
        totalPulsesRight += deltaPulsesRight;

        //the right motor is the master
        errorDelta = deltaPulsesRight - deltaPulsesLeft;
        errorTotal = totalPulsesRight - totalPulsesLeft;

        correctedSpeed = BASE_SPEED + (errorDelta * kp) + (errorTotal * ki);
        MOTOR_SetSpeed(LEFT, speedSignLeft * correctedSpeed);
    }
}

//---------------- Tests ----------------

void Tests()
{
    for (int i = 0; i < 20; i++)
    {
        Serial.println(Random(0,3));
    }    
}

//---------------- Init and loop functions ----------------

void setup()
{
    BoardInit();

    //Variables initiation

    currentMode = Mode::Sleep;

    totalPulsesLeft = 0;
    totalPulsesRight = 0;
    deltaPulsesLeft = 0;
    deltaPulsesRight = 0;
    errorDelta = 0;
    errorTotal = 0;

    currentOrientation = Orientation::North;
    position[(int)Axis::X] = 7;
    position[(int)Axis::Y] = 7;
    pulsesToTravel[LEFT] = 0;
    pulsesToTravel[RIGHT] = 0;
    moveCompleted = true;

    //this is only for testing
    delay(5000);
}

void loop()
{
    //main program goes here

    /*
    if(ROBUS_IsBumper(REAR))
    {
        Tests();
        delay(1000);
    }*/
    
    if(moveCompleted)
    {
        GenerateRandomMove();
    }
    else
    {
        Move();
    }
    
    //SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}