/*

The MIT License (MIT)

Copyright (c) 2016 Hubert Denkmair

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#include "config.h"
#include "hal_include.h"
#include "timer.h"
#include "util.h"

#ifdef CANDLE_HW_TIMESTAMP
#define TIMER_TICK_HZ 1000000UL
#define TIMER_TICK_NS (1000000000ULL / TIMER_TICK_HZ)

static volatile uint64_t timer_high_word;
static volatile uint32_t timer_last_sample;

#if defined(CANDLE_HW_TIMESTAMP_DEBUG)
#ifndef CANDLE_HW_TIMESTAMP_DEBUG_COUNT
#define CANDLE_HW_TIMESTAMP_DEBUG_COUNT 16
#endif

static void timer_debug_emit(uint64_t timestamp)
{
        static uint32_t debug_sample_count;
        static uint64_t previous_timestamp;

        if (debug_sample_count >= CANDLE_HW_TIMESTAMP_DEBUG_COUNT) {
                return;
        }

        uint64_t delta = (debug_sample_count == 0) ? 0 : (timestamp - previous_timestamp);

        char buf[48];
        char *p = buf;

        const char prefix[] = "HWTS ";
        for (unsigned int i = 0; i < (sizeof(prefix) - 1U); i++) {
                *p++ = prefix[i];
        }

        char hexbuf[9];
        hex32(hexbuf, (uint32_t)(timestamp >> 32));
        for (unsigned int i = 0; i < 8; i++) {
                *p++ = hexbuf[i];
        }
        *p++ = ':';
        hex32(hexbuf, (uint32_t)timestamp);
        for (unsigned int i = 0; i < 8; i++) {
                *p++ = hexbuf[i];
        }

        *p++ = ' ';

        hex32(hexbuf, (uint32_t)(delta >> 32));
        for (unsigned int i = 0; i < 8; i++) {
                *p++ = hexbuf[i];
        }
        *p++ = ':';
        hex32(hexbuf, (uint32_t)delta);
        for (unsigned int i = 0; i < 8; i++) {
                *p++ = hexbuf[i];
        }

        *p++ = '\r';
        *p++ = '\n';

        for (char *c = buf; c < p; c++) {
                ITM_SendChar((uint32_t)*c);
        }

        previous_timestamp = timestamp;
        debug_sample_count++;
}
#else
static inline void timer_debug_emit(uint64_t timestamp)
{
        (void)timestamp;
}
#endif
#endif

void timer_init(void)
{
        __HAL_RCC_TIM2_CLK_ENABLE();

        TIM2->CR1 = 0;
	TIM2->CR2 = 0;
	TIM2->SMCR = 0;
	TIM2->DIER = 0;
	TIM2->CCMR1 = 0;
	TIM2->CCMR2 = 0;
	TIM2->CCER = 0;
        TIM2->PSC = (TIM2_CLOCK_SPEED / 1000000) - 1;   // run @1MHz = 1us
        TIM2->ARR = 0xFFFFFFFF;
        TIM2->CR1 |= TIM_CR1_CEN;
        TIM2->EGR = TIM_EGR_UG;

#ifdef CANDLE_HW_TIMESTAMP
        timer_high_word = 0;
        timer_last_sample = TIM2->CNT;
#endif
}

uint32_t timer_get(void)
{
        return TIM2->CNT;
}

#ifdef CANDLE_HW_TIMESTAMP
uint64_t timer_get_timestamp(void)
{
        bool was_irq_enabled = disable_irq();
        uint32_t ticks = TIM2->CNT;

        if (ticks < timer_last_sample) {
                timer_high_word += (1ULL << 32);
        }

        timer_last_sample = ticks;

        uint64_t timestamp = timer_high_word | ticks;
        restore_irq(was_irq_enabled);

        timer_debug_emit(timestamp);

        return timestamp;
}

uint32_t timer_timestamp_to_us32(uint64_t timestamp)
{
        return (uint32_t)timestamp;
}

uint64_t timer_ticks_to_ns(uint64_t ticks)
{
        return ticks * TIMER_TICK_NS;
}
#endif
