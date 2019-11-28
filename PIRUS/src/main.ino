/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre, Vincent-Xavier Voyer, Pierre-Olivier Lajoie, Robin Mailhot
Description: Programme qui gere le ROBUS pour le concours PIRUS.
Date: 28 novembre 2019
*/

//--------------- Libraries ---------------

#include <Arduino.h>
#include <LibRobus.h>
#include <math.h>
#include <LiquidCrystal.h>

//--------------- Defines ---------------

#define HOUR 0
#define MIN 1
#define SEC 2
#define X 0
#define Y 1
#define FORWARD 0
#define BACKWARDS 1
#define TURN_LEFT 2
#define TURN_RIGHT 3

//--------------- Function declarations ---------------

int DistanceToPulses(float);

//--------------- Enumerations ---------------

enum class Mode { Sleep, Alarm, Simon};
enum class Orientation { North, West, South, East};

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
const int INI_X = 1;
const int INI_Y = 1;
const int MAX_X = 2;
const int MAX_Y = 2;

//----- Clock -----
const unsigned long COUNTDOWN_TIME = 10000;
const int TIME_DEFAULT_HOUR = 6;
const int TIME_DEFAULT_MIN = 59;
const int TIME_DEFAULT_SEC = 30;
const int TIME_ALARM_HOUR = 7;
const int TIME_ALARM_MIN = 0;
const int TIME_ALARM_SEC = 0;

//----- Bells -----
const int TIME_START_BELLS = 15000;

//--------------- Variables ---------------

//----- General -----
Mode currentMode;
bool buttonPressed; //the button to stop the alarm
bool firstTimeInLoop;
unsigned long timeIni;

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

//----- Clock -----
int timeCurrent[3];
unsigned long timePrevious;
bool countdownOver;

LiquidCrystal lcd(34,2,3,36,4,38);



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

///Checks if the user has showed a sign of life.
void CheckForInteraction()
{
    //if(something)
    //{
    //    currentMode = Mode::Simon;
    //    StopMovement();
    //    //stop the bells if they are currently activated?
    //}
}

//---------------- Music functions ----------------

///Raises the volume of the music over time.
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

///Starts playing music at the indicated volume.
void StartMusic(float volume)
{
    musicPlaying = true;
    musicTimeIni = millis();
    AUDIO_SetVolume(volume);
    AUDIO_Next();
}

///Starts the music at a low volume - meant to be called if the volume will raise over time.
void StartMusic()
{
    musicFadeComplete = false;
    StartMusic(BASE_VOLUME);
}

///Stops music.
void StopMusic()
{
    musicPlaying = false;
    AUDIO_Stop();
}

void PlayMusic()
{
    if(!musicPlaying)
    {
        StartMusic();
    }
    else if(!musicFadeComplete)
    {
        RaiseVolume();
    }
}

//---------------- Movement functions ----------------

///Updates the position of the ROBUS according to the move done.
///variation : 1 if it moves forward, -1 if it moves backwards
void UpdatePosition(int variation)
{
    switch (currentOrientation)
    {
        case Orientation::North:
            position[Y] -= variation;
            break;
        case Orientation::West:
            position[X] -= variation;
            break;
        case Orientation::South:
            position[Y] += variation;
            break;
        case Orientation::East:
            position[X] += variation;
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
    if( (currentOrientation == Orientation::North && position[Y] == 0) ||
        (currentOrientation == Orientation::West && position[X] == 0) ||
        (currentOrientation == Orientation::South && position[Y] == MAX_Y) ||
        (currentOrientation == Orientation::East && position[X] == MAX_X))
    {
        invalidMove = FORWARD;
    }
    else if((currentOrientation == Orientation::North && position[Y] == MAX_Y) ||
            (currentOrientation == Orientation::West && position[X] == MAX_X) ||
            (currentOrientation == Orientation::South && position[Y] == 0) ||
            (currentOrientation == Orientation::East && position[X] == 0))
    {
        invalidMove = BACKWARDS;
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
        case FORWARD:
            UpdatePosition(1);
            break;
        case BACKWARDS:
            UpdatePosition(-1);
            break;
        case TURN_LEFT:
            currentOrientation = (Orientation)(((int)currentOrientation + 1) % 4);
            break;
        case TURN_RIGHT:
            currentOrientation = (Orientation)(((int)currentOrientation - 1 + 4) % 4);
            break;
    }
    //Serial.print("X : "); Serial.print(position[0]); Serial.print(" | Y : "); Serial.println(position[1]);

    //make sure the next loop starts the move
    moveCompleted = false;
}

///Moves the ROBUS according to the values in pulsesToTravel.
void MoveUpdate()
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

void StopMovement()
{
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

void Move()
{
    if(moveCompleted)
    {
        GenerateRandomMove();
    }
    else
    {
        MoveUpdate();
    }
}

//---------------- Clock functions ----------------

///Updates the false clock of the ROBUS. Must be run in the loop() function at all times.
void Clock()
{
    int sec = (millis() - timePrevious) / 1000;
    if(sec == 1)
    {
        timePrevious = millis();
        ++timeCurrent[SEC];
    }
    if(timeCurrent[SEC] >= 60)
    {
        timeCurrent[SEC] = 0;
        ++timeCurrent[MIN];
    }
    if(timeCurrent[MIN] >= 60)
    {
        timeCurrent[MIN] = 0;
        ++timeCurrent[HOUR];
    }
}

///Prints the current time.
void PrintTime()
{
    lcd.setCursor(0,1);
    lcd.print(timeCurrent[HOUR]);
    lcd.print(":");
    lcd.print(timeCurrent[MIN]);
    //lcd.print("m");
    //lcd.print(timeCurrent[SEC]);
}

///Checks if the alarm is supposed to start.
void CheckForAlarm()
{
    if (timeCurrent[HOUR] >= TIME_ALARM_HOUR && timeCurrent[MIN] >= TIME_ALARM_MIN && timeCurrent[SEC] >= TIME_ALARM_SEC)
    {
        currentMode = Mode::Alarm;
    }
}

//---------------- Simon functions ----------------

void Simon()
{
    int switchState1 = 0;
    int switchState2 = 0;
    int switchState3 = 0;
    int sequence[5] = {0,0,0,0,0};
    int order[5] = {Random(1,3), Random(1,3), Random(1,3), Random(1,3), Random(1,3)};
    int n = 0, i = 0;
    int time = 0;
    Serial.print("       ");
    Serial.print(order[0]);
    Serial.print(order[1]);
    Serial.print(order[2]);
    Serial.print(order[3]);
    Serial.print(order[4]);

    for(i==0 ; i<=5 ; i++)
    {
        if(order[i]==1)
        {
            digitalWrite(23,HIGH);
            delay(1000);
            digitalWrite(23,LOW);
            delay(500);
        }
        if(order[i]==2)
        {
            digitalWrite(25,HIGH);
            delay(1000);
            digitalWrite(25,LOW);
            delay(500);
        }
        if(order[i]==3)
        {
            digitalWrite(27,HIGH);
            delay(1000);
            digitalWrite(27,LOW);
            delay(500);
        }
    }

    while(n <= 4 && time < 15000)
    {   
        time = millis();
        switchState1 = digitalRead(22);
        switchState2 = digitalRead(24);
        switchState3 = digitalRead(26);
  
        if (switchState1 == 1)
        {
            digitalWrite(23, HIGH);
            delay(200);
            digitalWrite(23,LOW);
            sequence[n] = 1;
            n++;
        }
        if (switchState2 == 1)
        {
            digitalWrite(25, HIGH);
            delay(200);
            digitalWrite(25,LOW);
            sequence[n] = 2;
            n++;
        }
        if (switchState3 == 1)
        {
            digitalWrite(27, HIGH);
            delay(200);
            digitalWrite(27,LOW);
            sequence[n] = 3;
            n++;
        }
        delay(50);
        //Serial.print(switchState1);
        //Serial.print(switchState2);
        //Serial.print(switchState3);
    }
    if ((sequence[0] == order[0]) && (sequence[1] == order[1]) && (sequence[2] == order[2]) && (sequence[3] == order[3]) && (sequence[4] == order[4])){
        Serial.println("HEEEEEEELLLLLLLLLL   YYYYYHHHHEAAAAAAAA"); // tu peux mettre le return true ici
        digitalWrite(23, HIGH);
        digitalWrite(25, HIGH);
        digitalWrite(27, HIGH);
        delay(500);
        digitalWrite(23, LOW);
        digitalWrite(25, LOW);
        digitalWrite(27, LOW);
        delay(500);
        digitalWrite(23, HIGH);
        digitalWrite(25, HIGH);
        digitalWrite(27, HIGH);
        delay(500);
        digitalWrite(23, LOW);
        digitalWrite(25, LOW);
        digitalWrite(27, LOW);
        delay(1000000);// meme chose que le commentaire dans le sprochaines lignes
    }
    else
    {
        digitalWrite(27,HIGH);
        Serial.println("CRISS DE CAVE");
        // tu peux mettre le return false ici
        //delay(100000);// j'ai mis ca juste pour le test, dans le vraie code il va sortir de la fonction. c'est juste que
        //il loop back si on le fait pas sortir
        delay(1000000);
    }
}

//---------------- Init and loop functions ----------------

void setup()
{
    //Board initialization

    //--- General ---
    BoardInit();
    AudioInit();
    lcd.begin(6,1);

    //--- Simon ---
    pinMode(23, OUTPUT); //pour le 1
    pinMode(22, INPUT);

    pinMode(25, OUTPUT); //pour le 2
    pinMode(24, INPUT);

    pinMode(27, OUTPUT); //pour le 3
    pinMode(26, INPUT);


    //Variables initiation

    //--- General ---
    currentMode = Mode::Sleep;
    firstTimeInLoop = true;
    timeIni = 0;

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
    position[X] = INI_X;
    position[Y] = INI_Y;
    pulsesToTravel[LEFT] = 0;
    pulsesToTravel[RIGHT] = 0;
    moveCompleted = true;

    //--- Clock ---
    timeCurrent[HOUR] = TIME_DEFAULT_HOUR;
    timeCurrent[MIN] = TIME_DEFAULT_MIN;
    timeCurrent[SEC] = TIME_DEFAULT_SEC;
    timePrevious = 0;
}

void loop()
{
    Clock();
    PrintTime();
    if(currentMode == Mode::Sleep)
    {
        CheckForAlarm();
    }
    else if (currentMode == Mode::Alarm)
    {
        if(firstTimeInLoop)
        {
            timeIni = millis();
            firstTimeInLoop = false;
        }
        Move();
        PlayMusic();
        if(millis() - timeIni > TIME_START_BELLS)
        {
            //call the function that triggers the bells
        }
        CheckForInteraction();
    }
    else if (currentMode == Mode::Simon)
    {
        Simon();
    }
    
    //SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}