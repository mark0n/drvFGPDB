#include <stdio.h>
#include <functional>

#include "epicsTimer.h"



// typedef for eventTimer callback functions
typedef double (*eventTimerFunc)(void *obj);

// Return values for eventTimer callback functions
// Values > 0 specify interval (in secs) until next callback occurs
static constexpr double  DefaultInterval = 0.0;  //!< Use default time till next callback
static constexpr double  DontReschedule = -1.0;  //!< Sleep until restarted/triggered


/*
  NOTES regarding concurrent calls to the start() and expire() functions:

  Based on the descriptions for the epicsTimer start() and cancel() functions,
  if a different thread calls start() before a callback to the expire() function
  (by the timer queue thread) completes, the call to start() will be blocked
  until the expire() call completes.

  The implication is that this avoids a race condition where the resulting
  state of the timer could be determined either by the return value of the
  expire() function or the one specified in the call to the start() function.
  In particular, the value specified by a concurrent call to the start()
  function should effectively override the value returned by the expired()
  function (since it will always be applied AFTER expire() completes).

  Unfortunately, it also means that any thread that reschedules a timer will
  be blocked if the expire() function is running in (or is about to be called
  by) the timer queue thread.
*/

//----------------------------------------------------------------------------
class eventTimer : public epicsTimerNotify
{
  public:

    /**
     * @brief Constructor for a event timer object that does a callback to a
     *        specified function after a specified interval of time passes.
     *
     * @param[in] handlerFunc    func to be called at end of interval
     * @param[in] defaultDelay   default interval between callbacks
     * @param[in] queue          epicsTimerQueue that manages the timer
     */
    eventTimer(std::function<double()> handlerFunc, double defaultDelay,
               epicsTimerQueueActive &queue) :
        m_handlerFunc(handlerFunc),
        normDelay(defaultDelay),
        timer(queue.createTimer())
        { }

    /**
     * @brief Method to cancel and free all resources used by the timer. Note
     *        that this will block if the timer's expire callback is running
     *        (or is about to be called).  This needs to be called for each
     *        timer using a queue before releasing the queue.
     */
    void destroy(void)  { timer.destroy(); }

    /**
     * @brief Method to activate an inactive/idle timer using its default
     *        trigger interval or reduce its current trigger interval if it is
     *        longer than the default interval
     */
    void start(void) { start(normDelay); }
    /**
     * @brief Method to activate an inactive/idle timer using its default
     *        trigger interval or reduce its current interval if it is longer
     *        than the specified one
     */
    void start(double delay)  {
      if (delay < 0.0)  return;
      epicsTimer::expireInfo info = timer.getExpireInfo();
      if (!info.active or (info.expireTime - epicsTime::getCurrent() > delay))
        timer.start(*this, delay);
    }

    /**
     * @brief Method to activate an inactive/idle timer or change an active one
     *        to trigger after its default interval expires.
     */
    void restart(void)  { timer.start(*this, normDelay); }
    /**
     * @brief Method to activate an inactive/idle timer or change an active one
     *        to trigger after the specified interval expires.
     */
    void restart(double delay)  { timer.start(*this, delay); }

    /**
     * @brief Method to trigger the callback ASAP
     */
    void wakeUp(void)  { start(0.0); }

    /**
     * @brief Method to get the time until a timer triggers
     *
     * @return The # of seconds until the timer expires and triggers its
     *         callback.  Returns -DBL_MAX if the timer is not active.
     *         Any other negative value indicates how long since the timer
     *         should have expired.
     */
    double getExpireDelay()  {
      epicsTimer::expireInfo info = timer.getExpireInfo();
      if (!info.active)  return -DBL_MAX;
      return info.expireTime - epicsTime::getCurrent();
    }

    /**
     * @brief Method called when the timeout interval for an active timer
     *        expires.  Used to call the callback function specifed in the
     *        constructor and to optionally restart the timer.
     *
     * @return An expireStatus object that determines if the timer will be
     *         restarted and, if so, how long until it expires and triggers
     *         another callback.
     */
    virtual expireStatus expire(const epicsTime &)  {
      double newDelay = m_handlerFunc();
      if (newDelay < 0.0)  return expireStatus(epicsTimerNotify::noRestart);
      if (newDelay == DefaultInterval)
        return expireStatus(epicsTimerNotify::restart, normDelay);
      return expireStatus(epicsTimerNotify::restart, newDelay);
    }

  private:
    std::function<double()> m_handlerFunc; //!< func to call each time timer expires
    const double  normDelay;    //!< The default interval between callbacks
    epicsTimer  &timer;         //!< EPICS libCom timer object
};


