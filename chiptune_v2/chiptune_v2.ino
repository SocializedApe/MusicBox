#include <EEPROM.h>
#include "pitches.h"

#define REST 0
#define HIT  1

#include "song.h"

#define BUZZER_PIN 4
#define SERVO_PIN  8
#define MELODY_PIN 9
#define HARM_PIN   10
#define RELAY_PIN  2
#define BUTTON_PIN 3

#define MAGIC   0xF1A5
#define VERSION 5

//because buzzer struggles with low freq
#define OCTAVE_UP 2

const int kick[] PROGMEM = {
  HIT,4,  REST,4, REST,4, REST,4,
};

const int snare[] PROGMEM = {
  REST,4, REST,4, HIT,4,  REST,4,
};

#define VOICES 5
const int *const SONG[VOICES] = {bass, melody, harmony, kick, snare};
const unsigned int SONG_LENGTH[VOICES] = {
  sizeof(bass) / sizeof(bass[0]) / 2,
  sizeof(melody) / sizeof(melody[0]) / 2,
  sizeof(harmony) / sizeof(harmony[0]) / 2,
  sizeof(kick) / sizeof(kick[0]) / 2,
  sizeof(snare) / sizeof(snare[0]) / 2,
};

struct Saved {
  unsigned int magic;
  byte version;
  unsigned int bar;
};
Saved data;

// a list of indexes for each voice. eg notes[0] might be 10 etc.
unsigned int notes[VOICES];
//how many notes left to play? all user timer 0. eg note 1 might have 20 ticks eg 20 ms left to play etc.
unsigned int noteLeft[VOICES];
//how long until we hit again
byte hitLeft = 0;
// counts whe reachs servoPulse for pulsing shit
byte servoCount = 0;

//how muc click left for relay
byte clickLeft = 0;
// true ~every 1ms timer 0
volatile bool tick = false;
// 2 hit, 1 rest, when 2 goes one dir, and 1 goes up.
volatile byte servoPulse = 1;


volatile unsigned int melodyHalf = 0;
volatile unsigned int harmHalf = 0;
//paused interrupt
volatile bool paused = true;
volatile unsigned long lastPress = 0;
bool wasPaused = true;

// runs ~1 ms
ISR(TIMER0_COMPA_vect)
{
  tick = true;
  //if servo reaches 20 it resets to 0, and servo goes up, else servo goes up
  if (++servoCount == 20) {
    servoCount = 0;
    //hit for 2 ms
    digitalWrite(SERVO_PIN, HIGH);
  }
  // its like HIGH HIGH, then == setrouce count == servo pulse and we go low for 18MS or 19 MS if servoe pulse =1
  if (servoCount == servoPulse) digitalWrite(SERVO_PIN, LOW); //every servo count pulses
}

//
ISR(TIMER1_COMPA_vect)
{
  OCR1A += melodyHalf;
}

ISR(TIMER1_COMPB_vect)
{
  OCR1B += harmHalf;
}

//button ISR
ISR(INT1_vect)
{
  if (millis() - lastPress > 200) {
    paused = !paused;
    lastPress = millis();
  }
}

// if does not exist load default else do smth else.
void loadData() {
  EEPROM.get(0, data);
  if (data.magic != MAGIC || data.version != VERSION || data.bar >= SONG_BARS) {
    data = {MAGIC, VERSION, 0};
    EEPROM.put(0, data);
  }
}

//need to read from flash 10KB. word is two bytes. [v] voice table, i the index
int readSong(byte v, unsigned int i) {
  return pgm_read_word(&SONG[v][i]);
}

byte noteLen(int divider) {
  //
  byte len;

  // we get eg 16/4, so shortest note
  if (divider > 0) {
    len = 16 / divider;
  } else {
    // when minus its like 1.5 of note
    len = 24 / -divider;
  }
  return len;
}

void playNote(byte v, int freq, byte len) {
  // one tick of the time is 1.024 timer 0
  // 15000ms = 60000/4 one sisteen in ms.. 600000 = 60 s, we don DOM.
  noteLeft[v] = len * (15000 / 1.024 / BPM);

  if (freq == REST) return;

  if (v == 0) {
    tone(BUZZER_PIN, freq * OCTAVE_UP);
  } else if (v == 1) {
    noInterrupts();
    melodyHalf = 1000000UL / (freq * OCTAVE_UP);
    OCR1A = TCNT1 + melodyHalf;
    interrupts();
    bitSet(TCCR1A, COM1A0);
  } else if (v == 2) {
    noInterrupts();
    harmHalf = 1000000UL / (freq * OCTAVE_UP);
    OCR1B = TCNT1 + harmHalf;
    interrupts();
    bitSet(TCCR1A, COM1B0);
  } else if (v == 3) {
    digitalWrite(RELAY_PIN, HIGH);
    clickLeft = 30;
  } else {
    // we start smashing the drum at the start of the note
    servoPulse = 2;
    // and set how long it goes down.
    hitLeft = 80;
  }
}

void stopNote(byte v) {
  if (v == 0) noTone(BUZZER_PIN);
  else if (v == 1) bitClear(TCCR1A, COM1A0);
  else if (v == 2) bitClear(TCCR1A, COM1B0);
}

void startNote(byte v) {
  // mosition of the note in the array
  unsigned int i = notes[v] * 2;

  //  the freq we get from i
  int freq = readSong(v, i);

  //note length
  byte len = noteLen(readSong(v, i + 1));

  playNote(v, freq, len);
}

// start every voice from the saved bar, a long note gets only what is left of it
// resume the song
void seek() {
  unsigned long target = data.bar * 16;

  for (byte v = 0; v < VOICES; v++) {
    stopNote(v);
    notes[v] = 0;
    unsigned long t = 0;
    byte len = noteLen(readSong(v, 1));
    while (t + len <= target) {
      t += len;
      if (++notes[v] == SONG_LENGTH[v]) notes[v] = 0;
      len = noteLen(readSong(v, notes[v] * 2 + 1));
    }
    playNote(v, readSong(v, notes[v] * 2), t + len - target);
  }
}

void setup() {
  Serial.begin(115200);
  loadData();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(MELODY_PIN, OUTPUT);
  pinMode(HARM_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  bitSet(EICRA, ISC11);
  bitSet(EIMSK, INT1);
  //timer0 is 8 bit. once we hit this 128, eg once per count, we run time inerttup

  //
  OCR0A = 128;
  bitSet(TIMSK0, OCIE0A);

  TCCR1A = 0;
  TCCR1B = 0;
  bitSet(TCCR1B, CS11);
  bitSet(TIMSK1, OCIE1A);
  bitSet(TIMSK1, OCIE1B);

  Serial.println("press the button");
}

void loop() {
  if (paused != wasPaused) {
    wasPaused = paused;
    if (paused) {
      for (byte v = 0; v < VOICES; v++) stopNote(v);
      digitalWrite(RELAY_PIN, LOW);
      servoPulse = 1;
      EEPROM.put(0, data);
      Serial.println("pause");
    } else {
      Serial.print("play from bar ");
      Serial.println(data.bar);
      seek();
    }
  }
  if (paused) return;

  //
  if (tick) {
    tick = false;

    //if no hits left, we start moving up, changes the ISR
    if (hitLeft > 0 && --hitLeft == 0) servoPulse = 1;

    if (clickLeft > 0 && --clickLeft == 0) digitalWrite(RELAY_PIN, LOW);

    for (byte v = 0; v < VOICES; v++) {
      //after 1 ms we have left ress of the note
      noteLeft[v]--;

      //stops note.
      if (noteLeft[v] == 20) stopNote(v);

      // if no note left we move up on the data
      if (noteLeft[v] == 0) {
        if (++notes[v] == SONG_LENGTH[v]) {
          notes[v] = 0;
          // kick table is one bar, so it counts the bars
          if (v == 3 && ++data.bar == SONG_BARS) data.bar = 0;
          //every 16 notes w esave to eprom the index.
          if (v == 3 && data.bar % 16 == 0) EEPROM.put(0, data);
        }
        startNote(v);
      }
    }
  }
}
