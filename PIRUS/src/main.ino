/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre, Vincent-Xavier Voyer, Pierre-Olivier Lajoie, Robin Mailhot
Description: Programme qui gere le ROBUS pour le concours PIRUS.
Date: 20 novembre 2019
*/

//--------------- Libraries ---------------

#include <Arduino.h>
#include <LibRobus.h>
#include <math.h>

//--------------- Function declarations ---------------

int DistanceToPulses(float);

//--------------- Constants ---------------

//----- General -----
const float WHEEL_CIRCUMFERENCE = 23.94;
const float RADIUS_ROTATION = 19.33 / 2; //19.33 is the distance between the wheels
const int PULSES_PER_CYCLE = 3200;
const float BASE_SPEED = 0.4;

//----- Music -----
const float BASE_VOLUME = 1;
const unsigned long MUSIC_FADE_TIME = 10000; //in milliseconds

//----- Movement -----
const int DISTANCE = DistanceToPulses(20); 
const int DISTANCE_ROTATION = DistanceToPulses(RADIUS_ROTATION * PI/2); //the rotations will always be of 90 degrees
//there are 4 movements (in order) : forward, backwards, turn left, turn right
const int MOVEMENTS[4][2] = { {DISTANCE, DISTANCE}, {-DISTANCE, -DISTANCE}, {-DISTANCE_ROTATION, DISTANCE_ROTATION}, {DISTANCE_ROTATION, -DISTANCE_ROTATION}};
const int INI_X = 2;
const int INI_Y = 2;
const int MAX_X = 4;
const int MAX_Y = 4;

//----- Countdown -----
const unsigned long COUNTDOWN_TIME = 10000;

//--------------- Enumerations ---------------

enum class Mode { Sleep, Move, Simon};
enum class Orientation { North, West, South, East};
enum class Move { Forward, Backwards, TurnLeft, TurnRight};
enum class Axis { X, Y};
//Move and Axis are not that necessary, but are used for clarity in some functions

//--------------- Variables ---------------

//----- General -----
Mode currentMode;
bool buttonPressed; //the button to stop the alarm

//----- Music -----
bool musicPlaying;
bool musicFadeComplete;
float musicNewVolume;
unsigned long musicTimeIni;
unsigned long musicTimeCurrent;

//----- Movements -----
int totalPulsesLeft, totalPulsesRight, 
    deltaPulsesLeft, deltaPulsesRight,
    errorDelta, errorTotal;
const float kp = 0.001, ki = 0.001;

Orientation currentOrientation;
int position[2]; //X and Y
int pulsesToTravel[2]; //LEFT and RIGHT
bool moveCompleted;

//----- Countdown -----
unsigned long countdownTimeIni, countdownTimeLeft;
bool countdownOver;

//----- Demo -----
bool demoMusic, demoMovement, demoCountdown;
unsigned long demoTimeIni;



//---------------------------------------------------------------------------
//------------------------------   Functions   ------------------------------
//---------------------------------------------------------------------------

//---------------- General functions ----------------

///Converts a distance to the according number of pulses.
///distance : The distance in centimeters.
int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

///Uses the linetracker to return some random value.
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
    if(first) 
    {
        srand(GetRandomData()); //seeding for the first time only, until the program restarts
        first = false;
    }
    return min + (rand() % (max + 1 - min));
}

//---------------- Music functions ----------------

void RaiseVolume()
{
    musicTimeCurrent = millis() - musicTimeIni;
    musicNewVolume = BASE_VOLUME + (musicTimeCurrent / MUSIC_FADE_TIME * (1 - BASE_VOLUME));
    if(musicNewVolume >= 1.0)
    {
        musicNewVolume = 1;
        musicFadeComplete = true;
    }
    AUDIO_SetVolume(musicNewVolume);
}

void StartMusic()
{
    musicPlaying = true;
    musicTimeIni = millis();
    AUDIO_SetVolume(BASE_VOLUME);
    AUDIO_Next();
}

void StopMusic()
{
    musicPlaying = false;
    AUDIO_Stop();
}

//---------------- Movement functions ----------------

///Updates the position of the ROBUS according to the move done.
///variation : 1 if it moves forward, -1 if it moves backwards
void UpdatePosition(int variation)
{
    switch (currentOrientation)
    {
        case Orientation::North:
            position[(int)Axis::Y] -= variation;
            break;
        case Orientation::West:
            position[(int)Axis::X] -= variation;
            break;
        case Orientation::South:
            position[(int)Axis::Y] += variation;
            break;
        case Orientation::East:
            position[(int)Axis::X] += variation;
            break;
    }
}

///Uses the current position and orientation to determine if a move is invalid.
///Returns the index of the invalid move, or -1 if anything is valid.
int DetermineInvalidMove()
{
    //it can always turn, so only Forward or Backwards can be invalid
    //the ifs (conditions) are not pretty, but are easily understandable
    int invalidMove = -1;
    if( (currentOrientation == Orientation::North && position[(int)Axis::Y] == 0) ||
        (currentOrientation == Orientation::West && position[(int)Axis::X] == 0) ||
        (currentOrientation == Orientation::South && position[(int)Axis::Y] == MAX_Y) ||
        (currentOrientation == Orientation::East && position[(int)Axis::X] == MAX_X))
    {
        invalidMove = 0;//(int)Move::Forward;
    }
    else if((currentOrientation == Orientation::North && position[(int)Axis::Y] == MAX_Y) ||
            (currentOrientation == Orientation::West && position[(int)Axis::X] == MAX_X) ||
            (currentOrientation == Orientation::South && position[(int)Axis::Y] == 0) ||
            (currentOrientation == Orientation::East && position[(int)Axis::X] == 0))
    {
        invalidMove = 1;//(int)Move::Backwards;
    }
    return invalidMove;
}

///Generates a random move for the ROBUS. Makes sure it doesn't go out of the allowed area.
void GenerateRandomMove()
{
    int newMove = -1;
    //check orientation and position, so the next move is chosen accordingly
    int invalidMove = DetermineInvalidMove();

    //pick a random, valid move
    do
    {
        newMove = Random(0,3);
    } 
    while (newMove == invalidMove);
    
    //set the new distance to travel
    pulsesToTravel[LEFT] = MOVEMENTS[newMove][LEFT];
    pulsesToTravel[RIGHT] = MOVEMENTS[newMove][RIGHT];

    //set the new position or orientation
    switch (newMove)
    {
        case 0://(int)Move::Forward:
            UpdatePosition(1);
            break;
        case 1://(int)Move::Backwards:
            UpdatePosition(-1);
            break;
        case 2://(int)Move::TurnLeft:
            currentOrientation = (Orientation)(((int)currentOrientation + 1) % 4);
            break;
        case 3://(int)Move::TurnRight:
            currentOrientation = (Orientation)(((int)currentOrientation - 1 + 4) % 4);
            break;
    }
    Serial.print("X : "); Serial.print(position[0]); Serial.print(" | Y : "); Serial.println(position[1]);

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

//---------------- Countdown functions ----------------

void Countdown()
{
    countdownTimeLeft = COUNTDOWN_TIME - (millis() - countdownTimeIni);
    static int previousPrintedValue = -1;
    if(countdownTimeLeft < 0)
    {
        Serial.println("Countdown over.");
        countdownOver = true;
    }
    else
    {
        int valueToPrint = ceil(countdownTimeLeft / (float)1000); //to print seconds
        if(valueToPrint != previousPrintedValue)
        {
            previousPrintedValue = valueToPrint;
            Serial.println(valueToPrint);
        }
    }
}

float CountdownPO(float timeHours, float timeMin, float timeSec) //time in hours, min, sec
{
	float timeleftSec = 0, timeleftMin = 0, timeleftHours = 24, timeleft = 0;
	//timeSec = (timeMin*60) + (timeHours*60*60) + (timeSec); 
    for (int i = 0; i<24 && timeleftHours >0; i++)
    {
        timeleftHours = timeHours - i;
        //int j = 0;
        timeleftMin = 60;
        for (int j = 0; j <60 && timeleftMin > 0; j ++)
        {
            if (i==0)
            {
                timeleftMin = timeMin - j;
            }
            else(timeleftMin = 59 - j);
            int timeleftSec = 60;
            int k = 0;
            for (k = 0; k<60 && timeleftSec > 0; k++)
            {
                if(timeleftMin==timeMin)
                {
                    timeleftSec = timeSec - k;
                }
                else(timeleftSec = 59 - k);
                delay (1000);
                Serial.print("time left :");
                Serial.print(timeleftHours);
                Serial.print(" : ");
                Serial.print(timeleftMin);
                Serial.print("\t");
                Serial.print(timeleftSec);
                Serial.print("\t");
                Serial.print("\n");
            }
        }
    }
    /*for (int i = 0; i<=timeSec; i++)
    {
        timeleft = timeSec - i;
        delay(1000);
        Serial.print("time left :");
        Serial.print(timeleft);
        Serial.print("\n");
    }*/
    Serial.print("GO");
    return timeleftSec;
}

//---------------- Demos ----------------

void LoopMusicDemo()
{
    if(!musicPlaying)
    {
        StartMusic();
    }
    else if(!musicFadeComplete)
    {
        RaiseVolume();
    }
    else if((millis() - musicTimeIni) > 20000)
    {
        StopMusic();
        demoMusic = false;
    }
}

void LoopMovementDemo()
{
    if((millis() - demoTimeIni) < 10000)
    {
        if(moveCompleted)
        {
            GenerateRandomMove();
        }
        else
        {
            Move();
        }
    }
    else
    {
        demoTimeIni = 0;
        demoMovement = false;
    }
}

void LoopCountdownDemo()
{
    if(!countdownOver)
    {
        Countdown();
    }
    else
    {
        demoCountdown = false;
    }
}

//---------------- Init and loop functions ----------------

void setup()
{
    BoardInit();
    AudioInit();

    //Variables initiation

    //--- Demo ---
    demoMusic = false;
    demoMovement = false;
    demoCountdown = false;
    demoTimeIni = 0;

    //--- General ---
    currentMode = Mode::Sleep;

    //--- Music ---
    musicPlaying = false;
    musicFadeComplete = false;
    musicNewVolume = 0;
    musicTimeIni = 0;
    musicTimeCurrent = 0;

    //--- Movement ---
    totalPulsesLeft = 0;
    totalPulsesRight = 0;
    deltaPulsesLeft = 0;
    deltaPulsesRight = 0;
    errorDelta = 0;
    errorTotal = 0;

    currentOrientation = Orientation::North;
    position[(int)Axis::X] = INI_X;
    position[(int)Axis::Y] = INI_Y;
    pulsesToTravel[LEFT] = 0;
    pulsesToTravel[RIGHT] = 0;
    moveCompleted = true;

    //--- Countdown ---
    countdownTimeIni = 0;
    countdownOver = true;


    //this is only for testing
    //delay(5000);
}

void loop()
{
    if(!demoMusic && !demoMovement && !demoCountdown)
    {
        demoMusic = ROBUS_IsBumper(FRONT);
        demoMovement = ROBUS_IsBumper(LEFT);
        demoCountdown = ROBUS_IsBumper(REAR);
    }
    else if(demoMusic)
    {
        LoopMusicDemo();
    }
    else if(demoMovement)
    {
        if(demoTimeIni == 0)
            demoTimeIni = millis();
        LoopMovementDemo();
    }
    else if(demoCountdown)
    {
        CountdownPO(0,0,10);
        demoCountdown = false;

        /*
        if(countdownTimeIni == 0)
        {
            countdownTimeIni = millis();
            countdownOver = false;
        }
        LoopCountdownDemo();
        */
    }
    
    //SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}