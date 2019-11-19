
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

///Increase the volume over time
void Volume(float baseTime)
{
    float baseVolume = 0.1;
    float newVolume; 
    float currentTime = millis() - baseTime; //baseTime: for testing
    newVolume = baseVolume + (currentTime / 20000 * (1 - baseVolume));
    if(newVolume >= 1.0)
    {
        AUDIO_SetVolume(1.0);
    }
    else
    {
        AUDIO_SetVolume(newVolume);
    }
}

//---------------- TESTS ----------------

void Tests()
{
    float baseTime = millis();
    AUDIO_SetVolume(0.1);
    AUDIO_Next();

    while(millis() - baseTime <= 20000)
    {
        Volume(baseTime);
    }
    AUDIO_Stop();
}

//-------------------------------------------------

void setup()
{
    BoardInit();
    AudioInit();
    /*AUDIO_SetVolume(0.5);
    delay(5000);
    AUDIO_Next();
    delay(10000);
    AUDIO_Stop();*/
}

void loop()
{
    
    if(ROBUS_IsBumper(REAR))
    {
        delay(500);
        Tests();
        //do something
    }
    //SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}