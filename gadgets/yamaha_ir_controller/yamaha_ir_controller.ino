/**
 * YAMAHA Analogue IR Controller
 * 
 * This project uses the IR08-H IR tranceiver to control a YAMAHA soundbar
 * 
 * @author: Fabian Prinz-Arnold
 */

// define IR codes
uint8_t increase_volume[5]={0xA1, 0xF1, 0x78, 0x87, 0x1E};
uint8_t decrease_volume[5]={0xA1, 0xF1, 0x78, 0x87, 0x1F};
uint8_t mute_volume[5]={0xA1, 0xF1, 0x78, 0x87, 0x9C};
uint8_t input_dvd[5]={0xA1, 0xF1, 0x78, 0x87, 0x4A};
uint8_t input_aux[5]={0xA1, 0xF1, 0x78, 0x87, 0xDE};
uint8_t output_5beam[5]={0xA1, 0xF1, 0x78, 0x87, 0xC2};
uint8_t output_st3beam[5]={0xA1, 0xF1, 0x78, 0x87, 0xC3};
uint8_t output_3beam[5]={0xA1, 0xF1, 0x78, 0x87, 0xC4};
uint8_t output_stereo[5]={0xA1, 0xF1, 0x78, 0x87, 0x50};

// settings
int max_volume = 60;
int minimum_delay = 300; // ms

// setup
int current_volume = 0;
int requested_volume = 0;
int idle = 0;
int last_potentiometer_reading = 0;
bool stereo_mode = false;

int incoming_byte = 0; // for incoming serial data

void setup() {
  Serial.begin(9600);     // open serial port, set data rate to 9600 bps
  
  pinMode(2, INPUT);
  pinMode(3, INPUT);

  attachInterrupt(digitalPinToInterrupt(2), leftButtonInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(3), rightButtonInterrupt, RISING);

  delay(3000); // give it time to boot

  Serial.write(output_5beam, 5); // reset to default mode

  delay(minimum_delay);

  // reset volume to 0
  for (int x = 0; x <= max_volume; x++) {
    Serial.print(" -> Volume: ");
    Serial.println(max_volume - x);
    Serial.write(decrease_volume, 5);
    delay(300);
  }
  Serial.println("ready");
}

void loop() {
  int potentiometer_reading = 1024 - analogRead(A0); // reversed

  // filter out any noisy readings and scale the output to maximum volume
  if (abs(potentiometer_reading - last_potentiometer_reading) > 20) requested_volume = potentiometer_reading / 10.21 * (max_volume * 0.01);
  
  // this part is only needed to read incoming IR codes
  /*
  if (Serial.available() > 0) {
          // read the incoming byte:
          incoming_byte = Serial.read();

          // say what you got:
          Serial.print("I received: ");
          Serial.println(incoming_byte, HEX);
  }
  */

  // change volume if needed
  if (requested_volume != current_volume) {
    last_potentiometer_reading = potentiometer_reading;

    // the device needs to be active to change the volume
    if (idle > 33) Serial.write(decrease_volume, 5);
    if (idle > 33) Serial.println(" -> ACTIVATE");
    idle = 0;

    Serial.write(current_volume > requested_volume ? decrease_volume : increase_volume, 5);
    current_volume = current_volume > requested_volume ? current_volume = current_volume - 1 : current_volume = current_volume + 1;
    
    Serial.print(" -> Volume: ");
    Serial.print(requested_volume);
    Serial.print(" => ");
    Serial.println(current_volume);
  } else {
    idle = idle >= 100 ? 100 : idle + 1; // count the idle cycles to decide, if the device is sleeping
  }
 
  delay(minimum_delay);
}

void leftButtonInterrupt() {
  Serial.println("left pressed");
  Serial.write(mute_volume, 5);
}

void rightButtonInterrupt() {
  Serial.println("right pressed");
  Serial.write(stereo_mode ? output_5beam : output_stereo, 5);
  stereo_mode = !stereo_mode;
}
