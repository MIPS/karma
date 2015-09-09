#include "hpet.h"
#include "devices_list.h"
#include "../util/debug.h"
#include "../params.h"
#include "../l4_mem.h"

L4_hpet::L4_hpet()
{}

void L4_hpet::init()
{
    printf("initializing l4_hpet!\n");
    if(params.virtual_cpus != 1)
    {
        karma_log(WARN, "Warning using hpet in smp mode. This is highly experimental, also only cpu 0 uses hpet as timer source\n");
    }
    l4io_device_handle_t hpet_device;
    l4io_resource_handle_t hpet_handle, hpet_mem_handle;
    l4io_resource_t hpet_resource_mem;
    int error = l4io_lookup_device("PNP0103", &hpet_device, 0, &hpet_handle);
    if(error)
    {
        karma_log(ERROR, "Fatal error: l4io_lookup_device PNP0103 failed (error = %i).\n"
               "Cannot use Hpet. Terminating.\n", error);
        exit(1);
    }
    hpet_mem_handle = hpet_handle;
    error = l4io_lookup_resource(hpet_device, L4IO_RESOURCE_MEM, &hpet_mem_handle, &hpet_resource_mem);
    if (error)
    {
        karma_log(ERROR, "Fatal error: l4io_lookup_device failed (error = %i)\n"
               "Cannot use Hpet. Terminating.", error);
        exit(1);
    }

    unsigned long hpet_address = hpet_resource_mem.start;

    karma_log(INFO, "HPET phys addr = %lx\n", hpet_address);

    l4io_resource_t hpet_resource_irq;
    std::set<unsigned> irqs;
    while (!l4io_lookup_resource(hpet_device, L4IO_RESOURCE_IRQ, &hpet_handle, &hpet_resource_irq))
    {
        irqs.insert(hpet_resource_irq.start);
        karma_log(DEBUG, "HPET IRQ: %ld - %ld\n", hpet_resource_irq.start, hpet_resource_irq.end);
    }
    l4_addr_t hpet_v_addr;
    if (l4io_request_iomem(hpet_address, L4_PAGESIZE, 0, &hpet_v_addr)){
        karma_log(ERROR, __FILE__ "@" TO_STRING(__LINE__) ": l4io_repuest_iomem failed. Terminating.\n");
        exit(1);
    }

    _hpet_driver = reinterpret_cast<L4::Driver::Hpet *>(hpet_v_addr);
    _hpet_timer = _hpet_driver->timer(2);
    unsigned irqnum = 8;
    for(std::set<unsigned>::iterator irq = irqs.begin(); irq != irqs.end(); ++irq)
    {
        if(*irq)
        {
            irqnum = *irq;
            break;
        }
    }

    if (l4io_request_irq(irqnum, _lirq.lirq().cap()))
    {
        karma_log(ERROR, __FILE__ "@" TO_STRING(__LINE__) ": l4io_repuest_irq failed (irgnum = %u). Terminating.\n", irqnum);
        exit(1);
    }

    _timer_freq_kHz = 1000000000000ULL/_hpet_driver->counter_clk_period();
//    _hpet_timer->force_32bit();
    _hpet_timer->set_int_type_edge();
    _hpet_timer->set_int_route(irqnum);

    _hpet_driver->enable();
    _hpet_driver->irq_clear_active(2);
    _hpet_timer->enable_int();


    _hpet_driver->print_state();
}

L4_hpet::~L4_hpet()
{
}

bool L4_hpet::scheduleNext()
{
    _hpet_timer->set_comparator(_next);
    _next = _hpet_timer->comparator();
    _lirq.lirq()->unmask();
    _hpet_timer->enable_int();
    if((l4_int32_t)(_next - (l4_uint32_t)_hpet_driver->main_counter_val()) <= 1)
    {
        _hpet_timer->disable_int();
        return false;
    }
    return true;
}

bool L4_hpet::nextEvent(l4_uint32_t delta, l4_uint64_t programming_timestamp)
{
    _delta = delta;
    _next = _hpet_driver->main_counter_val();
    _next += delta;
    l4_uint64_t programming_delay = util::tsc() - programming_timestamp;
    if(!_periodic)
    {
        // compensate for programming latency
        _next -= (l4_uint32_t)((programming_delay * (l4_uint64_t)_timer_freq_kHz) / l4re_kip()->frequency_cpu);
    }
    return scheduleNext();
}

void L4_hpet::setPeriodic(bool periodic)
{
    _periodic = periodic;
}

bool L4_hpet::isPeriodic() const
{
    return _periodic;
}

l4_uint32_t L4_hpet::getFreqKHz() const
{
    return _timer_freq_kHz;
}

void L4_hpet::stop()
{
    _hpet_timer->disable_int();
}

bool L4_hpet::fire()
{
    if((l4_int32_t)((l4_uint32_t)_hpet_driver->main_counter_val() - _next) < 0)
    {
        // so we got notified prematurely ...
        // make sure the timer stays armed
        scheduleNext();
        return false;
    }
    if(_periodic)
    {
        // automatically rearm the timer
        l4_uint32_t latency = (_hpet_driver->main_counter_val() - _next) % _delta;
        _next = _hpet_driver->main_counter_val() + _delta - latency;
        scheduleNext();
    }
    return true;
}

L4::Cap<L4::Irq> & L4_hpet::irq_cap()
{
    return _lirq.lirq();
}

