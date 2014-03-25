#define A 8
#define B 9
#define C 10
#define NUM_LEDS 6

int leds[][NUM_LEDS] = {
    {A, B}, {B, A}, {B, C}, {C, B}, {A, C}, {C, A}
};

void setup() {
    /* High Impedance on all pins */
    pinMode(A, INPUT);
    pinMode(B, INPUT);
    pinMode(C, INPUT);
}

void loop() {
  
    int pattern[6] = {1, 1, 1, 1, 1, 0};

    while(1) {
      int *tmp_pattern = pattern;
      
      for (int i = 0; i < NUM_LEDS; i++) {
        if(pattern[i] == 1) {
          light(i);
        }
      }
    }
  
}

void light(int led) {
    setup();
    int *pattern = leds[led];
    pinMode(pattern[0], OUTPUT);
    digitalWrite(pattern[0], HIGH);
    pinMode(pattern[1], OUTPUT);
    digitalWrite(pattern[1], LOW);
}

