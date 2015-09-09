#include "../l4_cpu.h"

class CPUSpawner : public device::IDevice
{
public:
    virtual void hypercall(HypercallPayload & hp)
    {
        GET_VM.spawn_cpu(hp.reg(0), hp.reg(1), hp.reg(2));
    }
};

