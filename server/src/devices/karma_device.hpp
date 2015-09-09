#include <string.h>
#include "../util/cas.hpp"
#include <l4/sys/ktrace.h>
#include <l4/rtc/rtc.h>

class KarmaDevice : public device::IDevice
{
    unsigned int _jiffies;
    unsigned int _reports;
    unsigned long long _last_report_tsc;
public:
    KarmaDevice(): _jiffies(0), _reports(0), _last_report_tsc(util::tsc()){}
    void jiffiesUpdate(unsigned int jiffies)
    {
        _jiffies += jiffies;
        ++_reports;
        fiasco_tbuf_log_3val("jiffy_report", 0, 0, 0);
        if(_jiffies >= 1000)
        {
            unsigned long long tsc_diff, now = util::tsc();
            tsc_diff = now - _last_report_tsc;
            _last_report_tsc = now;
            double seconds = (double)tsc_diff / (double)(l4re_kip()->frequency_cpu * 1000);
            double report_size = (double)_jiffies / (double) _reports;
            if(seconds > 0)
            {
                printf("jiffy freq %lf Hz in the last %lf seconds average report %lf\n",
                    (double)_jiffies / seconds,
                    seconds, 
                    report_size);
            }
            _jiffies = 0;
            _reports = 0;
        }
    }

    virtual void hypercall(HypercallPayload & hp)
    {
        const char * str_ptr;
        l4_uint32_t tmp;
        switch(hp.address())
        {
            case karma_df_printk:
                try {
                    str_ptr = (char*)GET_VM.mem().base_ptr() + hp.reg(0);
                    if(strlen(str_ptr) <= 2)
                        printf("%s", str_ptr);
                    else
                        printf("CPU%d: %s", current_cpu().id(), str_ptr);
                }catch(...){} // TODO what's with the try catch block ??? //nullpointers
                break;
            case karma_df_panic:
                try {
                    printf("Guest panic: %s\n", (char*)(GET_VM.mem().base_ptr() + hp.reg(0)));
                } catch(...) {}//nullpointers
                throw L4_PANIC_EXCEPTION;
                break;
            case karma_df_debug:
                printf("DEBUG: addr=%lx\n", hp.reg(0));
                break;
            case karma_df_get_time:
                l4rtc_get_seconds_since_1970(&tmp);
                hp.reg(0) = tmp;
                break;
            case karma_df_get_khz_cpu:
                hp.reg(0) = l4re_kip()->frequency_cpu;
                break;
            case karma_df_get_khz_bus:
                hp.reg(0) = l4re_kip()->frequency_bus;
                break;
            case karma_df_oops:
                printf("Guest reports Oops\n");
                break;
            case karma_df_report_jiffies_update:
                jiffiesUpdate(hp.reg(0));
                break;
            case karma_df_log3val:
                fiasco_tbuf_log_3val("guest_log", hp.reg(0), hp.reg(1), hp.reg(2));
                break;
            case karma_df_request_irq:
                karma_log(INFO, "karma_df_request_irq irq=%lu\n", hp.reg(0));
                GET_VM.gic().attach(hp.reg(0), hp.reg(0), false);
                GET_VM.gic().unmask(hp.reg(0));
                hp.reg(1) = 0;
                break;
            default:
                hp.reg(0) = -1UL;
                break;
        }
    }
};

