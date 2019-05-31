#include <Arduino.h>

#include <util/delay.h>

#define TRIG_PIN (1<<2)
#define SYNC_PIN (1<<3)

#define ECHO_D_MASK 0xFC
#define ECHO_B_MASK 0x03

#define REQUEST_PIN (1<<4)

#define ECHOS ((PIND & ECHO_D_MASK) | (PINB & ECHO_B_MASK))

#define ECHO_TIMEOUT   500 // us
#define MEAS_TIMEOUT 50000 // us

#define CORRECTION -8

uint32_t time() { return micros(); }

volatile uint8_t dist = 0;

ISR(PCINT0_vect) {
    if ((PINB & REQUEST_PIN) == 0) {
        Serial.write(dist);
    }
}

void setup() {
    Serial.begin(115200);
    DDRB  |= TRIG_PIN;
    PORTB |= SYNC_PIN;
    PORTD |= ECHO_D_MASK;
    PORTB |= ECHO_B_MASK;
    PORTB |= REQUEST_PIN;
    PCMSK0 |= REQUEST_PIN;
    PCICR |= 1<<PCIE0;
}

const char* to_hex_str(uint8_t v) {
    static char res[3] = { 0, 0, 0 };
    static const char c[] = "0123456789ABCDEF";
    res[0] = c[v >> 4]; 
    res[1] = c[v & 0x0F];
    return res;
}

uint8_t zeropos(uint8_t v) {
    for(uint8_t i = 0; i != 8; ++i)
        if ((v & (1<<i)) == 0)
            return i;
    return 8;
}

bool autosend = false;

void loop() {
    while ((PINB & SYNC_PIN) == 0) {}
    while ((PINB & SYNC_PIN) != 0) {}
    PORTB |= TRIG_PIN;
    _delay_us(10);
    PORTB &= ~TRIG_PIN;
    const uint32_t t_start = time();
    uint8_t echos = ECHOS;
    while (ECHOS != 0xFF) {
        if ((time() - t_start) > ECHO_TIMEOUT) {
            //Serial.print("Echo start timeout, read 0x");
            //Serial.println(to_hex_str(echos));
            return;
        }
        echos = ECHOS;
    }
    uint32_t rect[8];
    uint8_t rece[8];
    int8_t i = 0;
    for (; i != 8; ++i) {
        uint8_t last_echos = echos;
        if (echos == 0)
            break;
        uint32_t t = t_start;
        while(echos == last_echos) {
            if ((t - t_start) > MEAS_TIMEOUT) {
                //Serial.print("Echo rec timeout, read 0x");
                //Serial.println(to_hex_str(echos));
                i = -i;
                break;
            }
            t = time();
            echos = ECHOS;
        }
        if (i < 0) {
            i = -i;
            break;
        }
        rect[i] = t - t_start;
        rece[i] = echos;
    }
    // Serial.print("Received ");
    // Serial.print(i);
    // Serial.println(" changes:");
    uint8_t old_mask = 0;
    float dmin = -1;
    for (int8_t j = 0; j != i; ++j) {
        uint8_t reci = zeropos(rece[j] | old_mask);
        old_mask |= (1<<reci);
        float d = 340*(rect[j]/1e6)*100 + CORRECTION;
        if (j == 0) {
            dist = d < 255 ? d : 255;
            dmin = d;
        }
        // Serial.print('\t');
        // Serial.print(reci);
        // Serial.print(": ");
        // Serial.print(d);
        // Serial.println(" cm");
    }
    if (Serial.available()) {
        char c = Serial.read();
        switch(c) {
            case '\r':
                Serial.write('\n');
                break;
            case 's':
                Serial.println(dist);
            case 'm':
                autosend = false;
                break;
            case 'a':
                autosend = true;
                break;
        }
    }
    if (autosend)
        Serial.println(dmin);
}