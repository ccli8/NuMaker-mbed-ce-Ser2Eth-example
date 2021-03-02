/**
 * @file    BufferSerial.cpp
 * @brief   Software Buffer - Extends mbed Serial functionallity adding irq driven TX and RX
 * @author  sam grove
 * @version 1.0
 * @see
 *
 * Copyright (c) 2013
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BufferSerial.h"
#include <stdarg.h>

#if MBED_MAJOR_VERSION >= 6

BufferSerial::BufferSerial(PinName tx, PinName rx, uint32_t buf_size, uint32_t tx_multiple, const char* name)
    : MyUnbufferedSerial(tx, rx) , _rxbuf(buf_size), _txbuf((uint32_t)(tx_multiple*buf_size))
#else
BufferSerial::BufferSerial(PinName tx, PinName rx, uint32_t buf_size, uint32_t tx_multiple, const char* name)
    : RawSerial(tx, rx) , _rxbuf(buf_size), _txbuf((uint32_t)(tx_multiple*buf_size))
#endif
{
#if MBED_MAJOR_VERSION >= 6
    MyUnbufferedSerial::attach(callback(this, &BufferSerial::rxIrq), SerialBase::RxIrq);
#else
    RawSerial::attach(this, &BufferSerial::rxIrq, Serial::RxIrq);
#endif
    this->_buf_size = buf_size;
    this->_tx_multiple = tx_multiple;   
    return;
}

BufferSerial::~BufferSerial(void)
{
#if MBED_MAJOR_VERSION >= 6
    MyUnbufferedSerial::attach(NULL, MyUnbufferedSerial::RxIrq);
    MyUnbufferedSerial::attach(NULL, MyUnbufferedSerial::TxIrq);
#else
    RawSerial::attach(NULL, RawSerial::RxIrq);
    RawSerial::attach(NULL, RawSerial::TxIrq);
#endif

    return;
}

int BufferSerial::readable(void)
{
    return _rxbuf.available();  // note: look if things are in the buffer
}

int BufferSerial::writeable(void)
{
    return 1;   // buffer allows overwriting by design, always true
}

int BufferSerial::getc(void)
{
    return _rxbuf;
}

int BufferSerial::putc(int c)
{
    _txbuf = (char)c;
    BufferSerial::prime();

    return c;
}

int BufferSerial::puts(const char *s)
{
    if (s != NULL) {
        const char* ptr = s;
    
        while(*(ptr) != 0) {
            _txbuf = *(ptr++);
        }
        _txbuf = '\n';  // done per puts definition
        BufferSerial::prime();
    
        return (ptr - s) + 1;
    }
    return 0;
}

int BufferSerial::printf(const char* format, ...)
{
    char buffer[this->_buf_size];
    memset(buffer,0,this->_buf_size);
    int r = 0;

    va_list arg;
    va_start(arg, format);
    r = vsprintf(buffer, format, arg);
    // this may not hit the heap but should alert the user anyways
    if(r > this->_buf_size) {
        error("%s %d buffer overwrite (max_buf_size: %d exceeded: %d)!\r\n", __FILE__, __LINE__,this->_buf_size,r);
        va_end(arg);
        return 0;
    }
    va_end(arg);
    r = BufferSerial::write(buffer, r);

    return r;
}

ssize_t BufferSerial::write(const void *s, size_t length)
{
    if (s != NULL && length > 0) {
        const char* ptr = (const char*)s;
        const char* end = ptr + length;
    
        while (ptr != end) {
            _txbuf = *(ptr++);
        }
        BufferSerial::prime();
    
        return ptr - (const char*)s;
    }
    return 0;
}


void BufferSerial::rxIrq(void)
{
    // read from the peripheral and make sure something is available
    if(serial_readable(&_serial)) {
        _rxbuf = serial_getc(&_serial); // if so load them into a buffer
    }

    return;
}

void BufferSerial::txIrq(void)
{
    // see if there is room in the hardware fifo and if something is in the software fifo
    while(serial_writable(&_serial)) {
        if(_txbuf.available()) {
            serial_putc(&_serial, (int)_txbuf.get());
        } else {
            // disable the TX interrupt when there is nothing left to send
#if MBED_MAJOR_VERSION >= 6
            MyUnbufferedSerial::attach(NULL, MyUnbufferedSerial::TxIrq);
#else
            RawSerial::attach(NULL, RawSerial::TxIrq);
#endif
            break;
        }
    }

    return;
}

void BufferSerial::prime(void)
{
    // if already busy then the irq will pick this up
    if(serial_writable(&_serial)) {
#if MBED_MAJOR_VERSION >= 6
        MyUnbufferedSerial::attach(NULL, MyUnbufferedSerial::TxIrq);    // make sure not to cause contention in the irq
        BufferSerial::txIrq();                // only write to hardware in one place
        MyUnbufferedSerial::attach(callback(this, &BufferSerial::txIrq), MyUnbufferedSerial::TxIrq);
#else
        RawSerial::attach(NULL, RawSerial::TxIrq);    // make sure not to cause contention in the irq
        BufferSerial::txIrq();                // only write to hardware in one place
        RawSerial::attach(this, &BufferSerial::txIrq, RawSerial::TxIrq);
#endif
    }

    return;
}


