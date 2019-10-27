/*
Projet: main.ino
Equipe: P29
Auteurs: Étienne Lefebvre
Description: Programme pour deplacer le ROBUS pour le defi du combattant.
Date: 25 octobre 2019
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

/* ****************************************************************************
Vos propres fonctions sont creees ici
**************************************************************************** */

///Converts a distance to the according number of pulses.
///distance : The distance in centimeters.
int DistanceToPulses(float distance)
{
    return (distance / WHEEL_CIRCUMFERENCE) * PULSES_PER_CYCLE;
}

///Finds the two sensors that are above the line.
///firstSensor : the pin of the first sensor (A0-A7).
///secondSensor : the pin of the second sensor (A0-A7).
int FindLine(int* firstSensor, int* secondSensor)
{
    int posBiggestValue = A0;
    //technically, the line should be under only 2 sensors of the 8

    //we start by searching for the biggest value (where the line is)
    for(int i = A1; i <= A7; i++)
    {
        posBiggestValue = analogRead(i) > analogRead(posBiggestValue) ? i : posBiggestValue;
    }

    //now we find the second sensor with the biggest value
    if(analogRead(posBiggestValue - 1) > analogRead(posBiggestValue + 1))
    {
        *firstSensor = posBiggestValue - 1;
        *secondSensor = posBiggestValue;
    }
    else
    {
        *firstSensor = posBiggestValue;
        *secondSensor = posBiggestValue + 1;
    }
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

///Function to move ROBUS in a straight line.
///speed : The base speed.
///pulses : The distance to travel in pulses. Should always be positive.
///useLineTracker : Enables/disables the line tracking feature.
void PID(float speed, int pulses, bool useLineTracker)
{
    int totalPulsesLeft = 0, totalPulsesRight = 0, 
        deltaPulsesLeft = 0, deltaPulsesRight = 0,
        errorDelta = 0, errorTotal = 0;

    int lineErrorLeft = 0, lineErrorRight = 0, lineFirstSensor = 0, lineSecondSensor = 0;

    const float kp = 0.001, ki = 0.001, kLine = 0.01;
    
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

    if(useLineTracker)
        FindLine(&lineFirstSensor, &lineSecondSensor);

    while(totalPulsesRight < pulses)
    {
        deltaPulsesLeft = abs(ENCODER_ReadReset(LEFT));
        deltaPulsesRight = abs(ENCODER_ReadReset(RIGHT));
        totalPulsesLeft += deltaPulsesLeft;
        totalPulsesRight += deltaPulsesRight;

        if(useLineTracker)
        {
            lineErrorLeft = analogRead(lineFirstSensor - 1) / analogRead(lineFirstSensor);
            lineErrorRight = analogRead(lineSecondSensor + 1) / analogRead(lineSecondSensor);
        }

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
        if(useLineTracker)
        {
            slaveSpeed += (lineErrorRight * kLine) - (lineErrorLeft * kLine);
        }
        MOTOR_SetSpeed(LEFT, speedSign * slaveSpeed);
        MOTOR_SetSpeed(RIGHT, speedSign * masterSpeed);
        delay(40);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Function to make ROBUS follow a line.
///speed : The base speed.
///pulses : The distance to travel in pulses. Should always be positive.
void LineTracker(float speed, int pulses)
{
    int totalPulsesLeft = 0, totalPulsesRight = 0,
        firstSensor = 0, secondSensor = 0;
    float lineErrorLeft = 0, lineErrorRight = 0;

    const float kLine = 0.05;
    
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

    FindLine(&firstSensor, &secondSensor);

    while(totalPulsesRight < pulses)
    {
        totalPulsesLeft += abs(ENCODER_ReadReset(LEFT));
        totalPulsesRight += abs(ENCODER_ReadReset(RIGHT));

        lineErrorLeft = analogRead(firstSensor - 1) / analogRead(firstSensor);
        lineErrorRight = analogRead(secondSensor + 1) / analogRead(secondSensor);

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
        slaveSpeed = masterSpeed + (lineErrorRight * kLine) - (lineErrorLeft * kLine);
        
        MOTOR_SetSpeed(LEFT, speedSign * slaveSpeed);
        MOTOR_SetSpeed(RIGHT, speedSign * masterSpeed);
        //delay(40);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Function to make ROBUS follow a line.
///speed : The base speed.
void LineTracker(float speed)
{
    int firstSensor = 0, secondSensor = 0;
    float lineErrorLeft = 0, lineErrorRight = 0;

    const float kLine = 0.05;
    
    float masterSpeed = 0, slaveSpeed = 0, slowSpeed = 0.2;
    int speedSign = speed > 0 ? 1 : -1;
    speed *= speedSign; //we'll convert it back to the negative value (if that's the case) at the end
    ENCODER_Reset(LEFT);
    ENCODER_Reset(RIGHT);

    //we give it a slow speed to start
    MOTOR_SetSpeed(LEFT, slowSpeed);
    MOTOR_SetSpeed(RIGHT, slowSpeed);

    FindLine(&firstSensor, &secondSensor);

    while(IsOnALine())
    {
        lineErrorLeft = analogRead(firstSensor - 1) / analogRead(firstSensor);
        lineErrorRight = analogRead(secondSensor + 1) / analogRead(secondSensor);

        masterSpeed = speed;
        slaveSpeed = masterSpeed + (lineErrorRight * kLine) - (lineErrorLeft * kLine);
        
        MOTOR_SetSpeed(LEFT, speedSign * slaveSpeed);
        MOTOR_SetSpeed(RIGHT, speedSign * masterSpeed);
        //delay(40);
    }
    MOTOR_SetSpeed(LEFT, 0);
    MOTOR_SetSpeed(RIGHT, 0);
}

///Move ROBUS according to the distance specified.
///distance : The distance to cover in centimeters. Can be negative.
///speed : The base speed. Must be between 0.15 and 1 for it to work.
///useLineTracker : Enables/disables the line tracking feature.
void Move(float distance, float speed, bool useLineTracker)
{
    int distanceSign = distance > 0 ? 1 : -1;
    //distanceSign is used to reverse the speed if the distance is negative
    PID(distanceSign * speed, DistanceToPulses(abs(distance)), useLineTracker);
}

void FollowLine(float speed, float distance)
{
    int distanceSign = distance > 0 ? 1 : -1;
    //distanceSign is used to reverse the speed if the distance is negative
    LineTracker(distanceSign * speed, DistanceToPulses(abs(distance)));
}

void FollowLine(float speed)
{
    LineTracker(speed);
}

///Turn ROBUS according to the angle specified.
///angle : the rotation angle in radians. Positive makes it go left,
///        while negative makes it go right.
void Turn(float angle)
{
    //only one wheel will move
    float radius = 19.33; //in centimeters
    int motor = angle > 0 ? RIGHT : LEFT;
    int totalPulses = 0;
    float pulsesRotation = DistanceToPulses(abs(radius * angle));

    ENCODER_Reset(motor);
    MOTOR_SetSpeed(motor, 0.4);
    while(totalPulses < pulsesRotation)
    {
        totalPulses = ENCODER_Read(motor);
        delay(20); //might not be necessary
    }
    MOTOR_SetSpeed(motor, 0);    
}

//----------------

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

void TestLineTrackerMove()
{
    FollowLine(0.2);
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
        TestLineTrackerMove();
    }
    // SOFT_TIMER_Update(); // A decommenter pour utiliser des compteurs logiciels
    delay(10);// Delais pour décharger le CPU
}