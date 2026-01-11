// Venerate: A library for communicating to the ET312 box via serial
//
// tip is rx (yellow), ring is tx (green), body in gnd
// you must use a ttl to rs232 converter and hardware serial
//
// This arduino code is based off the et312-perl code but with the
// addition of some new methods that were not known before (figured out
// by trial-and-error watching all the available memory locations)
//
// June 2015

#include "Venerate.h"
#include <Arduino.h>

//#ifdef ARDUINO
//#include <Arduino.h>
//
//static void log_to_serial(void* ctx, const char* msg) {
//  auto* s = static_cast<HardwareSerial*>(ctx);
//  if (!s) return;
// s->print(msg);
//}
//#endif
//venerate.setdebug(1, log_to_serial, &Serial);

Venerate::Venerate(uint8_t boxid)
{
    _debug = 0;
    _state = 0;
    _boxid = boxid;
}

#ifdef HASSTREAM
void Venerate::begin(Stream &serial)
{
    _serial = &serial;
    _serial->setTimeout(20UL);
}
#endif

void Venerate::begin(cbfunc_t t, cbfunc_r r, cbfunc_f f)
{
    _txcb = t;
    _rxcb = r;
    _flushcb = f;
}

void Venerate::setmod(uint8_t mod) {
    _mod = mod;
}

bool Venerate::isconnected(void) {
    return (_state == 1);
}

// Send a series of uint8_ts to the box and return the result, based
// on the code from et312-perl

int Venerate::cp(uint8_t msg[], uint8_t n, uint8_t reply[]) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t c = msg[i];
        sum += c; // overflow expected and ok
        c ^= _mod;
        if (_debug)  _debugserial.printf("%02X ", (uint8_t)c);
        if (_txcb) {
            _txcb((uint8_t)c);
        } else {
#ifdef HASSTREAM
            _serial->write((uint8_t)c);
#endif
        }
    }
    if (n > 1) {
        if (_debug) _debugserial.printf("%02X", (uint8_t)(sum ^ _mod));
        if (_txcb) {
            _txcb((uint8_t)sum ^ _mod);
        } else {  
#ifdef HASSTREAM      
            _serial->write((uint8_t)sum ^ _mod);
#endif
        }
    }
    if (_debug) _debugserial.printf(" tx\n");
    if (_txcb)
        _flushcb();
    else { 
#ifdef HASSTREAM
        _serial->flush();
#endif
    }
    // In perl we wait for 10+10*1 milliseconds  20ms only!
    uint8_t bread = 0;
    if (!_txcb) {
#ifdef HASSTREAM
        bread = _serial->readBytes(reply, maxrxbytes);
#endif
    } else
        bread = _rxcb((char *)reply,maxrxbytes);

    if (bread <1) {
        if (_debug) _debugserial.printf("no rx\n");
        _state = 0;
    } else if (_debug) {
        for (int i = 0; i < bread; i++) {
            _debugserial.printf("%02X ", reply[i]);
        }
        _debugserial.printf(" rx\n");
    }
    return bread;
}

// Return the byte at the memory address or -1 if error

int Venerate::getbyte(int n) {
    uint8_t reply[maxrxbytes];
    uint8_t msg[3];

    msg[0] = 0x3c;
    msg[1] = (uint8_t)(n >> 8);
    msg[2] = (uint8_t)(n & 255);
    int count = Venerate::cp(msg, 3, reply);
    if (count < 3) return -1; // got to be 3 chars
    if (reply[0] != 0x22) return -1; // first is 0x22
    uint8_t sum = reply[0] + reply[1]; // with valid checksum, allow overflow
    if (sum != reply[2]) return -1;
    return reply[1];
}

// Send a byte to a memory address, false if error

bool Venerate::setbyte(int n, int b) {
    uint8_t reply[maxrxbytes];
    uint8_t msg[4];

    msg[0] = 0x4d;    msg[1] = (uint8_t)(n >> 8);
    msg[2] = (uint8_t)(n & 255);
    msg[3] = (uint8_t)(b);
    int count = Venerate::cp(msg, 4, reply);
    if (count < 1) return false; // got to be 1 chars
    if (reply[0] != 0x06) return false; // success is a 6
    return true;
}

// hello, hello, good to be back

bool Venerate::newhello()
{
    // Realign packet boundaries for the protocol
    // If another program has accessed the ET-312 before this session, we're
    // not sure what state it left the protocol in. Sending 0x0, possibly
    // encrypted with the key that the box established prior to this
    // session, should allow the box to realign the protocol. As the longest
    // command possible is 11 bytes (a command to write 8 bytes to an
    // address), we need to send up to 12 0s. Once we get back a 0x7, the
    // protocol is synced and we can move on.

    if (_state != 0) {
      if (_debug) _debugserial.printf("State !0\n");
        return Venerate::isconnected();
    }
    uint8_t rx[maxrxbytes];

    if (_debug) _debugserial.printf("tx hello\n");

    //    _mod = EEPROM.read(_boxid);
    _mod = 0;
    
    int s = 0;
    for (int i = 0; i < 12; i++) {
        uint8_t send[] = {0x00};
        _mod = 0;
        int chars = Venerate::cp(send, 1, rx);
        if (chars > 0 && rx[0] == 0x07) {
            s++;
            if (s>3)  // was 3
                break;
        }
    }
    if (s > 3) {
        if (_debug) _debugserial.printf("rx hello\n");
        uint8_t send[] = {0x2f, 0x00};
        _mod = 0;
        int chars = Venerate::cp(send, 2, rx);
        int sum = rx[0] + rx[1];
        if (sum > 256) sum -= 256;
        if (chars < 3 || rx[0] != 0x21 || sum != rx[2]) {
            if (_debug) _debugserial.printf("no sync\n");
        } else {
            _mod = rx[1] ^ 0x55;
            if (_debug) _debugserial.printf("%02X=mod\n", _mod);
            //EEPROM.write(_boxid, _mod);
        }
    }
    if (s>3) {
        // just a test memory get
        int y = Venerate::getbyte(ETMEM_knoba);
        if (y < 0) {
            if (_debug) _debugserial.printf("fail\n");
        } else {
            if (_debug) _debugserial.printf("%02X=knoba\n", y);
            _state = 1;
        }
    }
    return Venerate::isconnected();
}


bool Venerate::helloreadonly()
{
    if (_state != 0) {
        if (_debug) _debugserial.printf("State !0\n");
        return Venerate::isconnected();
    }
    uint8_t rx[maxrxbytes];

    if (_debug) _debugserial.printf("tx hello\n");
    int s = 0;
    for (int i = 0; i < 10; i++) {
        uint8_t send[] = {0x00};
        _mod = 0;
        int chars = Venerate::cp(send, 1, rx);
        if (chars > 0 && rx[0] == 0x07) {
            s++;
            if (s > 3) break;
        } else {
            s = 0;
        }
    }
    if (s > 3) {
        if (_debug) _debugserial.printf("rx hello\n");

	    // just a test memory get
	    int y = Venerate::getbyte(ETMEM_knoba);
	    if (y < 0) {
            if (_debug) _debugserial.printf("fail\n");
	    } else {
	        if (_debug) _debugserial.printf("%02X=knoba\n", y);
	        _state = 1;
	    }
    }
    return Venerate::isconnected();
}



