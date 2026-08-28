#include "esphome.h"

class AJ_SR04M_Sensor : public PollingComponent, public UARTDevice, public Sensor {
  public:
    float last_state;
    float old_state;
    AJ_SR04M_Sensor(UARTComponent *parent) : PollingComponent(150), UARTDevice(parent) {}

    void update() override {
      old_state = last_state;
      //ESP_LOGD("Start", "Last: %.2f", old_state);

      byte frame[4];
      int pos = 0;
      float value = 0.0;

      //write(0x00); // Try this
      //write(0x01); // Try this
      write(0x55); // Try this AJ_SR04M
      //write(0xFF); // Try this DYP-A12BNYTW-V1.0
      while (available()) {

        frame[pos] = read();
        //ESP_LOGD("Hex", "Hex: %X", frame[pos]);

        //pos++;

        if (pos == 3) {
          //ESP_LOGD("Validation", "pos == 4");

          //if ((frame[0] == 0xFF) && (frame[4] == 0x00) && (((frame[0] + frame[1] + frame[2]) & 0x00ff) == frame[3])){
          if ((frame[0] == 0xFF) && (((frame[0] + frame[1] + frame[2]) & 0x00FF) == frame[3])) {
            //ESP_LOGD("Validation", "SUM");
            value = ((frame[1] << 8) + frame[2]) / 10.0;
            publish_state(value);
            last_state = value;
            //ESP_LOGD("Start", "Last: %f", last_state);
          }
          break;


        }
        pos++;

      }
    }
};
