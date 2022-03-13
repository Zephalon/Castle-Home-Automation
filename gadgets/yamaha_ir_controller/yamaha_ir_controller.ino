/**
 * Analog IR Soundbar Controller
 * 
 * This project utilises the IR08-H IR tranceiver to control a YAMAHA YSP-1100 soundbar
 * 
 * @author: Fabian Prinz-Arnold
 */

// define IR codes
uint8_t increase_volume[5] =  {0xA1, 0xF1, 0x78, 0x87, 0x1E};
uint8_t decrease_volume[5] =  {0xA1, 0xF1, 0x78, 0x87, 0x1F};
uint8_t mute_volume[5] =      {0xA1, 0xF1, 0x78, 0x87, 0x9C};
uint8_t input_dvd[5] =        {0xA1, 0xF1, 0x78, 0x87, 0x4A};
uint8_t input_aux[5] =        {0xA1, 0xF1, 0x78, 0x87, 0xDE};
uint8_t output_5beam[5] =     {0xA1, 0xF1, 0x78, 0x87, 0xC2};
uint8_t output_st3beam[5] =   {0xA1, 0xF1, 0x78, 0x87, 0xC3};
uint8_t output_3beam[5] =     {0xA1, 0xF1, 0x78, 0x87, 0xC4};
uint8_t output_stereo[5] =    {0xA1, 0xF1, 0x78, 0x87, 0x50};

// settings
int min_volume = 5;
int max_volume = 40;
int minimum_delay = 300; // minimum delay between IR commands, in ms
int volume_sleep_timer = 3000; // time until the volume control needs to be reactivated, in ms

// setup
int current_volume = 0;
int requested_volume = 0;
int idle = 0;
int last_potentiometer_reading = -1;
bool stereo_mode = false;
bool muted = false;
int volume_range = max_volume - min_volume;

int incoming_byte = 0; // for incoming serial data

void setup() {
  Serial.begin(9600); // open serial port, set data rate to 9600 bps

  // set pin mode for buttons
  pinMode(2, INPUT);
  pinMode(3, INPUT);

  // do stuff if buttons are pressed
  attachInterrupt(digitalPinToInterrupt(2), leftButtonInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(3), rightButtonInterrupt, RISING);

  delay(1000); // give it time to boot

  // reset to default output mode
  Serial.write(output_5beam, 5);
  delay(minimum_delay);

  // reset volume to 0
  for (int x = 0; x <= max_volume; x++) {
    Serial.print(" Reset -> Volume: ");
    Serial.println(max_volume - x);
    Serial.write(decrease_volume, 5);
    delay(minimum_delay);
  }
  Serial.println("Setup done.");
}

void loop() {
  int potentiometer_reading = analogRead(A0);
  int requested_percent = potentiometer_reading / 10.21; // scaled to 100

  // this part is only needed to read incoming IR codes
  if (Serial.available() > 0) {
    // read the incoming byte:
    incoming_byte = Serial.read();
  
    // say what you got:
    Serial.print("I received: ");
    Serial.println(incoming_byte, HEX);
  }

  // filter out any noisy readings
  if (last_potentiometer_reading == -1 || abs(potentiometer_reading - last_potentiometer_reading) > 10) {
    // activate if muted
    if (muted) {
      Serial.write(mute_volume, 5);
      muted = false;
      delay(minimum_delay);
    }

    // check if the sound should be muted
    if (potentiometer_reading <= 10 && !muted) {
      // mute
      Serial.write(mute_volume, 5);
      Serial.println(" => Mute ");
      muted = true;
      delay(minimum_delay);
    } else {
      // get requested volume
      requested_volume = requested_percent * (volume_range * 0.01) + min_volume;
    }

    last_potentiometer_reading = potentiometer_reading; // save last reading
  }

  // change volume if needed
  if (!muted && requested_volume != current_volume) {
    // the device needs to be active to change the volume
    if (idle > volume_sleep_timer / minimum_delay) {
      Serial.write(decrease_volume, 5);
    }

    Serial.write(current_volume > requested_volume ? decrease_volume : increase_volume, 5);
    current_volume = current_volume > requested_volume ? current_volume = current_volume - 1 : current_volume = current_volume + 1;
    idle = 0;
    
    Serial.print(" -> Volume: ");
    Serial.print(requested_volume);
    Serial.print(" => ");
    Serial.println(current_volume);
  } else {
    idle = idle >= 100 ? 100 : idle + 1; // count the idle cycles to decide if the device is sleeping
  }
 
  delay(minimum_delay);
}

void leftButtonInterrupt() {
  Serial.println("left pressed");
  Serial.write(stereo_mode ? output_5beam : output_st3beam, 5);
  stereo_mode = !stereo_mode;
}

void rightButtonInterrupt() {
  Serial.println("right pressed");
  Serial.write(mute_volume, 5);
  muted = !muted;
}
