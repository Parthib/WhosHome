// Need to import the following library for speech recognition support.
// https://www.audeme.com/downloads.html
#include "MOVIShield.h"
#include <stdlib.h>

// Four known names and three people with simple unknown names
#define SAMPLE_SIZE 8

// speech recognition object
MOVI recognizer(true);
// stores the latest reading of the distance from the sensor
int pm_value;
// stores the reading of the distance from the sensor from the previous iteration
int pm_value_old;
// stores the distance to the ground in mm
int baseline = -1;
// two different modes. When greeting is enabled, the program greets you when you walk underneath the sensor
bool greeting = false;
 
// array of heights in millimeters
int heights[SAMPLE_SIZE] = {-1, -1, -1, -1, -1, -1, -1, -1};
// array of names
String names[SAMPLE_SIZE] = {"Rishab", "Parthib", "Patrick", "Professor Manohar", "Ivan", "Person A", "Person B", "Person C"};
// binary semaphore system for recognizing whether someone is in the room or not
int present[SAMPLE_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0};
 
void setup() {
  // initialize the voice recognizer
  recognizer.init();
  // we call the recognizer by saying, "Arduino!"
  recognizer.callSign(F("Arduino"));

  // train the recognizer with all the possible commands
  recognizer.addSentence(F("Recalibrate."));
  recognizer.addSentence(F("Who is here?"));
  recognizer.addSentence(F("What is my height?"));
  recognizer.addSentence(F("Who am I?"));
  recognizer.addSentence(F("Set Rishab height"));
  recognizer.addSentence(F("Set Parthib height"));
  recognizer.addSentence(F("Set Patrick height"));
  recognizer.addSentence(F("Set Professor height"));
  recognizer.addSentence(F("Set Ivan height"));
  recognizer.addSentence(F("Set Person A height"));
  recognizer.addSentence(F("Set Person B height"));
  recognizer.addSentence(F("Set Person C height"));
  recognizer.addSentence(F("Enable Greeting Mode"));
  recognizer.addSentence(F("Disable Greeting Mode"));
  recognizer.train();

  // set the sonar sensor as an input
  pinMode(5, INPUT);

  // for debugging only
  // Serial.begin(9600);

  // start off by setting the baseline distance from the ceiling to the ground
  calibrate();
}

// listens for speech input and sensor readings
void loop() {
  // listen for any voice command
  int res = recognizer.poll();
  // check the sonar reading for the distance from the sonar sensor
  pm_value = pulseIn(5, HIGH);
  // Serial.println(pm_value); // for debugging only

  // check if we are in greeting mode
  if (greeting) {
    // only if we see a significant difference, call whoami (checks if anyone known has walked underneath)
    if (abs(pm_value - pm_value_old) > 100) whoami(0);
  }
  // this is the old reading for the next iteration
  pm_value_old = pm_value;

  // if we have asked it to be in greeting mode
  if (res == 13) {
    if (greeting) recognizer.say("I am already in greeting mode");
    else {
      recognizer.say("I am now in greeting mode");
      greeting = true;
    }
  }
  // if we have asked it to exit greeting mode
  else if (res == 14) {
    if (!greeting) recognizer.say("I am already not in greeting mode");
    else {
      recognizer.say("I am no longer in greeting mode");
      greeting = false;
    }
  }
  // recalibrate the distance to the ground
  else if (res == 1) calibrate();
  // tell us who is in the room based ons entries/exit
  else if (res == 2) whoishere();
  // these methods run if we are not in greeting mode
  if (!greeting) {
    // stand underneath and ask for your height
    if (res == 3) whatismyheight();
    // stand underneath and ask who you are
    else if (res == 4) whoami(1);
    // set your height (use person A/B/C if not Parthib, Patrick, Rishab, Rajit or Ivan)
    else if (res > 4 && res <= 12) updateheight(res-5);
  }
  

}
 
// sets the baseline height to the ground
void calibrate() {
  // the biggest possible reading must be the distance to the floor
  int r_max = pulseIn(5, HIGH);
  int temp;
  recognizer.say("Calibrating");
    // poll 10 times. This will take ~2 seconds
  for (int i=0; i < 10; i++) {
    temp = pulseIn(5, HIGH);
    if (temp > r_max) r_max = temp;
    delay(200);
  }
  // the baseline is now equal to this value
  baseline = r_max;
  // tell us the distance to the ground
  recognizer.say("Calibrated. The distance to the ground is " + floatToString(baseline));
}
 
// does the reading correspond to a person we know -- mode = 0 implies greeting mode, 1 is command mode
void whoami(int mode) {
  int temp;
  // poll 10 times to get real value
  for (int i=0; i < 10; i++) {
    temp = pulseIn(5, HIGH);
    if (temp < pm_value) pm_value = temp;
  }
  // find the actual height in mm
  int true_height = baseline - pm_value;

  // check whose height is closest to what we just saw
  int nearest = baseline;
  int person_index = 0;
  // loop through everybody's heights
  for (int i=0; i<SAMPLE_SIZE; i++) {
    if (abs(heights[i] - true_height) < nearest) {
      // if someone taller seems nearer to the value, but someone shorter is as close or even 20mm farther, it must
      // be the shorter person (experimentally obtained)
      if (true_height - heights[i] > 0 && true_height - heights[i] + 20 > nearest) continue;
      nearest = abs(heights[i] - true_height);
      // keep track of who it is
      person_index = i;
    }
  }
  // must be within 2 inches of the nearest height -- NOTE that if someone has not been added
  // to the heights and is within 2 inches of someone who has been added, it will think it
  // is the already added person
  if ((nearest < 50) && (heights[person_index] != -1)) {
    // greeting mode
    if (mode == 0) {
        // for debugging only
        // Serial.println(heights[person_index]);
        // Serial.println(true_height);
        // this person has arrived or left, change their semaphore value
        if (present[person_index] == 0) present[person_index] = 1;
        else present[person_index] = 0;
        // tell us who walked by
        if (present[person_index]) recognizer.say("Welcome, " + names[person_index]);
        else recognizer.say("Goodbye, " + names[person_index]);
    }
    else {
        // i have asked for my name
        recognizer.say("You are " + names[person_index]);
    }
  }
}
 
// tells us who is in the room
void whoishere() {
  // build up a sentence with the names of people who are here
  String sentence = "";
  // to make sure at least one person is present
  boolean onePresent = false;
  for (int i=0; i < SAMPLE_SIZE; i++) {
    if (present[i]) {
      sentence += names[i];
      sentence += " is here. ";
      onePresent = true;
    }
  }
  // tell us who is here!
  if (!onePresent) recognizer.say("Nobody is here");
  else recognizer.say(sentence);
}

// checks the height of whoever stands underneath the sensor
// MUST NOT BE in greeting mode to call this
void whatismyheight() {
  String height = floatToString(baseline - pulseIn(5, HIGH));
  recognizer.say("Your height is" + height);
}

// adds/updates the height of the specified person in the global array
// MUST NOT BE in greeting mode to call this
void updateheight(int index) {
    heights[index] = baseline - pulseIn(5, HIGH);
    // tell me my new height
    recognizer.say("Okay " + names[index] + ", you are " + floatToString(heights[index]));
}

// -------------- HELPER ----------------- //
 
// height in millimeters
String floatToString(int h) {
  // convert to inches first
  float inches = (h + 50) / 25.4; // experimentally found it underreported by 2 inches
  // take the feet from the inches
  int feet = (int) (inches / 12);
  // take the inches remaining
  int remainder = (int) inches % 12;
 
  String height = "";
  height += feet;
  height += " foot, ";
  height += remainder;
  height += " inches.";
 
  return height;
}
