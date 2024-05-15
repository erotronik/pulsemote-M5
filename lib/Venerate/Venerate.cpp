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

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#elif defined(SPARK)
#include "application.h"
#endif

#include "Venerate.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "EEPROM.h" // store the last mod byte in case we lose power
#endif

Venerate::Venerate(byte boxid)
{
    _debug = 0;
    _state = 0;
    _boxid = boxid;
}

void Venerate::begin(Stream &serial)
{
    _serial = &serial;
    _serial->setTimeout(20UL);
}

void Venerate::begin(cbfunc_t t, cbfunc_r r, cbfunc_f f)
{
    _txcb = t;
    _rxcb = r;
    _flushcb = f;
}

void Venerate::setdebug(Stream &debugserial, byte debug) {
    _debug = debug;
    _debugserial = &debugserial;
}

void Venerate::setmod(byte mod) {
    _mod = mod;
}

boolean Venerate::isconnected(void) {
    return (_state == 1);
}

// Send a series of bytes to the box and return the result, based
// on the code from et312-perl

int Venerate::cp(byte msg[], byte n, byte reply[]) {
    byte sum = 0;
    for (byte i = 0; i < n; i++) {
        byte c = msg[i];
        sum += c; // overflow expected and ok
        c ^= _mod;
        if (_debug>1) {
            Serial.print(c, HEX);
            Serial.print(" ");
        }
        if (_txcb) {
            _txcb((byte)c);
        } else {
            _serial->write((byte)c);
        }
    }
    if (n > 1) {
        if (_debug>1) _debugserial->print((byte)(sum ^ _mod), HEX);
        if (_txcb) {
            _txcb((byte)sum ^ _mod);
        } else {        
            _serial->write((byte)sum ^ _mod);
        }
    }
    if (_debug>1) _debugserial->println(F(" tx"));
    if (_txcb)
        _flushcb();
    else         
        _serial->flush();
    // In perl we wait for 10+10*1 milliseconds  20ms only!
#if defined (SPARK)
    byte bread = _serial->readBytes((char *)reply, maxrxbytes);
#else
    byte bread = 0;
    if (!_txcb) 
        bread = _serial->readBytes(reply, maxrxbytes);
    else
        bread = _rxcb((char *)reply,maxrxbytes);
#endif

    if (bread <1) {
        if (_debug) _debugserial->println(F("no rx"));
        _state = 0;
    } else if (_debug>1) {
        for (int i = 0; i < bread; i++) {
            _debugserial->print(reply[i], HEX);
            _debugserial->print(F(" "));
        }
        _debugserial->println(F(" rx"));
    }
    return bread;
}

// Return the byte at the memory address or -1 if error

int Venerate::getbyte(int n) {
    byte reply[maxrxbytes];
    byte msg[3];

    msg[0] = 0x3c;
    msg[1] = (byte)(n >> 8);
    msg[2] = (byte)(n & 255);
    int count = Venerate::cp(msg, 3, reply);
    if (count < 3) return -1; // got to be 3 chars
    if (reply[0] != 0x22) return -1; // first is 0x22
    byte sum = reply[0] + reply[1]; // with valid checksum, allow overflow
    if (sum != reply[2]) return -1;
    return reply[1];
}

// Send a byte to a memory address, false if error

boolean Venerate::setbyte(int n, int b) {
    byte reply[maxrxbytes];
    byte msg[4];

    msg[0] = 0x4d;    msg[1] = (byte)(n >> 8);
    msg[2] = (byte)(n & 255);
    msg[3] = (byte)(b);
    int count = Venerate::cp(msg, 4, reply);
    if (count < 1) return false; // got to be 1 chars
    if (reply[0] != 0x06) return false; // success is a 6
    return true;
}

// hello, hello, good to be back

boolean Venerate::newhello()
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
      if (_debug) _debugserial->println(F("State !0"));
        return Venerate::isconnected();
    }
    byte rx[maxrxbytes];

    if (_debug) _debugserial->println(F("tx hello"));

    //    _mod = EEPROM.read(_boxid);
    _mod = 0;
    
    int s = 0;
    for (int i = 0; i < 12; i++) {
        byte send[] = {0x00};
        _mod = 0;
        int chars = Venerate::cp(send, 1, rx);
        if (chars > 0 && rx[0] == 0x07) {
            s++;
            if (s>3)  // was 3
                break;
        }
    }
    if (s > 3) {
        if (_debug) _debugserial->println("rx hello");
        byte send[] = {0x2f, 0x00};
        _mod = 0;
        int chars = Venerate::cp(send, 2, rx);
        int sum = rx[0] + rx[1];
        if (sum > 256) sum -= 256;
        if (chars < 3 || rx[0] != 0x21 || sum != rx[2]) {
            if (_debug) _debugserial->println(F("no sync"));
        } else {
            _mod = rx[1] ^ 0x55;
            if (_debug) {
                _debugserial->print(_mod, HEX);
                _debugserial->println(F("=mod"));
            }
            EEPROM.write(_boxid, _mod);
        }
    }
    if (s>3) {
    // just a test memory get
    int y = Venerate::getbyte(ETMEM_knoba);
    if (y < 0) {
        if (_debug) _debugserial->println(F("fail"));
    } else {
        if (_debug) {
            _debugserial->print(y, HEX);
            _debugserial->println(F("=knoba"));
        }
        _state = 1;
    }
    }
    return Venerate::isconnected();
}


boolean Venerate::helloreadonly()
{
  if (_state != 0) {
      if (_debug) _debugserial->println(F("State !0"));
        return Venerate::isconnected();
    }
    byte rx[maxrxbytes];

    if (_debug) _debugserial->println(F("tx hello"));
    int s = 0;
    for (int i = 0; i < 10; i++) {
        byte send[] = {0x00};
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
        if (_debug) _debugserial->println(F("rx hello"));

	// just a test memory get
	int y = Venerate::getbyte(ETMEM_knoba);
	if (y < 0) {
            if (_debug) _debugserial->println(F("fail"));
	} else {
	  if (_debug) {
            _debugserial->print(y, HEX);
            _debugserial->println(F("=knoba"));
	  }
	  _state = 1;
	}
    }
    return Venerate::isconnected();
}



