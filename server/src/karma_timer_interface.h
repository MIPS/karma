/*
 * karma_timer_interface.h
 *
 *  Created on: 09.06.2011
 *      Author: janis
 */

#ifndef KARMA_TIMER_INTERFACE_H_
#define KARMA_TIMER_INTERFACE_H_

class KarmaTimer{
public:
    /**
     * Initializes the timer.
     */
    virtual void init() = 0;
    /**
     * Sets the next event to be triggered by this timer in delta ticks. Where
     * the duration of one tick is given by getPeriodFS() in femtoseconds.
     * @param delta time until the next event in timer ticks
     * @param programming_timestamp timestamp of when delta was computed (tsc on x86)
     */
    virtual bool nextEvent(l4_uint32_t delta, l4_uint64_t programming_timestamp) = 0;
    /**
     * Sets the timer in periodic mode. Will rearm the timer with the last given delta
     * upon an event
     * @param periodic
     */
    virtual void setPeriodic(bool periodic) = 0;
    /**
     *
     * @return whether the timer is in periodic mode
     */
    virtual bool isPeriodic() const = 0;
    /**
     * Returns the frequency of timer ticks in kHz.
     */
    virtual l4_uint32_t getFreqKHz() const = 0;
    /**
     * Stops the timer
     */
    virtual void stop() = 0;
    /**
     * Shall be called when the timer fires
     * @return false if by any chance the the timer fired prematurely
     */
    virtual bool fire() = 0;
    /**
     * Returns the Irq-object triggered by this timer.
     * @return
     */
    virtual L4::Cap<L4::Irq> & irq_cap() = 0;
};


#endif /* KARMA_TIMER_INTERFACE_H_ */
